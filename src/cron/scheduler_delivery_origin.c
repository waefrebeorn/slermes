/*
 * scheduler_delivery_origin.c — C port of upstream Hermes cron/scheduler_delivery_origin.py.
 *
 * PoP: w18_schedulerd_934af8_stamp @ cron/scheduler_delivery_origin.py:stamp_origin_discriminators
 *
 * Faithful setdefault stamping: origin-target scope_id and relay user_id only,
 * truthiness gate, str() coercion of non-string values, no overwrite of an
 * existing key in either metadata dict.
 */
#include "noc_w18_schedulerd_934af8.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0];
        case JSON_ARRAY:  return json_len(v) > 0;
        case JSON_OBJECT: return json_object_size(v) > 0;
        default:          return false;
    }
}
/* Never name a helper `dup` — `dupstr` only. */
static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}
/* Python str() for the scalar discriminator values we stamp. */
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("None");
    switch (v->type) {
        case JSON_BOOL:   return dupstr(v->bool_val ? "True" : "False");
        case JSON_STRING: return dupstr(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char b[64];
            double d = v->num_val;
            if (d == (double)(long long)d) snprintf(b, sizeof(b), "%.0f", d);
            else snprintf(b, sizeof(b), "%g", d);
            return dupstr(b);
        }
        case JSON_ARRAY:  return dupstr("[]");
        case JSON_OBJECT: return dupstr("{}");
        default:          return dupstr("");
    }
}
static void stamp_one(json_t *meta, const char *key, const json_t *value) {
    if (!truthy(value) || json_obj_get(meta, key) != NULL) return;  /* setdefault */
    char *s = py_str(value);
    if (s) { json_set(meta, key, json_string(s)); free(s); }
}

json_t *w18_schedulerd_934af8_stamp(const json_t *t, const json_t *route, const json_t *media) {
    json_t *r = (route && route->type == JSON_OBJECT) ? json_copy(route) : json_object();
    json_t *m = (media && media->type == JSON_OBJECT) ? json_copy(media) : json_object();
    if (t && t->type == JSON_OBJECT) {
        if (truthy(json_obj_get(t, "origin_target"))) {
            json_t *origin = json_obj_get(t, "origin");
            json_t *sid = (origin && origin->type == JSON_OBJECT) ? json_obj_get(origin, "scope_id") : NULL;
            stamp_one(r, "scope_id", sid);
            stamp_one(m, "scope_id", sid);
        }
        if (truthy(json_obj_get(t, "is_relay"))) {
            json_t *uid = json_obj_get(t, "origin_user_id");
            stamp_one(r, "user_id", uid);
            stamp_one(m, "user_id", uid);
        }
    }
    json_t *out = json_object();
    json_set(out, "route", r);
    json_set(out, "media", m);
    return out;
}
