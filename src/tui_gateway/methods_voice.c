/*
 * methods_voice.c — C home for upstream Hermes tui_gateway/methods_voice.py.
 *
 * PoP: methodvoi_voice_cfg_number @ tui_gateway/methods_voice.py:_voice_cfg_number
 * PoP: methodvoi_frame_fields @ tui_gateway/methods_voice.py:_frame_fields
 * PoP: methodvoi_fd_barge_params @ tui_gateway/methods_voice.py:_fd_barge_params
 * PoP: methodvoi_owner_result @ tui_gateway/methods_voice.py:_owner_result
 *
 * Faithful C11 ports of the deterministic helpers. All exported symbols are
 * `methodvoi_` prefixed. The only external seam (_ok) is identity on the wire.
 */
#include "noc_w13_methodsvoi_a3e372.h"
#include "hermes_json.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default: return 0;
    }
}

struct json_t *methodvoi_voice_cfg_number(const struct json_t *value, const struct json_t *dflt) {
    /* isinstance(value,(int,float)) and not isinstance(value,bool) */
    if (value && value->type == JSON_NUMBER)
        return json_copy(value);
    return dflt ? json_copy(dflt) : json_null();
}

struct json_t *methodvoi_frame_fields(const struct json_t *frame) {
    json_t *out = json_object();
    static const char *P = "sample_rate";
    static const char *L = "frame_length";
    json_t *v = frame ? json_obj_get(frame, P) : NULL;
    json_set(out, P, v ? json_copy(v) : json_number(16000));
    v = frame ? json_obj_get(frame, L) : NULL;
    json_set(out, L, v ? json_copy(v) : json_number(1280));
    return out;
}

/* float(v or 0): v truthy -> float(v), else 0.0; on conversion error -> 0.0. */
static double conv_float_or0(const json_t *v) {
    if (!py_truthy(v)) return 0.0;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL)   return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_STRING) {
        char *end = NULL;
        double d = strtod(v->str_val, &end);
        if (end && *end == '\0') return d;
        return 0.0; /* ValueError -> float(default=0) */
    }
    return 0.0;
}

/* int(float(v) * 1000); on error -> int(float(0.5)*1000) = 500. */
static double conv_grace_ms(const json_t *v) {
    double f;
    if (!v || v->type == JSON_NULL || v->type == JSON_BOOL) {
        f = 0.5;
    } else if (v->type == JSON_NUMBER) {
        f = v->num_val;
    } else if (v->type == JSON_STRING) {
        char *end = NULL;
        double d = strtod(v->str_val, &end);
        if (!end || *end != '\0') f = 0.5;
        else f = d;
    } else {
        f = 0.5;
    }
    double ms = trunc(f * 1000.0); /* int() truncates toward zero */
    if (ms < 0.0) ms = 0.0;        /* max(0, ...) */
    return ms;
}

struct json_t *methodvoi_fd_barge_params(const struct json_t *cfg) {
    json_t *arr = json_array();
    json_t *mv = cfg ? json_obj_get(cfg, "barge_in_threshold_multiplier") : NULL;
    json_t *gv = cfg ? json_obj_get(cfg, "barge_in_grace_seconds") : NULL;
    json_append(arr, json_number(conv_float_or0(mv)));
    json_append(arr, json_number(conv_grace_ms(gv)));
    return arr;
}

struct json_t *methodvoi_owner_result(const char *field, const struct json_t *ok,
                                      const struct json_t *extra) {
    json_t *out = json_object();
    json_set(out, field ? field : "", ok ? json_copy(ok) : json_null());
    if (py_truthy(ok)) json_set(out, "reason", json_null());
    else               json_set_hdr(out, "reason", "not_owner");
    if (extra && extra->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(extra); ++i) {
            const char *k = json_object_get_key_at(extra, i);
            if (k) json_set(out, k, json_copy(json_object_get_at(extra, i)));
        }
    }
    return out;
}
