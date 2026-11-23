/*
 * run_turn_prepare.c — C home for upstream Hermes gateway/run_turn_prepare.py.
 *
 * PoP: w10_runturnpr_6928a3_session_override_snapshot @ gateway/run_turn_prepare.py:_session_override_snapshot
 * PoP: w10_runturnpr_6928a3_recover_or_remember_model @ gateway/run_turn_prepare.py:_recover_or_remember_model
 *
 * The model-override snapshot projection and the last-known-good model
 * recovery safety net.  Session state is injected through fixture fields
 * (state_last / star_last) instead of a live SessionStore.
 */
#include "noc_w10_runturnpr_6928a3.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

static int is_empty(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 1;
    if (v->type == JSON_STRING) return v->str_val[0] == '\0';
    if (v->type == JSON_ARRAY) return json_len(v) == 0;
    if (v->type == JSON_OBJECT) return json_object_size(v) == 0;
    if (v->type == JSON_BOOL) return !v->bool_val;
    if (v->type == JSON_NUMBER) return v->num_val == 0.0;
    return 1;
}
static void put_field(json_t *out, const char *key, const json_t *src) {
    const json_t *v = src ? json_obj_get(src, key) : NULL;
    json_set(out, key, v ? json_copy(v) : json_null());
}

struct json_t *w10_runturnpr_6928a3_session_override_snapshot(const struct json_t *override) {
    static const char *const KEYS[] = {"provider", "requested_provider", "api_key", "base_url",
                                       "api_mode", "max_tokens", "credential_pool",
                                       "request_overrides", "capabilities"};
    json_t *out = json_object();
    if (!override || override->type != JSON_OBJECT) return out;
    for (size_t i = 0; i < sizeof(KEYS)/sizeof(*KEYS); i++) put_field(out, KEYS[i], override);
    const json_t *caps = json_obj_get(out, "capabilities");
    json_t *newcaps = json_object();
    if (caps && caps->type == JSON_OBJECT)
        for (size_t i = 0; i < json_object_size(caps); i++)
            json_set(newcaps, json_object_get_key_at(caps, i), json_copy(json_object_get_at(caps, i)));
    json_set(out, "capabilities", newcaps);
    return out;
}

struct json_t *w10_runturnpr_6928a3_recover_or_remember_model(const char *skey, const struct json_t *model,
                                                             const struct json_t *state_last,
                                                             const struct json_t *star_last) {
    if (!is_empty(model)) return json_copy((json_t *)model);
    if (!skey || skey[0] == '\0') {
        /* skey falsy: no per-session state read. */
    }
    const json_t *recovered = NULL;
    if (skey && skey[0] != '\0' && state_last && state_last->type == JSON_STRING && state_last->str_val[0] != '\0')
        recovered = state_last;
    if (!recovered && star_last && star_last->type == JSON_STRING && star_last->str_val[0] != '\0')
        recovered = star_last;
    if (recovered) return json_copy((json_t *)recovered);
    if (!model || model->type == JSON_NULL) return json_null();
    return json_copy((json_t *)model);
}
