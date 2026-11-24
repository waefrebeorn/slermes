/*
 * run_turn_followup_ack.c — faithful C11 port of upstream Hermes
 * gateway/run_turn_followup_ack.py (gate w17_runturnfo_18196f).
 *
 * PoP: w17_runturnfo_18196f_followup_processing_hooks_apply @ gateway/run_turn_followup_ack.py:_followup_processing_hooks_apply
 * PoP: w17_runturnfo_18196f_followup_cancel_outcome @ gateway/run_turn_followup_ack.py:_followup_cancel_outcome
 * PoP: w17_runturnfo_18196f_run_followup_processing_hook @ gateway/run_turn_followup_ack.py:_run_followup_processing_hook
 *
 * Decision functions ported verbatim.  Collaborator state (adapter attributes,
 * event attributes, asyncio.current_task()) is delivered as JSON object doubles;
 * `io` supplies scripted seam results.  No stubs.
 */
#include "noc_w17_runturnfo_18196f.h"

#include <string.h>
#include <stdlib.h>

static bool rtf_truthy(json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:  return v->c.count != 0;
        case JSON_OBJECT: return v->c.count != 0;
    }
    return false;
}
static bool rtf_is_null(json_t *v) { return !v || v->type == JSON_NULL; }

static bool rtf_equal(json_t *a, json_t *b) {
    if (rtf_is_null(a) || rtf_is_null(b)) return rtf_is_null(a) && rtf_is_null(b);
    if (a->type != b->type) {
        if (a->type == JSON_NUMBER && b->type == JSON_NUMBER) return a->num_val == b->num_val;
        return false;
    }
    switch (a->type) {
        case JSON_BOOL:   return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return a->str_val && b->str_val && strcmp(a->str_val, b->str_val) == 0;
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!rtf_equal(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !rtf_equal(a->c.items[i], bv)) return false;
            }
            return true;
        default: return true;
    }
}

/* PoP: w17_runturnfo_18196f_followup_processing_hooks_apply */
bool w17_runturnfo_18196f_followup_processing_hooks_apply(json_t *adapter, json_t *event) {
    if (rtf_is_null(adapter) || rtf_is_null(event)) return false;
    json_t *mid = json_obj_get(event, "message_id");
    json_t *raw = json_obj_get(event, "raw_message");
    if (!(rtf_truthy(mid) || rtf_truthy(raw))) return false;
    return json_get_bool(adapter, "overrides_start", false);
}

/* PoP: w17_runturnfo_18196f_followup_cancel_outcome */
const char *w17_runturnfo_18196f_followup_cancel_outcome(json_t *adapter, json_t *io) {
    json_t *expected = rtf_is_null(adapter) ? NULL : json_obj_get(adapter, "expected_cancelled");
    if (rtf_is_null(expected)) return "FAILURE";
    json_t *ts = io ? json_obj_get(io, "current_task") : NULL;
    if (json_is_object(ts) && json_obj_get(ts, "$raise")) return "FAILURE";
    json_t *current = ts;
    if (rtf_is_null(current)) return "FAILURE";
    if (expected->type == JSON_ARRAY) {
        for (size_t i = 0; i < expected->c.count; i++)
            if (rtf_equal(current, expected->c.items[i])) return "CANCELLED";
        return "FAILURE";
    }
    if (expected->type == JSON_STRING && current->type == JSON_STRING &&
        expected->str_val && current->str_val && strstr(expected->str_val, current->str_val))
        return "CANCELLED";
    return "FAILURE";
}

/* PoP: w17_runturnfo_18196f_run_followup_processing_hook */
json_t *w17_runturnfo_18196f_run_followup_processing_hook(json_t *adapter, json_t *event,
        const char *hook_name, json_t *extra_args, json_t *io) {
    json_t *calls = json_array();
    (void)io;
    if (!w17_runturnfo_18196f_followup_processing_hooks_apply(adapter, event)) return calls;
    if (!json_get_bool(adapter, "run_hook", false)) return calls;
    json_t *call = json_object();
    json_set(call, "hook", json_string(hook_name ? hook_name : ""));
    json_set(call, "event", rtf_is_null(event) ? json_null() : json_copy(event));
    json_set(call, "extra", (extra_args && extra_args->type == JSON_ARRAY) ? json_copy(extra_args) : json_array());
    json_append(calls, call);
    return calls;
}
