/*
 * session_context_refs.c - C home for upstream Hermes gateway/session_context_refs.py.
 * PoP: w16_sessioncon_760928_refused_turn_result @ gateway/session_context_refs.py:refused_turn_result
 *
 * Faithful port of the pure refused-turn result builder. The async context
 * reference expansion stays with the live Python owner.
 */
#include "hermes_json.h"
#include "noc_w16_sessioncon_760928.h"

static json_t *clone(const json_t *v) {
    return v ? json_copy(v) : json_null();
}

struct json_t *w16_sessioncon_760928_refused_turn_result(const struct json_t *row,
                                                         const struct json_t *refusal) {
    const json_t *payload = json_obj_get(row, "payload");
    const json_t *finite = json_obj_get(payload, "finite");
    int is_true = finite && finite->type == JSON_BOOL && finite->bool_val;
    json_t *out = json_object();
    if (is_true) {
        json_set(out, "final_response", json_string(""));
        json_set(out, "messages", json_array());
        json_set(out, "failed", json_bool(1));
        json_set(out, "completed", json_bool(0));
        json_set(out, "error", clone(refusal));
    } else {
        json_set(out, "final_response", clone(refusal));
        json_set(out, "messages", json_array());
    }
    return out;
}
