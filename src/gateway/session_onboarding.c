/*
 * session_onboarding.c — C port of upstream Hermes gateway/session_onboarding.py.
 *
 * PoP: w13_sessiononb_967c70_handlers @ gateway/session_onboarding.py:handlers
 * PoP: w13_sessiononb_967c70_onboarding_connection @ gateway/session_onboarding.py:_onboarding_connection
 * PoP: w13_sessiononb_967c70_run_id @ gateway/session_onboarding.py:_run_id
 *
 * The sidecar's async/IO surface (start, skip, _choose_run, tracked writes) is a
 * seam in the differential oracle; this translation unit ports the pure
 * sidecar-side decisions that are observable without a live authority: the
 * handler-name map, the onboarding-profile sibling fallback, and the durable
 * run-id read with Python's `or` coercion.
 */
#include "noc_w13_sessiononb_967c70.h"
#include "hermes_json.h"

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

struct json_t *w13_sessiononb_967c70_handlers(void) {
    /* {'onboarding.start': partial(start, c), 'onboarding.skip': partial(skip, c)} */
    struct json_t *keys = json_array();
    json_append(keys, json_string("onboarding.skip"));
    json_append(keys, json_string("onboarding.start"));
    return keys;
}

struct json_t *w13_sessiononb_967c70_onboarding_connection(const struct json_t *sibling) {
    /* connection._sibling_for(ONBOARDING_PROFILE) or connection */
    if (jtruthy(sibling)) return json_copy(sibling);
    return json_string("self");
}

struct json_t *w13_sessiononb_967c70_run_id(const struct json_t *record) {
    /* (load(...) or {}).get('run_id') or ''  -- record is the projected load result. */
    struct json_t *value = json_is_object(record) ? json_obj_get(record, "run_id") : NULL;
    if (jtruthy(value)) return json_copy(value);
    return json_string("");
}
