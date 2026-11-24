/*
 * delegate_tool_prefix_gate.c - C11 port of upstream
 * tools/delegate_tool_prefix_gate.py (gate w25_delegateto_a1fd91).
 *
 * PoP: w25_delegateto_a1fd91_route_key @ tools/delegate_tool_prefix_gate.py:_route_key
 * PoP: w25_delegateto_a1fd91_caches_prompts @ tools/delegate_tool_prefix_gate.py:_caches_prompts
 * PoP: w25_delegateto_a1fd91_first_response_started @ tools/delegate_tool_prefix_gate.py:_first_response_started
 * PoP: w25_delegateto_a1fd91_for_children @ tools/delegate_tool_prefix_gate.py:PrefixCacheGate.for_children
 * PoP: w25_delegateto_a1fd91_follows @ tools/delegate_tool_prefix_gate.py:PrefixCacheGate.follows
 * PoP: w25_delegateto_a1fd91_constants @ tools/delegate_tool_prefix_gate.py:<module constants>
 *
 * The gate's wait is a seam: threading.Event, time.monotonic and the poll loop
 * never appear here.  What is ported is what the gate decides before it ever
 * sleeps -- which children share the leader's route, whether the batch is
 * worth gating, and when the leader's first response has started.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_delegateto_a1fd91.h"

static const char *const ROUTE_ATTRS[W25_DELEGATETO_A1FD91_ROUTE_ATTRS] = {
    "provider", "base_url", "model", "api_mode",
};

static int dg_truthy(const json_t *v) {
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

/* Numbers only: an int, a float or a bool (None is none of those). */
static int dg_number(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* Python '==' over route-key elements: a missing key is None. */
static int dg_equal(const json_t *a, const json_t *b) {
    double x, y;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if (dg_number(a, &x) && dg_number(b, &y)) return x == y;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type == JSON_ARRAY && b->type == JSON_ARRAY) {
        size_t i, n = json_len(a);
        if (n != json_len(b)) return 0;
        for (i = 0; i < n; i++)
            if (!dg_equal(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    }
    if (a->type == JSON_OBJECT && b->type == JSON_OBJECT) {
        size_t i, n = json_object_size(a);
        if (n != json_object_size(b)) return 0;
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(a, i);
            if (!k || !dg_equal(json_object_get_at(a, i), json_obj_get(b, k))) return 0;
        }
        return 1;
    }
    return 0;
}

json_t *w25_delegateto_a1fd91_route_key(const json_t *child) {
    json_t *out = json_array();
    size_t i;
    for (i = 0; i < W25_DELEGATETO_A1FD91_ROUTE_ATTRS; i++) {
        const json_t *v = json_obj_get(child, ROUTE_ATTRS[i]);
        json_append(out, v ? json_copy(v) : json_null());
    }
    return out;
}

json_t *w25_delegateto_a1fd91_caches_prompts(const json_t *child) {
    const json_t *v = json_obj_get(child, "_use_prompt_caching");
    /* getattr(..., False) is True: only the bool True passes that identity. */
    return json_bool(v && v->type == JSON_BOOL && v->bool_val ? true : false);
}

json_t *w25_delegateto_a1fd91_first_response_started(const json_t *child) {
    const json_t *calls = json_obj_get(child, "session_api_calls");
    const json_t *desc = json_obj_get(child, "_last_activity_desc");
    double n = 0.0;

    if (!dg_number(calls, &n)) n = 0.0;   /* (x or 0): None and non-numbers alike */
    if (n >= 1.0) return json_bool(true);
    if (desc && desc->type == JSON_STRING && desc->str_val &&
        !strcmp(desc->str_val, W25_DELEGATETO_A1FD91_FIRST_CHUNK))
        return json_bool(true);
    return json_bool(false);
}

json_t *w25_delegateto_a1fd91_for_children(const json_t *children, const json_t *max_wait_s) {
    json_t *out = json_object();
    double wait = 0.0;
    int gate = 0;

    if (dg_number(max_wait_s, &wait)) {
        if (wait > 0.0 && children && children->type == JSON_ARRAY && json_len(children) >= 2) {
            const json_t *leader = json_get(children, 0);
            if (leader && json_obj_get(leader, "_use_prompt_caching") &&
                json_obj_get(leader, "_use_prompt_caching")->type == JSON_BOOL &&
                json_obj_get(leader, "_use_prompt_caching")->bool_val)
                gate = 1;
        }
    }
    json_set(out, "gate", json_bool(gate ? true : false));
    return out;
}

json_t *w25_delegateto_a1fd91_follows(const json_t *request) {
    const json_t *leader = json_obj_get(request, "leader");
    const json_t *child = json_obj_get(request, "child");
    int same = dg_truthy(json_obj_get(request, "same"));
    int follows = 0;
    json_t *out = json_object();

    if (!same && child) {
        const json_t *pc = json_obj_get(child, "_use_prompt_caching");
        if (pc && pc->type == JSON_BOOL && pc->bool_val) {
            json_t *lk = w25_delegateto_a1fd91_route_key(leader);
            json_t *ck = w25_delegateto_a1fd91_route_key(child);
            follows = dg_equal(lk, ck);
            json_free(lk);
            json_free(ck);
        }
    }
    json_set(out, "follows", json_bool(follows ? true : false));
    return out;
}

json_t *w25_delegateto_a1fd91_constants(void) {
    json_t *out = json_object();
    json_set(out, "first_chunk_activity", json_string(W25_DELEGATETO_A1FD91_FIRST_CHUNK));
    json_set(out, "poll_seconds", json_number(0.05));
    return out;
}
