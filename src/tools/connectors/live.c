/*
 * live.c - C11 port of upstream tools/connectors/live.py (gate w24_live_416722).
 *
 * PoP: w24_live_416722_effective_profile_key @ tools/connectors/live.py:_profile_key
 * PoP: w24_live_416722_key_pair @ tools/connectors/live.py:_key
 * PoP: w24_live_416722_select @ tools/connectors/live.py:current / get / get_by_op_id / find_target
 * PoP: w24_live_416722_open_result @ tools/connectors/live.py:open (+ OperationAlreadyOpen)
 * PoP: w24_live_416722_close_result @ tools/connectors/live.py:close
 * PoP: w24_live_416722_conflict_message @ tools/connectors/live.py:OperationAlreadyOpen.__init__
 * PoP: w24_live_416722_constants @ tools/connectors/live.py:<op_id length>
 *
 * The registry dict is a seam: the caller passes its contents in, this file
 * answers exactly what the live lookups answer -- key composition, the settled
 * filter (``current`` skips settled, ``get`` does not), insertion-order first
 * match for ``get_by_op_id``/``find_target``, the connector/alias/repair_id
 * triple, the already-open conflict message and the identity-guarded delete.
 * No lock, thread, logger, socket or file is touched.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_live_416722.h"

typedef struct {
    const char *session_key;
    const char *profile_key;
    const char *op_id;
    int settled;
    const json_t *targets;      /* borrowed array */
} lv_op;

static const char *lv_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

/* Python `a == b` for the optional strings these keys carry. */
static int lv_opt_eq(const json_t *a, const json_t *b) {
    int an = !a || a->type == JSON_NULL;
    int bn = !b || b->type == JSON_NULL;
    if (an || bn) return an && bn;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    return 0;
}

static size_t lv_parse_ops(const json_t *ops, lv_op **out) {
    size_t i, n;
    lv_op *list;
    *out = NULL;
    if (!ops || ops->type != JSON_ARRAY || json_len(ops) == 0) return 0;
    n = json_len(ops);
    list = (lv_op *)calloc(n, sizeof(lv_op));
    if (!list) return 0;
    for (i = 0; i < n; i++) {
        const json_t *o = json_get(ops, i);
        if (!o || o->type != JSON_OBJECT) { list[i].op_id = NULL; continue; }
        list[i].session_key = lv_str(json_obj_get(o, "session_key"));
        list[i].profile_key = lv_str(json_obj_get(o, "profile_key"));
        list[i].op_id = lv_str(json_obj_get(o, "op_id"));
        list[i].settled = json_get_bool(o, "settled", 0) ? 1 : 0;
        list[i].targets = json_obj_get(o, "targets");
    }
    *out = list;
    return n;
}

static int lv_key_index(const lv_op *ops, size_t n, const char *pk, const char *sk) {
    size_t i;
    for (i = 0; i < n; i++) {
        const char *opk = ops[i].profile_key ? ops[i].profile_key : "";
        const char *osk = ops[i].session_key ? ops[i].session_key : "";
        if (strcmp(opk, pk ? pk : "") == 0 && strcmp(osk, sk ? sk : "") == 0) return (int)i;
    }
    return -1;
}

/* target.kind == "connector" */
static int lv_eq_cstr(const json_t *v, const char *s) {
    return v && v->type == JSON_STRING && v->str_val && strcmp(v->str_val, s) == 0;
}

/* operation.target(name): first target whose name matches. */
static const json_t *lv_target(const json_t *targets, const char *name) {
    size_t i, n;
    if (!targets || targets->type != JSON_ARRAY) return NULL;
    n = json_len(targets);
    for (i = 0; i < n; i++) {
        const json_t *t = json_get(targets, i);
        const char *tn;
        if (!t || t->type != JSON_OBJECT) continue;
        tn = lv_str(json_obj_get(t, "name"));
        if (tn && name && strcmp(tn, name) == 0) return t;
    }
    return NULL;
}

