/*
 * session_moa_turn.c — faithful C11 port of gateway/session_moa_turn.py.
 * Gate w10_sessionmoa_08e3f3.
 *
 * PoP: w10_sessionmoa_08e3f3_moa_override @ gateway/session_moa_turn.py:moa_override
 * PoP: w10_sessionmoa_08e3f3_stamp_moa_turn @ gateway/session_moa_turn.py:stamp_moa_turn
 * PoP: w10_sessionmoa_08e3f3_restore_moa_turn @ gateway/session_moa_turn.py:restore_moa_turn
 */
#include "hermes_json.h"
#include "noc_w10_sessionmoa_08e3f3.h"

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}

json_t *w10_sessionmoa_08e3f3_moa_override(const struct json_t *preset) {
    json_t *out = json_object();
    json_set(out, "provider", json_string("moa"));
    json_set(out, "model", preset ? json_copy(preset) : json_null());
    json_set(out, "base_url", json_string("moa://local"));
    json_set(out, "api_key", json_string("moa-virtual-provider"));
    json_set(out, "api_mode", json_string("chat_completions"));
    return out;
}

json_t *w10_sessionmoa_08e3f3_stamp_moa_turn(const struct json_t *preset,
                                             const struct json_t *envelope) {
    json_t *out = envelope && envelope->type == JSON_OBJECT ? json_copy(envelope) : json_object();
    if (py_truthy(preset)) {
        json_t *stamp = json_object();
        json_set(stamp, "preset", preset ? json_copy(preset) : json_null());
        json_set(out, "moa_turn_v1", stamp);
    }
    return out;
}

json_t *w10_sessionmoa_08e3f3_restore_moa_turn(const struct json_t *envelope) {
    json_t *stamp = json_obj_get(envelope, "moa_turn_v1");
    if (!stamp || stamp->type != JSON_OBJECT) return json_null();
    json_t *preset = json_obj_get(stamp, "preset");
    return preset ? json_copy(preset) : json_null();
}
