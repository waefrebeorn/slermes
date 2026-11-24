/*
 * delegate_tool_child_run.c — C port of upstream Hermes tools/delegate_tool_child_run.py.
 *
 * PoP: w17_delegateto_5314c5_num @ tools/delegate_tool_child_run.py:_num
 * PoP: w17_delegateto_5314c5_str_or_none @ tools/delegate_tool_child_run.py:_str_or_none
 * PoP: w17_delegateto_5314c5_is_image_url @ tools/delegate_tool_child_run.py:_is_image_url
 * PoP: w17_delegateto_5314c5_budget_warning_text @ tools/delegate_tool_child_run.py:_budget_warning_text
 * PoP: w17_delegateto_5314c5_fabricated_entry @ tools/delegate_tool_child_run.py:_fabricated_entry
 * PoP: w17_delegateto_5314c5_append_missed_steer @ tools/delegate_tool_child_run.py:_append_missed_steer
 * PoP: w17_delegateto_5314c5_tier_in_effect @ tools/delegate_tool_child_run.py:_tier_in_effect
 * PoP: w17_delegateto_5314c5_constants @ tools/delegate_tool_child_run.py:_HEARTBEAT_INTERVAL/_HEARTBEAT_STALE_CYCLES_IDLE/_HEARTBEAT_STALE_CYCLES_IN_TOOL/_STALE_RESULT_GRACE_SECONDS/_LIVENESS_POLL_SECONDS/_BUDGET_WARNING_FRACTION
 */
#include "noc_w17_delegateto_5314c5.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0];
        case JSON_ARRAY: return json_len(v) > 0;
        case JSON_OBJECT: return json_object_size(v) > 0;
        default: return false;
    }
}
static json_t *copy_or_null(const json_t *v) { return v ? json_copy(v) : json_null(); }

json_t *w17_delegateto_5314c5_num(const json_t *value, const json_t *def) {
    long long d = 0;
    if (def && (def->type == JSON_NUMBER || def->type == JSON_BOOL))
        d = def->type == JSON_BOOL ? (def->bool_val ? 1 : 0) : (long long)def->num_val;
    if (value && (value->type == JSON_NUMBER || value->type == JSON_BOOL)) {
        long long n = value->type == JSON_BOOL
            ? (value->bool_val ? 1 : 0) : (long long)value->num_val;
        return json_number((double)n);
    }
    return json_number((double)d);
}

json_t *w17_delegateto_5314c5_str_or_none(const json_t *value) {
    if (value && value->type == JSON_STRING) return json_copy(value);
    return json_null();
}

json_t *w17_delegateto_5314c5_is_image_url(const char *ref) {
    if (!ref) return json_bool(false);
    return json_bool(!strncmp(ref, "http://", 7) || !strncmp(ref, "https://", 8)
                     || !strncmp(ref, "data:image/", 11));
}

json_t *w17_delegateto_5314c5_budget_warning_text(double idle_seconds, double child_timeout) {
    char b[512];
    /* em dash — is UTF-8 E2 80 94 */
    snprintf(b, sizeof(b),
        "[delegation budget warning] No progress signal for %.0fs of your %.0fs "
        "inactivity window. Finish the current step and return your summary now \xe2\x80\x94 "
        "the work is discarded if the window elapses.", idle_seconds, child_timeout);
    return json_string(b);
}

json_t *w17_delegateto_5314c5_fabricated_entry(int idx, const char *status, const json_t *error,
                                              const json_t *child, double duration) {
    json_t *entry = json_object();
    json_set(entry, "task_index", json_number(idx));
    json_set(entry, "status", json_string(status ? status : ""));
    json_set(entry, "summary", json_null());
    json_set(entry, "error", copy_or_null(error));
    json_set(entry, "api_calls", json_number(0));
    json_set(entry, "duration_seconds", json_number(duration));
    json_t *role = (child && child->type == JSON_OBJECT) ? json_obj_get(child, "_delegate_role") : NULL;
    json_set(entry, "_child_role", copy_or_null(role));
    /* Schema-less children take no route branch; a non-dict _delegate_route
     * yields no "route" key too. */
    return entry;
}

json_t *w17_delegateto_5314c5_append_missed_steer(const json_t *entry, const json_t *late_steer) {
    json_t *out = entry ? json_copy(entry) : json_object();
    if (late_steer && late_steer->type == JSON_STRING && late_steer->str_val && late_steer->str_val[0]) {
        json_t *err = json_obj_get(out, "error");
        const char *base = (err && err->type == JSON_STRING && err->str_val) ? err->str_val : "";
        size_t n = strlen(base) + strlen(late_steer->str_val) + 96;
        char *joined = (char *)malloc(n);
        if (joined) {
            snprintf(joined, n, "%s [steer did not land before the subagent stopped: %s]",
                     base, late_steer->str_val);
            json_set(out, "error", json_string(joined));
            free(joined);
        }
        json_set(out, "missed_steer", json_copy(late_steer));
    }
    return out;
}

static const json_t *truthy_str(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    json_t *v = json_obj_get(obj, key);
    return py_truthy(v) ? v : NULL;
}

json_t *w17_delegateto_5314c5_tier_in_effect(const json_t *child) {
    if (!child || child->type != JSON_OBJECT) return json_null();
    const json_t *own = truthy_str(child, "service_tier");
    if (own) return json_copy(own);
    json_t *overrides = json_obj_get(child, "request_overrides");
    if (!overrides || overrides->type != JSON_OBJECT) overrides = NULL;
    json_t *extra = NULL;
    if (overrides) {
        json_t *e = json_obj_get(overrides, "extra_body");
        if (e && e->type == JSON_OBJECT) extra = e;
    }
    for (int scope = 0; scope < 2; scope++) {
        json_t *s = scope == 0 ? overrides : extra;
        if (!s) continue;
        const json_t *tier = truthy_str(s, "service_tier");
        if (tier) return json_copy(tier);
        json_t *speed = json_obj_get(s, "speed");
        if (speed && speed->type == JSON_STRING && speed->str_val && !strcmp(speed->str_val, "fast"))
            return json_string("priority");
    }
    return json_null();
}

json_t *w17_delegateto_5314c5_constants(void) {
    json_t *out = json_object();
    json_set(out, "_HEARTBEAT_INTERVAL", json_number(30));
    json_set(out, "_HEARTBEAT_STALE_CYCLES_IDLE", json_number(15));
    json_set(out, "_HEARTBEAT_STALE_CYCLES_IN_TOOL", json_number(40));
    json_set(out, "_STALE_RESULT_GRACE_SECONDS", json_number(2.0));
    json_set(out, "_LIVENESS_POLL_SECONDS", json_number(5.0));
    json_set(out, "_BUDGET_WARNING_FRACTION", json_number(0.8));
    return out;
}