static json_t *lv_ids(const lv_op *ops, size_t n) {
    json_t *arr = json_array();
    size_t i;
    for (i = 0; i < n; i++)
        json_append(arr, json_string(ops[i].op_id ? ops[i].op_id : ""));
    return arr;
}

/* Python repr() of a str, as the conflict message embeds it. */
static char *lv_repr(const char *s) {
    size_t i, out = 0, n;
    char q;
    char *buf;
    if (!s) s = "";
    q = (strchr(s, '\'') && !strchr(s, '"')) ? '"' : '\'';
    n = strlen(s);
    buf = (char *)malloc(n * 4 + 4);
    if (!buf) return NULL;
    buf[out++] = q;
    for (i = 0; i < n; i++) {
        char c = s[i];
        if (c == q || c == '\\') { buf[out++] = '\\'; buf[out++] = c; }
        else if (c == '\n') { buf[out++] = '\\'; buf[out++] = 'n'; }
        else if (c == '\r') { buf[out++] = '\\'; buf[out++] = 'r'; }
        else if (c == '\t') { buf[out++] = '\\'; buf[out++] = 't'; }
        else buf[out++] = c;
    }
    buf[out++] = q;
    buf[out] = '\0';
    return buf;
}

json_t *w24_live_416722_effective_profile_key(const json_t *profile_home,
                                              const json_t *process_key) {
    const char *home = lv_str(profile_home);
    if (home && home[0]) return json_string(home);           /* `profile_home or ...` */
    return json_string(lv_str(process_key) ? lv_str(process_key) : "");
}

json_t *w24_live_416722_key_pair(const json_t *session_key, const json_t *profile_home,
                                 const json_t *process_key) {
    json_t *arr = json_array();
    json_t *pk = w24_live_416722_effective_profile_key(profile_home, process_key);
    json_append(arr, pk);
    json_append(arr, json_string(lv_str(session_key) ? lv_str(session_key) : ""));
    return arr;
}

json_t *w24_live_416722_select(const json_t *ops, const json_t *query) {
    lv_op *list = NULL;
    size_t n = lv_parse_ops(ops, &list), i;
    const char *op = lv_str(json_obj_get(query, "op"));
    const char *sk = lv_str(json_obj_get(query, "session_key"));
    const char *qid = lv_str(json_obj_get(query, "op_id"));
    const char *name = lv_str(json_obj_get(query, "name"));
    const char *pk;
    json_t *result = json_null();
    json_t *pkj;

    if (!n) { free(list); return result; }
    pkj = w24_live_416722_effective_profile_key(json_obj_get(query, "profile_home"),
                                                json_obj_get(query, "process_key"));
    pk = lv_str(pkj);

    if (op && strcmp(op, "current") == 0) {
        int idx = lv_key_index(list, n, pk, sk);
        if (idx >= 0 && !list[idx].settled) result = json_number((double)idx);
    } else if (op && strcmp(op, "get") == 0) {
        int idx = lv_key_index(list, n, pk, sk);
        if (idx >= 0 && qid && list[idx].op_id && strcmp(list[idx].op_id, qid) == 0)
            result = json_number((double)idx);
    } else if (op && strcmp(op, "get_by_op_id") == 0) {
        for (i = 0; i < n; i++) {
            const char *opk = list[i].profile_key ? list[i].profile_key : "";
            if (strcmp(opk, pk) != 0) continue;
            if (!qid || !list[i].op_id || strcmp(list[i].op_id, qid) != 0) continue;
            if (list[i].settled) continue;
            result = json_number((double)i);
            break;
        }
    } else if (op && strcmp(op, "find_target") == 0) {
        for (i = 0; i < n; i++) {
            const char *opk = list[i].profile_key ? list[i].profile_key : "";
            const json_t *t;
            if (strcmp(opk, pk) != 0 || list[i].settled) continue;
            t = lv_target(list[i].targets, name);
            if (!t) continue;
            if (!lv_eq_cstr(json_obj_get(t, "kind"), "connector")) continue;
            if (!lv_opt_eq(json_obj_get(t, "alias"), json_obj_get(query, "alias"))) continue;
            if (!lv_opt_eq(json_obj_get(t, "repair_id"), json_obj_get(query, "repair_id"))) continue;
            result = json_number((double)i);
            break;
        }
    }
    json_free(pkj);
    free(list);
    return result;
}

