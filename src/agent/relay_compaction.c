/*
 * relay_compaction.c — C port of upstream Hermes agent/relay_compaction.py.
 *
 * PoP: w8_relaycompac_aac863_turn_target @ agent/relay_compaction.py:_turn_target
 * PoP: w8_relaycompac_aac863_session_target @ agent/relay_compaction.py:_session_target
 * PoP: w8_relaycompac_aac863_resolve_target @ agent/relay_compaction.py:_resolve_target
 * PoP: w8_relaycompac_aac863_target_available @ agent/relay_compaction.py:compaction_target_available
 * PoP: w8_relaycompac_aac863_emit_mark @ agent/relay_compaction.py:emit_compaction_mark
 *
 * relay_runtime collaborators (active_turn, HOST_REGISTRY, instrumentation
 * flag, run_in_session outcome) are injected from fixtures.
 */
#include "noc_w8_relaycompac_aac863.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: return v->c.count > 0 ? 1 : 0;
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default: return 0;
    }
}
static json_t *target_or_null(const json_t *v) {
    if (!v || v->type != JSON_ARRAY) return json_null();
    return json_copy(v);
}
static const char *sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

json_t *w8_relaycompac_aac863_turn_target(const struct json_t *args) {
    if (!truthy(json_obj_get(args, "active")) ||
        !truthy(json_obj_get(args, "live_runtime")))
        return json_null();
    const json_t *th = json_obj_get(args, "turn_handle");
    const json_t *sh = json_obj_get(args, "session_handle");
    const json_t *handle = truthy(th) ? th : sh;
    json_t *r = json_array();
    json_append(r, json_copy(json_obj_get(args, "host")));
    json_append(r, json_copy(json_obj_get(args, "session")));
    json_append(r, handle ? json_copy(handle) : json_null());
    return r;
}

json_t *w8_relaycompac_aac863_session_target(const struct json_t *args) {
    if (!truthy(json_obj_get(args, "host_runtime")) ||
        !truthy(json_obj_get(args, "session_exists")))
        return json_null();
    const json_t *handle = json_obj_get(args, "session_handle");
    if (!handle || handle->type == JSON_NULL) return json_null();
    json_t *r = json_array();
    json_append(r, json_copy(json_obj_get(args, "host")));
    json_append(r, json_copy(json_obj_get(args, "session")));
    json_append(r, json_copy(handle));
    return r;
}

json_t *w8_relaycompac_aac863_resolve_target(const struct json_t *args) {
    if (!truthy(json_obj_get(args, "enabled"))) return json_null();
    const json_t *tt = json_obj_get(args, "turn_target");
    const json_t *st = json_obj_get(args, "session_target");
    const json_t *target = truthy(tt) ? tt : (truthy(st) ? st : NULL);
    if (!target && truthy(json_obj_get(args, "turn_session_id"))) {
        const json_t *tt2 = json_obj_get(args, "turn_turn_target");
        if (truthy(tt2)) target = tt2;
    }
    return target ? json_copy(target) : json_null();
}

int w8_relaycompac_aac863_target_available(const struct json_t *args) {
    if (!truthy(json_obj_get(args, "session_id"))) return 0;
    return truthy(json_obj_get(args, "target")) ? 1 : 0;
}

json_t *w8_relaycompac_aac863_emit_mark(const struct json_t *args) {
    if (!truthy(json_obj_get(args, "session_id")) ||
        !truthy(json_obj_get(args, "target")) ||
        truthy(json_obj_get(args, "stalled")))
        return json_bool(false);
    const char *outcome = json_get_str(args, "run_outcome", "ok");
    if (!strcmp(outcome, "timeout")) {
        json_t *r = json_object();
        json_set(r, "__raise__", json_string("TimeoutError"));
        return r;
    }
    if (!strcmp(outcome, "error")) {
        json_t *r = json_object();
        json_set(r, "__raise__", json_string("RuntimeError"));
        return r;
    }
    return json_bool(true);
}
