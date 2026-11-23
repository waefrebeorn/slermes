/*
 * session_prompt_pin.c — C port of upstream Hermes gateway/session_prompt_pin.py.
 *
 * PoP: w13_sessionpro_0ed3f9_sanitize @ gateway/session_prompt_pin.py:sanitize_prompt_pin
 * PoP: w13_sessionpro_0ed3f9_get_pin @ gateway/session_prompt_pin.py:SessionPromptPinMixin.get_prompt_pin
 *
 * The routing-index lock and entry storage are seams; the validation rules and
 * the stale-turn guard are ported exactly (including Python's `True == 1`
 * version comparison and the falsy-pin `or None` behaviour).
 */
#include "noc_w13_sessionpro_0ed3f9.h"
#include "hermes_json.h"
#include <string.h>

#define PROMPT_PIN_VERSION 1

/* Python truthiness of a JSON value. */
static int jtruthy(const struct json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count != 0;
        default: return 0;
    }
}

/* pin.get("version") != PROMPT_PIN_VERSION -- Python equality, so True == 1. */
static int version_ok(const struct json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) return v->num_val == (double)PROMPT_PIN_VERSION;
    if (v->type == JSON_BOOL)   return v->bool_val && PROMPT_PIN_VERSION == 1;
    return 0;
}

struct json_t *w13_sessionpro_0ed3f9_sanitize(const struct json_t *pin) {
    if (!json_is_object(pin)) return json_null();
    struct json_t *version        = json_obj_get(pin, "version");
    struct json_t *context_key    = json_obj_get(pin, "context_key");
    struct json_t *context_prompt = json_obj_get(pin, "context_prompt");
    struct json_t *redact_pii     = json_obj_get(pin, "redact_pii");
    struct json_t *channel_prompt = json_obj_get(pin, "channel_prompt");
    struct json_t *parent_chat_id = json_obj_get(pin, "parent_chat_id");

    if (!version_ok(version)) return json_null();
    if (!json_is_string(context_key) || !context_key->str_val || !context_key->str_val[0]) return json_null();
    if (!json_is_string(context_prompt)) return json_null();
    if (!json_is_bool(redact_pii)) return json_null();
    if (channel_prompt && channel_prompt->type != JSON_NULL && !json_is_string(channel_prompt)) return json_null();
    if (parent_chat_id && parent_chat_id->type != JSON_NULL && !json_is_string(parent_chat_id)) return json_null();

    struct json_t *out = json_object();
    json_set(out, "version", json_number((double)PROMPT_PIN_VERSION));
    json_set(out, "context_key", json_copy(context_key));
    json_set(out, "context_prompt", json_copy(context_prompt));
    json_set(out, "redact_pii", json_copy(redact_pii));
    json_set(out, "channel_prompt", channel_prompt ? json_copy(channel_prompt) : json_null());
    json_set(out, "parent_chat_id", parent_chat_id ? json_copy(parent_chat_id) : json_null());
    return out;
}

struct json_t *w13_sessionpro_0ed3f9_get_pin(const struct json_t *entry,
                                              const struct json_t *expected_session_id) {
    /* entry = self._entry_locked(session_key) */
    if (!json_is_object(entry)) return json_null();
    struct json_t *actual = json_obj_get(entry, "session_id");

    /* if expected is not None and entry.session_id != expected: return None */
    if (expected_session_id && expected_session_id->type != JSON_NULL) {
        if (!json_is_string(actual) || !json_is_string(expected_session_id)
            || !actual->str_val || !expected_session_id->str_val
            || strcmp(actual->str_val, expected_session_id->str_val))
            return json_null();
    }

    /* return dict(entry.prompt_pin) if entry.prompt_pin else None */
    struct json_t *pin = json_obj_get(entry, "prompt_pin");
    if (!jtruthy(pin)) return json_null();
    if (json_is_object(pin)) return json_copy(pin);
    return json_null();
}