json_t *w24_live_416722_conflict_message(const json_t *session_key, const json_t *op_id) {
    char *repr = lv_repr(lv_str(session_key) ? lv_str(session_key) : "");
    const char *id = lv_str(op_id) ? lv_str(op_id) : "";
    size_t need;
    char *msg;
    json_t *out;
    if (!repr) return json_null();
    need = strlen(repr) + strlen(id) + 64;
    msg = (char *)malloc(need);
    if (!msg) { free(repr); return json_null(); }
    snprintf(msg, need, "session %s already has operation %s open", repr, id);
    out = json_string(msg);
    free(msg);
    free(repr);
    return out;
}

json_t *w24_live_416722_open_result(const json_t *ops, const json_t *query) {
    lv_op *list = NULL;
    size_t n = lv_parse_ops(ops, &list);
    json_t *pkj = w24_live_416722_effective_profile_key(
        json_obj_get(query, "active_profile_key"), json_obj_get(query, "process_key"));
    const char *pk = lv_str(pkj);
    const char *sk = lv_str(json_obj_get(query, "session_key"));
    const char *qid = lv_str(json_obj_get(query, "op_id"));
    int idx = lv_key_index(list, n, pk, sk);
    json_t *out = json_object();
    json_t *ids;

    if (idx >= 0 && !list[idx].settled) {
        json_set(out, "raised", json_bool(1));
        json_set(out, "message", w24_live_416722_conflict_message(
            json_string(sk ? sk : ""), json_string(list[idx].op_id ? list[idx].op_id : "")));
        json_set(out, "stored", json_bool(0));
        json_set(out, "op_ids", lv_ids(list, n));
    } else {
        lv_op *grown;
        if (idx >= 0) {
            list[idx].op_id = qid;                     /* replace a settled op in place */
        } else {
            grown = (lv_op *)realloc(list, (n + 1) * sizeof(lv_op));
            if (!grown) { json_free(pkj); free(list); return json_null(); }
            list = grown;
            list[n].session_key = sk;
            list[n].profile_key = pk;
            list[n].op_id = qid;
            list[n].settled = 0;
            list[n].targets = NULL;
            n++;
        }
        ids = lv_ids(list, n);
        json_set(out, "raised", json_bool(0));
        json_set(out, "message", json_null());
        json_set(out, "stored", json_bool(1));
        json_set(out, "op_ids", ids);
    }
    json_free(pkj);
    free(list);
    return out;
}

json_t *w24_live_416722_close_result(const json_t *ops, const json_t *query) {
    lv_op *list = NULL;
    size_t n = lv_parse_ops(ops, &list), i, w = 0;
    const char *pk = lv_str(json_obj_get(query, "profile_key"));
    const char *sk = lv_str(json_obj_get(query, "session_key"));
    int idx = lv_key_index(list, n, pk, sk);
    int via_registry = json_get_bool(query, "via_registry", 1) ? 1 : 0;   /* `_open.get(k) is op` */
    json_t *out = json_object();

    if (!via_registry) idx = -1;                 /* a detached object deletes nothing */
    json_set(out, "removed", json_bool(idx >= 0));
    if (idx >= 0) {
        for (i = 0; i < n; i++)
            if ((int)i != idx) list[w++] = list[i];
        n = w;
    }
    json_set(out, "op_ids", lv_ids(list, n));
    free(list);
    return out;
}

json_t *w24_live_416722_constants(void) {
    json_t *o = json_object();
    json_set(o, "op_id_length", json_number(12.0));
    return o;
}
