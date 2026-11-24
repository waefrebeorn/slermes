/*
 * kanban_pr_acceptance_store.c - C11 port of upstream
 * hermes_cli/kanban_pr_acceptance_store.py (gate w24_kanbanprac_15da5e).
 *
 * PoP: w24_kanbanprac_15da5e_decide @ hermes_cli/kanban_pr_acceptance_store.py:prepare_acceptance
 * PoP: w24_kanbanprac_15da5e_pr_match @ hermes_cli/kanban_pr_acceptance_store.py:_PR.fullmatch
 * PoP: w24_kanbanprac_15da5e_contract_ok @ hermes_cli/kanban_pr_acceptance.py:validate_contract
 * PoP: w24_kanbanprac_15da5e_record_outcome @ hermes_cli/kanban_pr_acceptance_store.py:record_acceptance
 * PoP: w24_kanbanprac_15da5e_constants @ hermes_cli/kanban_pr_acceptance.py:_PR
 *
 * What is ported is the decision chain itself, in the source's order: no row
 * refuses; a falsy or "local-only" contract skips; a status outside the open
 * set or a run-id mismatch refuses; a published PR URL whose repo is the
 * contract binds the contract to that URL (publication binds once, a retry
 * cannot swap in a green sibling); otherwise the contract stands.  The
 * connections, the write transaction, _snapshot's re-read, the event append,
 * the tasks UPDATE and the `gh` acceptance run are seams -- they arrive as the
 * snapshot row, the assignee and the receipt dict, and the values they decide
 * are exactly what is compared.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_kanbanprac_15da5e.h"

/* The open-status set of prepare_acceptance. */
#define KB_OPEN_STATUSES 4
static const char *const KB_STATUSES[KB_OPEN_STATUSES] = {"running", "ready", "blocked", "review"};

/* _PR / _REPO patterns (source text, kept verbatim for the constants table). */
#define KB_PR_PATTERN "https://github\\.com/([A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+)/pull/([1-9][0-9]*)"
#define KB_REPO_PATTERN "[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+"
/* re.UNICODE -- the only flag a str pattern carries in Python 3. */
#define KB_PR_FLAGS 32
#define KB_LOCAL_ONLY "local-only"
#define KB_CONTRACT_ERROR \
    "completion_contract must be local-only, OWNER/REPO, or an exact GitHub PR URL"
#define KB_EVENT "pr_acceptance"
#define KB_URL_PREFIX "https://github.com/"

/* --- Python value helpers ------------------------------------------------- */
typedef struct { char *b; size_t n, cap; int oom; } kb_buf;

static void kb_bput(kb_buf *s, const char *t, size_t len) {
    if (s->oom) return;
    if (s->n + len + 1 > s->cap) {
        size_t cap = s->cap ? s->cap : 64;
        while (cap < s->n + len + 1) cap *= 2;
        char *nb = (char *)realloc(s->b, cap);
        if (!nb) { s->oom = 1; return; }
        s->b = nb;
        s->cap = cap;
    }
    memcpy(s->b + s->n, t, len);
    s->n += len;
    s->b[s->n] = '\0';
}

static void kb_bputs(kb_buf *s, const char *t) { kb_bput(s, t, strlen(t)); }

static char *kb_dup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Python truthiness. */
static int kb_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

static int kb_is_str(const json_t *v) { return v && v->type == JSON_STRING; }

