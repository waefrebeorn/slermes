/*
 * session_vault_code_card.c -- real C11 port of the request/author decision
 * half of upstream Hermes gateway/session_vault_code_card.py.
 *
 * PoP: w20_sessionvau_bfb027_code_request @ gateway/session_vault_code_card.py:_CodeRequest
 * PoP: w20_sessionvau_bfb027_code_request @ gateway/session_vault_code_card.py:_CodeRequest.submit
 * PoP: w20_sessionvau_bfb027_code_request @ gateway/session_vault_code_card.py:_CodeRequest.close
 * PoP: w20_sessionvau_bfb027_code_request @ gateway/session_vault_code_card.py:_CodeRequest.cancel
 * PoP: w20_sessionvau_bfb027_code_request @ gateway/session_vault_code_card.py:_CodeRequest.wait
 * PoP: w20_sessionvau_bfb027_user_id @ gateway/session_vault_code_card.py:code_card_scope
 * PoP: w20_sessionvau_bfb027_scope_gate @ gateway/session_vault_code_card.py:_code_card_adapter
 * PoP: w20_sessionvau_bfb027_scope_gate @ gateway/session_vault_code_card.py:code_card_scope
 * PoP: w20_sessionvau_bfb027_wait_slice @ gateway/session_vault_code_card.py:_CodeRequest.wait
 *
 * The scope body itself (unlock.set_code_prompt_callback, gateway.run's
 * _approval_send_outcome, turn._schedule, the adapter modal) needs a live turn,
 * a live unlock registry and a live gateway, so it stays upstream. What is
 * ported is every decision that scope makes before it touches them: whether the
 * card may be installed at all, which author it binds to, and how one pending
 * code settles.
 */
#include "hermes_json.h"
#include "noc_w20_sessionvau_bfb027.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *sv027_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* Python truthiness for a decoded scalar (absent == None). */
static int sv027_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return v->str_val && v->str_val[0];
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0;
    }
    return 0;
}

static int sv027_is_py_int(double d) {
    if (!isfinite(d)) return 0;
    if (floor(d) != d) return 0;
    return d >= -9.007199254740992e15 && d <= 9.007199254740992e15;
}

/* --- _CodeRequest ---------------------------------------------------------- */

typedef struct {
    int settled;      /* threading.Event.is_set() */
    char *value;
} sv027_req;

static void sv027_req_free(sv027_req *r) {
    free(r->value);
    r->value = NULL;
    r->settled = 0;
}

/* str.strip() -- leading and trailing whitespace, both sides. */
static char *sv027_strip(const char *s) {
    size_t a = 0, b;
    if (!s) return sv027_dup("");
    b = strlen(s);
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' || s[a] == '\r' ||
                     s[a] == '\f' || s[a] == '\v')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\n' ||
                     s[b - 1] == '\r' || s[b - 1] == '\f' || s[b - 1] == '\v')) b--;
    {
        char *out = (char *)malloc(b - a + 1);
        if (!out) return NULL;
        memcpy(out, s + a, b - a);
        out[b - a] = '\0';
        return out;
    }
}

/* submit: settled once, by the author; empty / already-settled is refused. */
static int sv027_submit(sv027_req *r, const char *code) {
    char *stripped = sv027_strip(code ? code : "");
    int ok;
    if (!stripped) return 0;
    ok = !(r->settled || !stripped[0]);
    if (ok) {
        free(r->value);
        r->value = sv027_dup(stripped);
        r->settled = 1;
    }
    free(stripped);
    return ok;
}

json_t *w20_sessionvau_bfb027_code_request(const json_t *ops) {
    json_t *out = json_array();
    sv027_req req;
    size_t i, n;
    req.settled = 0;
    req.value = sv027_dup("");
    if (!ops || ops->type != JSON_ARRAY) {
        sv027_req_free(&req);
        return out;
    }
    n = ops->c.count;
    for (i = 0; i < n; i++) {
        json_t *op = ops->c.items[i];
        const char *name;
        json_t *rec = json_object();
        if (!op || op->type != JSON_OBJECT) {
            json_set(rec, "kind", json_string("skip"));
            json_append(out, rec);
            continue;
        }
        name = json_get_str(op, "op", "");
        if (!strcmp(name, "submit")) {
            json_set(rec, "kind", json_string("submit"));
            json_set(rec, "accepted", json_bool(sv027_submit(&req, json_get_str(op, "code", ""))));
        } else if (!strcmp(name, "close")) {
            req.settled = 1;                       /* value is kept */
            json_set(rec, "kind", json_string("void"));
        } else if (!strcmp(name, "cancel")) {
            free(req.value);
            req.value = sv027_dup("");             /* the turn ended: drop the code */
            req.settled = 1;
            json_set(rec, "kind", json_string("void"));
        } else if (!strcmp(name, "wait")) {
            int cancelled = json_get_bool(op, "cancelled", 0) ? 1 : 0;
            /* The wait ends settled: on timeout it closes (#66), and a /stop
             * that raced the submit still wins. */
            req.settled = 1;
            json_set(rec, "kind", json_string("wait"));
            json_set(rec, "code", json_string(cancelled ? "" : (req.value ? req.value : "")));
            json_set(rec, "settled", json_bool(1));
        } else if (!strcmp(name, "state")) {
            json_set(rec, "kind", json_string("state"));
            json_set(rec, "settled", json_bool(req.settled));
            json_set(rec, "value", json_string(req.value ? req.value : ""));
        } else {
            json_set(rec, "kind", json_string("skip"));
        }
        json_append(out, rec);
    }
    sv027_req_free(&req);
    return out;
}

/* --- the turn's author ----------------------------------------------------- */

char *w20_sessionvau_bfb027_user_id(const json_t *source) {
    const json_t *v;
    char buf[64];
    if (!source || source->type != JSON_OBJECT) return sv027_dup("");
    v = json_obj_get(source, "user_id");            /* getattr(source, "user_id", "") */
    if (!sv027_truthy(v)) return sv027_dup("");     /* `or ""` */
    switch (v->type) {
    case JSON_STRING: return sv027_dup(v->str_val);
    case JSON_BOOL:   return sv027_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (sv027_is_py_int(v->num_val)) {
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        } else {
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        }
        return sv027_dup(buf);
    default:
        return sv027_dup("");                       /* str(list/dict) is not an author id */
    }
}

json_t *w20_sessionvau_bfb027_scope_gate(const json_t *turn) {
    json_t *out = json_object();
    int has_adapter = turn ? (json_get_bool(turn, "has_adapter", 0) ? 1 : 0) : 0;
    /* `getattr(type(adapter), "send_vault_code_prompt", None) is not None`:
     * adapter_kind is "with" (defined method), "none" (attribute is None) or
     * "without" (no such attribute). */
    const char *kind = turn ? json_get_str(turn, "adapter_kind", "without") : "without";
    int adapter = has_adapter && !strcmp(kind, "with");
    char *user_id = turn ? w20_sessionvau_bfb027_user_id(json_obj_get(turn, "source"))
                         : sv027_dup("");
    json_set(out, "adapter", json_bool(adapter));
    json_set(out, "user_id", json_string(user_id ? user_id : ""));
    /* adapter is None or not user_id -> yield (nothing installed) */
    json_set(out, "gate", json_string(adapter && user_id && user_id[0] ? "install" : "passthrough"));
    free(user_id);
    return out;
}

double w20_sessionvau_bfb027_wait_slice(double remaining) {
    double v = remaining;
    if (v < 0.0) v = 0.0;
    if (v > 1.0) v = 1.0;
    return v;
}
