/*
 * account.c - C11 port of upstream tools/connectors/account.py
 * (gate w25_account_b4e63b).
 *
 * PoP: w25_account_b4e63b_matching_open_operation @ tools/connectors/account.py:_matching_open_operation
 * PoP: w25_account_b4e63b_plan @ tools/connectors/account.py:find_or_start_operation (ConnectionOperation/Target plan)
 * PoP: w25_account_b4e63b_constants @ tools/connectors/account.py:<module + call-site constants>
 *
 * The operation thread, its copied context, ``live.open``/``live.close`` and the
 * done event are seams: the per-name open-operation lookup arrives as a fixed
 * map. What is ported is the decision -- one open operation is reused for every
 * requested connector, and a request that would straddle two is refused.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_account_b4e63b.h"

#define AC_SESSION_PREFIX W25_ACCOUNT_B4E63B_SESSION_KEY_PREFIX
#define AC_TARGET_KIND    W25_ACCOUNT_B4E63B_TARGET_KIND
#define AC_WAIT_SECONDS   W25_ACCOUNT_B4E63B_PREPARE_WAIT_SECONDS
#define AC_SPLIT_MESSAGE  "requested connectors do not share one open operation"

/* A JSON string node or None for anything that is not a string. */
static const char *ac_name(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *ac_error(const char *kind, const char *message) {
    json_t *o = json_object();
    json_set(o, "error", json_string(kind));
    json_set(o, "message", json_string(message));
    return o;
}

/* live.find_target(name, ...) for this name: a fixed answer, None when absent. */
static json_t *ac_open_for(const json_t *open, const char *name) {
    json_t *v;
    if (!open || open->type != JSON_OBJECT || !name) return NULL;
    v = json_obj_get(open, name);
    if (!v || v->type == JSON_NULL) return NULL;
    return v;
}

static int ac_same(const json_t *a, const json_t *b) {
    char *x, *y;
    int eq;
    if (!a || !b) return a == b;
    x = json_dumps(a, 0);
    y = json_dumps(b, 0);
    eq = x && y && !strcmp(x, y);
    free(x);
    free(y);
    return eq;
}

json_t *w25_account_b4e63b_matching_open_operation(const json_t *req) {
    const json_t *names = req ? json_obj_get(req, "names") : NULL;
    const json_t *open = req ? json_obj_get(req, "open") : NULL;
    json_t *first = NULL, *o;
    size_t i, n;

    if (names && names->type == JSON_ARRAY) {
        n = json_len(names);
        /* first = next((op for op in matches if op is not None), None) */
        for (i = 0; i < n; i++) {
            first = ac_open_for(open, ac_name(json_get(names, i)));
            if (first) break;
        }
        /* any(op is not first for op in matches): a name that resolves to nothing
         * while another resolves is a mismatch, not a silent skip. */
        if (first)
            for (i = 0; i < n; i++)
                if (!ac_same(ac_open_for(open, ac_name(json_get(names, i))), first))
                    return ac_error("ValueError", AC_SPLIT_MESSAGE);
    }
    o = json_object();
    json_set(o, "found", first ? json_copy(first) : json_null());
    return o;
}

/* alias= / repair_id= are stored verbatim by the Target dataclass, including "". */
static json_t *ac_target(const json_t *name, const json_t *action, const json_t *alias,
                         const json_t *repair_id) {
    json_t *t = json_object();
    json_set(t, "name", name ? json_copy(name) : json_null());
    json_set(t, "kind", json_string(AC_TARGET_KIND));
    json_set(t, "action", action ? json_copy(action) : json_null());
    json_set(t, "alias", alias ? json_copy(alias) : json_null());
    json_set(t, "repair_id", repair_id ? json_copy(repair_id) : json_null());
    return t;
}

json_t *w25_account_b4e63b_plan(const json_t *req) {
    const json_t *names = req ? json_obj_get(req, "names") : NULL;
    const json_t *action = req ? json_obj_get(req, "action") : NULL;
    const json_t *alias = req ? json_obj_get(req, "alias") : NULL;
    const json_t *repair_id = req ? json_obj_get(req, "repair_id") : NULL;
    json_t *o = json_object(), *targets = json_array();
    size_t i, n = (names && names->type == JSON_ARRAY) ? json_len(names) : 0;

    for (i = 0; i < n; i++)
        json_append(targets, ac_target(json_get(names, i), action, alias, repair_id));
    json_set(o, "targets", targets);
    json_set(o, "session_key_prefix", json_string(AC_SESSION_PREFIX));
    return o;
}

json_t *w25_account_b4e63b_constants(void) {
    json_t *o = json_object();
    json_set(o, "prepare_wait_seconds", json_number(AC_WAIT_SECONDS));
    json_set(o, "thread_name", json_string("connector-account-operation"));
    json_set(o, "session_key_prefix", json_string(AC_SESSION_PREFIX));
    json_set(o, "session_key_hex_len", json_number(32.0));
    json_set(o, "target_kind", json_string(AC_TARGET_KIND));
    return o;
}
