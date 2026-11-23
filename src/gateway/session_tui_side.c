/*
 * session_tui_side.c — faithful C11 port of gateway/session_tui_side.py.
 * Gate w10_sessiontui_131199.
 *
 * PoP: w10_sessiontui_131199_abandoned @ gateway/session_tui_side.py:_abandoned
 * PoP: w10_sessiontui_131199_admit_valid @ gateway/session_tui_side.py:_admit
 */
#include "hermes_json.h"
#include "noc_w10_sessiontui_131199.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

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

json_t *w10_sessiontui_131199_abandoned(const struct json_t *holder) {
    if (!holder || holder->type != JSON_ARRAY || holder->c.count == 0) return json_bool(false);
    json_t *first = holder->c.items[0];
    if (!first || first->type != JSON_OBJECT) return json_bool(false);
    json_t *a = json_obj_get(first, "abandoned");
    if (!a || a->type == JSON_NULL) return json_bool(false);
    return json_bool(py_truthy(a) ? true : false);
}

json_t *w10_sessiontui_131199_admit_valid(const struct json_t *params,
                                          const struct json_t *sid,
                                          const char *required,
                                          const struct json_t *optional) {
    if (!params || params->type != JSON_OBJECT) return json_bool(false);
    /* set(params) - {'session_id', 'profile', required, *optional} must be empty. */
    for (size_t i = 0; i < json_object_size(params); i++) {
        const char *k = json_object_get_key_at(params, i);
        if (!k) continue;
        int allowed = !strcmp(k, "session_id") || !strcmp(k, "profile") ||
                      (required && !strcmp(k, required));
        if (!allowed && optional && optional->type == JSON_ARRAY) {
            for (size_t j = 0; j < optional->c.count; j++) {
                json_t *o = optional->c.items[j];
                if (o && o->type == JSON_STRING && o->str_val && !strcmp(k, o->str_val)) {
                    allowed = 1;
                    break;
                }
            }
        }
        if (!allowed) return json_bool(false);
    }
    /* not isinstance(ref.session_id, str) or not ref.session_id */
    if (!sid || sid->type != JSON_STRING || !sid->str_val || !*sid->str_val)
        return json_bool(false);
    /* text = params.get(required); not isinstance(text, str) or not text.strip() */
    json_t *text = json_obj_get(params, required);
    if (!text || text->type != JSON_STRING || !text->str_val) return json_bool(false);
    const char *a = text->str_val;
    while (*a && ws((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && ws((unsigned char)b[-1])) b--;
    if (b == a) return json_bool(false);
    /* any(params.get(k) is not None and not isinstance(params[k], str) for k in optional) */
    if (optional && optional->type == JSON_ARRAY) {
        for (size_t j = 0; j < optional->c.count; j++) {
            json_t *o = optional->c.items[j];
            if (!o || o->type != JSON_STRING || !o->str_val) continue;
            json_t *v = json_obj_get(params, o->str_val);
            if (v && v->type != JSON_NULL && v->type != JSON_STRING)
                return json_bool(false);
        }
    }
    return json_bool(true);
}
