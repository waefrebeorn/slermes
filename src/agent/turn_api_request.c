/*
 * turn_api_request.c — C port of upstream Hermes agent/turn_api_request.py.
 *
 * PoP: w8_turnapireq_cc50be_set_extra_header @ agent/turn_api_request.py:_set_extra_header
 * PoP: w8_turnapireq_cc50be_apply_user_turn_request_headers @ agent/turn_api_request.py:_apply_user_turn_request_headers
 *
 * The route_answer("request_headers") collaborator is injected by the fixture
 * (request_headers); all ported logic is the real decision/copy work.
 */
#include "noc_w8_turnapireq_cc50be.h"
#include "hermes_json.h"
#include <string.h>

/* Python truthiness of getattr(agent, name, default). */
static int js_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:  return v->c.count > 0 ? 1 : 0;
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default: return 0;
    }
}

json_t *w8_turnapireq_cc50be_set_extra_header(const json_t *api_kwargs,
                                              const char *key,
                                              const struct json_t *value) {
    json_t *out = json_object();
    if (api_kwargs && api_kwargs->type == JSON_OBJECT) {
        for (size_t i = 0; i < api_kwargs->c.count; i++) {
            const char *k = api_kwargs->c.keys[i];
            if (!k) continue;
            json_set(out, k, json_copy(api_kwargs->c.items[i]));
        }
    }
    json_t *xh = json_object();
    const json_t *eh = api_kwargs ? json_obj_get(api_kwargs, "extra_headers") : NULL;
    if (eh && eh->type == JSON_OBJECT) {
        for (size_t i = 0; i < eh->c.count; i++) {
            const char *k = eh->c.keys[i];
            if (!k) continue;
            json_set(xh, k, json_copy(eh->c.items[i]));
        }
    }
    json_set(xh, key ? key : "", value ? json_copy(value) : json_null());
    json_set(out, "extra_headers", xh);
    return out;
}

json_t *w8_turnapireq_cc50be_apply_user_turn_request_headers(
    const json_t *agent, const json_t *api_kwargs, const json_t *request_headers) {
    const json_t *orig = agent ? json_obj_get(agent, "_is_user_initiated_turn") : NULL;
    int is_user = js_truthy(orig);
    json_t *res = json_object();

    if (!is_user) {
        /* Python returns early; the flag keeps whatever falsy value it had
         * (getattr default False when absent). */
        json_set(res, "api_kwargs", api_kwargs ? json_copy(api_kwargs) : json_object());
        json_set(res, "_is_user_initiated_turn", orig ? json_copy(orig) : json_bool(false));
        return res;
    }

    /* agent._is_user_initiated_turn = False */
    json_t *kwargs = api_kwargs ? json_copy(api_kwargs) : json_object();
    if (request_headers && request_headers->type == JSON_OBJECT) {
        for (size_t i = 0; i < request_headers->c.count; i++) {
            const char *k = request_headers->c.keys[i];
            if (!k) continue;
            json_t *next = w8_turnapireq_cc50be_set_extra_header(kwargs, k, request_headers->c.items[i]);
            json_free(kwargs);
            kwargs = next;
        }
    }
    json_set(res, "api_kwargs", kwargs);
    json_set(res, "_is_user_initiated_turn", json_bool(false));
    return res;
}