static int kb_number(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* Python '==' over the value shapes a snapshot field can hold. */
static int kb_equal(const json_t *a, const json_t *b) {
    double x = 0.0, y = 0.0;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if (kb_number(a, &x) && kb_number(b, &y)) return x == y;
    if (kb_is_str(a) && kb_is_str(b))
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    return 0;
}

static void kb_num_text(double v, char *out, size_t cap) {
    char tmp[64];
    int p;
    if (v == (double)(long long)v && v < 1e16 && v > -1e16) { snprintf(out, cap, "%.0f", v); return; }
    for (p = 1; p <= 17; p++) {
        snprintf(tmp, sizeof(tmp), "%.*g", p, v);
        if (strtod(tmp, NULL) == v) { snprintf(out, cap, "%s", tmp); return; }
    }
    snprintf(out, cap, "%.17g", v);
}

static void kb_repr(kb_buf *s, const json_t *v);

static void kb_repr_str(kb_buf *s, const char *t) {
    kb_bput(s, "'", 1);
    for (const char *p = t; *p; p++) {
        if (*p == '\'') kb_bputs(s, "\\'");
        else if (*p == '\\') kb_bputs(s, "\\\\");
        else if (*p == '\n') kb_bputs(s, "\\n");
        else if (*p == '\t') kb_bputs(s, "\\t");
        else kb_bput(s, p, 1);
    }
    kb_bput(s, "'", 1);
}

static void kb_repr(kb_buf *s, const json_t *v) {
    size_t i, n;
    char num[64];
    if (!v) { kb_bputs(s, "None"); return; }
    switch (v->type) {
    case JSON_NULL: kb_bputs(s, "None"); return;
    case JSON_BOOL: kb_bputs(s, v->bool_val ? "True" : "False"); return;
    case JSON_NUMBER: kb_num_text(v->num_val, num, sizeof(num)); kb_bputs(s, num); return;
    case JSON_STRING: kb_repr_str(s, v->str_val ? v->str_val : ""); return;
    case JSON_ARRAY:
        kb_bput(s, "[", 1);
        n = json_len(v);
        for (i = 0; i < n; i++) {
            if (i) kb_bputs(s, ", ");
            kb_repr(s, json_get(v, i));
        }
        kb_bput(s, "]", 1);
        return;
    case JSON_OBJECT:
        kb_bput(s, "{", 1);
        n = json_object_size(v);
        for (i = 0; i < n; i++) {
            if (i) kb_bputs(s, ", ");
            kb_repr_str(s, json_object_get_key_at(v, i));
            kb_bputs(s, ": ");
            kb_repr(s, json_object_get_at(v, i));
        }
        kb_bput(s, "}", 1);
        return;
    }
}

/* str(v). Caller frees. */
static char *kb_pystr(const json_t *v) {
    kb_buf s = {NULL, 0, 0, 0};
    if (v && v->type == JSON_STRING) return kb_dup(v->str_val ? v->str_val : "");
    kb_repr(&s, v);
    if (s.oom) { free(s.b); return NULL; }
    if (!s.b) return kb_dup("");
    return s.b;
}

static json_t *kb_error(const char *name) {
    json_t *o = json_object();
    json_set(o, "error", json_string(name));
    return o;
}

static int kb_has(const json_t *o, const char *key) {
    size_t i, n;
    if (!o || o->type != JSON_OBJECT) return 0;
    n = json_object_size(o);
    for (i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(o, i);
        if (k && !strcmp(k, key)) return 1;
    }
    return 0;
}

/* A str field of the snapshot row; NULL when the row has no such field. */
static const json_t *kb_row(const json_t *snapshot, const char *key) {
    if (!snapshot || snapshot->type != JSON_OBJECT) return NULL;
    return json_obj_get(snapshot, key);
}

json_t *w24_kanbanprac_15da5e_decide(const json_t *snapshot, const json_t *expected_run_id,
                                     const json_t *metadata, const json_t *assignee) {
    const json_t *run_id, *status, *contract, *published_pr;
    json_t *out = json_object();
    size_t i;
    int open = 0;

    json_set(out, "contract", json_null());
    json_set(out, "published_pr", json_null());
    json_set(out, "assignee", json_null());

    /* snapshot is None -> prepare_acceptance returns False before unpacking. */
    if (!snapshot || snapshot->type != JSON_OBJECT) {
        json_set(out, "outcome", json_string("missing"));
        return out;
    }
    run_id = kb_row(snapshot, "run_id");
    status = kb_row(snapshot, "status");
    contract = kb_row(snapshot, "contract");

    /* if not contract or contract == "local-only": return None */
    if (!kb_truthy(contract) ||
        (kb_is_str(contract) && !strcmp(contract->str_val ? contract->str_val : "", KB_LOCAL_ONLY))) {
        json_set(out, "outcome", json_string("skip"));
        json_set(out, "contract", contract ? json_copy(contract) : json_null());
        return out;
    }
    json_set(out, "contract", json_copy(contract));

    /* status not in {running, ready, blocked, review} -> False */
    for (i = 0; i < KB_OPEN_STATUSES; i++)
        if (kb_is_str(status) && !strcmp(status->str_val ? status->str_val : "", KB_STATUSES[i])) { open = 1; break; }
    if (!open) {
        json_set(out, "outcome", json_string("refuse"));
        return out;
    }
    /* expected_run_id is not None and run_id != expected_run_id -> False */
    if (expected_run_id && expected_run_id->type != JSON_NULL && !kb_equal(run_id, expected_run_id)) {
        json_set(out, "outcome", json_string("refuse"));
        return out;
    }

    published_pr = (metadata && metadata->type == JSON_OBJECT) ? json_obj_get(metadata, "published_pr") : NULL;

    /* if match and contract == match[1]: publication binds once. */
    if (kb_is_str(published_pr)) {
        json_t *repo = w24_kanbanprac_15da5e_pr_match(published_pr);
        if (repo && kb_is_str(repo) && kb_is_str(contract) &&
            !strcmp(repo->str_val, contract->str_val)) {
            /* snapshot = (run_id, status, published_pr); contract = published_pr */
            json_set(out, "contract", json_copy(published_pr));
        }
        json_free(repo);
    }
    json_set(out, "outcome", json_string("prepare"));
    json_set(out, "published_pr", published_pr ? json_copy(published_pr) : json_null());
    json_set(out, "assignee", assignee ? json_copy(assignee) : json_null());
    return out;
}

/* [A-Za-z0-9_.-]+ -- the character class of both repo halves. */
static int kb_name_char(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == '_' || c == '.' || c == '-';
}

static int kb_fullmatch_repo(const char *s, size_t *consumed) {
    size_t i = 0, owner, name;
    owner = 0;
    while (s[i] && kb_name_char(s[i])) { i++; owner++; }
    if (owner == 0 || s[i] != '/') return 0;
    i++;
    name = 0;
    while (s[i] && kb_name_char(s[i])) { i++; name++; }
    if (name == 0) return 0;
    *consumed = i;
    return 1;
}

json_t *w24_kanbanprac_15da5e_pr_match(const json_t *url) {
    const char *s, *p;
    size_t consumed;
    json_t *out;

    if (!kb_is_str(url)) return json_null();
    s = url->str_val ? url->str_val : "";
    if (strncmp(s, KB_URL_PREFIX, sizeof(KB_URL_PREFIX) - 1) != 0) return json_null();
    p = s + sizeof(KB_URL_PREFIX) - 1;
    if (!kb_fullmatch_repo(p, &consumed)) return json_null();
    /* literal "/pull/" then [1-9][0-9]* then end of string */
    if (strncmp(p + consumed, "/pull/", 6) != 0) return json_null();
    p += consumed + 6;
    if (*p < '1' || *p > '9') return json_null();
    p++;
    while (*p >= '0' && *p <= '9') p++;
    if (*p != '\0') return json_null();

    out = json_string("");
    if (out) {
        char *buf = (char *)malloc(consumed + 1);
        if (!buf) { json_free(out); return json_null(); }
        memcpy(buf, s + sizeof(KB_URL_PREFIX) - 1, consumed);
        buf[consumed] = '\0';
        json_free(out);
        out = json_string(buf);
        free(buf);
    }
    return out;
}

json_t *w24_kanbanprac_15da5e_contract_ok(const json_t *value) {
    json_t *out = json_object();
    const char *s;

    if (!value || value->type == JSON_NULL ||
        (kb_is_str(value) && !strcmp(value->str_val ? value->str_val : "", KB_LOCAL_ONLY))) {
        json_set(out, "ok", json_bool(1));
        json_set(out, "value", json_string(KB_LOCAL_ONLY));
        return out;
    }
    s = kb_is_str(value) ? (value->str_val ? value->str_val : "") : NULL;
    if (s) {
        size_t consumed = 0;
        /* _REPO.fullmatch(value) or _PR.fullmatch(value) */
        if (kb_fullmatch_repo(s, &consumed) && s[consumed] == '\0') {
            json_set(out, "ok", json_bool(1));
            json_set(out, "value", json_copy(value));
            return out;
        }
        {
            json_t *m = w24_kanbanprac_15da5e_pr_match(value);
            int ok = m && m->type == JSON_STRING;
            json_free(m);
            if (ok) {
                json_set(out, "ok", json_bool(1));
                json_set(out, "value", json_copy(value));
                return out;
            }
        }
    }
    json_set(out, "ok", json_bool(0));
    json_set(out, "error", json_string(KB_CONTRACT_ERROR));
    return out;
}

json_t *w24_kanbanprac_15da5e_record_outcome(const json_t *snapshot_matches,
                                             const json_t *receipt) {
    json_t *out;
    const json_t *ok, *cls, *recovery, *detail;
    json_t *empty = NULL;

    /* if _snapshot(conn, task_id) != snapshot: return False */
    if (!kb_truthy(snapshot_matches)) {
        out = json_object();
        json_set(out, "result", json_bool(0));
        json_set(out, "detail", json_null());
        return out;
    }
    /* receipt["ok"] / receipt["classification"] / receipt["recovery"] */
    if (!kb_has(receipt, "ok")) return kb_error("KeyError");
    ok = json_obj_get(receipt, "ok");
    if (kb_truthy(ok)) {
        out = json_object();
        json_set(out, "result", ok ? json_copy(ok) : json_null());
        json_set(out, "detail", json_null());
        return out;
    }
    if (!kb_has(receipt, "classification") || !kb_has(receipt, "recovery")) return kb_error("KeyError");
    cls = json_obj_get(receipt, "classification");
    recovery = json_obj_get(receipt, "recovery");
    detail = json_obj_get(receipt, "detail");
    if (!detail) { empty = json_string(""); detail = empty; }   /* receipt.get('detail', '') */

    {
        char *c = kb_pystr(cls), *d = kb_pystr(detail), *r = kb_pystr(recovery);
        size_t need = strlen(c ? c : "") + strlen(d ? d : "") + strlen(r ? r : "") + 32;
        char *text = (char *)malloc(need);
        if (text)
            snprintf(text, need, "PR acceptance %s: %s %s", c ? c : "", d ? d : "", r ? r : "");
        out = json_object();
        json_set(out, "result", ok ? json_copy(ok) : json_null());
        json_set(out, "detail", json_string(text ? text : ""));
        free(text);
        free(c); free(d); free(r);
        if (empty) json_free(empty);
    }
    return out;
}

json_t *w24_kanbanprac_15da5e_constants(void) {
    json_t *o = json_object();
    json_set(o, "pr_pattern", json_string(KB_PR_PATTERN));
    json_set(o, "pr_pattern_flags", json_number(KB_PR_FLAGS));
    json_set(o, "repo_pattern", json_string(KB_REPO_PATTERN));
    return o;
}
