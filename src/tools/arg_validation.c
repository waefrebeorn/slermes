/*
 * arg_validation.c — faithful C11 port of the pure argument-check helpers in
 * tools/arg_validation.py (gate w7_argvalidati_b61680).
 *
 * PoP: w7_argvalidati_b61680_argval_with_choices @ tools/arg_validation.py:_with_choices
 * PoP: w7_argvalidati_b61680_argval_missing_unknown @ tools/arg_validation.py:_missing_and_unknown
 * PoP: w7_argvalidati_b61680_argval_flag_unknown_args @ tools/arg_validation.py:flag_unknown_args
 *
 * Only the pure data helpers needing no live ToolEntry/registry are ported.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w7_argvalidati_b61680.h"

static int is_type(const json_t *v, json_type_t t) { return v && v->type == t; }
static const char *scalar_str(const json_t *v) {
    return is_type(v, JSON_STRING) ? (v->str_val ? v->str_val : "") : "";
}

/* Python str() of a JSON scalar (enum values are scalars in the fixtures). */
static void py_str(const json_t *v, char *buf, size_t cap) {
    if (!v) { snprintf(buf, cap, "None"); return; }
    switch (v->type) {
        case JSON_STRING: snprintf(buf, cap, "%s", v->str_val ? v->str_val : ""); break;
        case JSON_BOOL:   snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); break;
        case JSON_NULL:   snprintf(buf, cap, "None"); break;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, cap, "%lld", (long long)v->num_val);
            else
                snprintf(buf, cap, "%g", v->num_val);
            break;
        default: snprintf(buf, cap, "[]"); break;
    }
}

struct json_t *argval_with_choices(const char *key, const struct json_t *properties) {
    if (!key) key = "";
    const json_t *spec = is_type(properties, JSON_OBJECT) ? json_obj_get(properties, key) : NULL;
    const json_t *choices = is_type(spec, JSON_OBJECT) ? json_obj_get(spec, "enum") : NULL;
    int truthy = is_type(choices, JSON_ARRAY) && json_len(choices) > 0;
    if (!truthy) return json_string(key);
    char out[4096];
    size_t o = 0;
    int written = snprintf(out + o, sizeof(out) - o, "%s (one of: ", key);
    if (written < 0 || (size_t)written >= sizeof(out) - o) return json_string(key);
    o += (size_t)written;
    for (size_t i = 0; i < json_len(choices) && o + 1 < sizeof(out); i++) {
        char val[512];
        py_str(json_get(choices, i), val, sizeof(val));
        if (i) { out[o++] = ','; out[o++] = ' '; }
        size_t n = strlen(val);
        if (o + n >= sizeof(out)) n = sizeof(out) - o - 1;
        memcpy(out + o, val, n); o += n;
    }
    if (o + 2 >= sizeof(out)) o = sizeof(out) - 3;
    out[o++] = ')'; out[o] = '\0';
    return json_string(out);
}

static int arr_contains(const json_t *arr, const char *s) {
    if (!is_type(arr, JSON_ARRAY) || !s) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *v = scalar_str(json_get(arr, i));
        if (v && !strcmp(v, s)) return 1;
    }
    return 0;
}
static int obj_has_key(const json_t *obj, const char *s) {
    if (!is_type(obj, JSON_OBJECT) || !s) return 0;
    for (size_t i = 0; i < json_object_size(obj); i++) {
        const char *k = json_object_get_key_at(obj, i);
        if (k && !strcmp(k, s)) return 1;
    }
    return 0;
}

struct json_t *argval_missing_unknown(const struct json_t *params, const struct json_t *args,
                                      const struct json_t *compat_args,
                                      const struct json_t *self_checked_args) {
    json_t *missing = json_array();
    json_t *unknown = json_array();
    const json_t *props = is_type(params, JSON_OBJECT) ? json_obj_get(params, "properties") : NULL;
    /* unknown = [key for key in args if key not in properties and key not in compat_args] */
    if (is_type(args, JSON_OBJECT)) {
        for (size_t i = 0; i < json_object_size(args); i++) {
            const char *k = json_object_get_key_at(args, i);
            if (!k) continue;
            if (!obj_has_key(props, k) && !arr_contains(compat_args, k))
                json_append(unknown, json_string(k));
        }
    }
    /* if any compat arg present: missing=[] */
    int compat_present = 0;
    if (is_type(args, JSON_OBJECT)) {
        for (size_t i = 0; i < json_object_size(args); i++) {
            const char *k = json_object_get_key_at(args, i);
            if (k && arr_contains(compat_args, k)) { compat_present = 1; break; }
        }
    }
    if (!compat_present) {
        const json_t *required = is_type(params, JSON_OBJECT) ? json_obj_get(params, "required") : NULL;
        if (is_type(required, JSON_ARRAY)) {
            for (size_t i = 0; i < json_len(required); i++) {
                const char *k = scalar_str(json_get(required, i));
                if (!k) continue;
                if (!obj_has_key(args, k) && !arr_contains(self_checked_args, k))
                    json_append(missing, json_string(k));
            }
        }
    }
    json_t *out = json_array();
    json_append(out, missing);
    json_append(out, unknown);
    return out;
}

static int json_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val != 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return json_len(v) > 0;
        case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

struct json_t *argval_flag_unknown_args(const char *result, const char *name,
                                        const struct json_t *unknown, const char *valid) {
    if (!result) result = "";
    if (!is_type(unknown, JSON_ARRAY) || json_len(unknown) == 0)
        return json_string(result);
    json_t *payload = json_parse(result, NULL);
    if (!is_type(payload, JSON_OBJECT)) {
        if (payload) json_free(payload);
        return json_string(result);
    }
    /* warning = ", ".join(unknown) + ": not a parameter of ..." */
    char joined[2048]; size_t o = 0;
    for (size_t i = 0; i < json_len(unknown) && o + 1 < sizeof(joined); i++) {
        const char *u = scalar_str(json_get(unknown, i));
        if (i) { joined[o++] = ','; joined[o++] = ' '; }
        size_t n = strlen(u ? u : "");
        if (o + n >= sizeof(joined)) n = sizeof(joined) - o - 1;
        memcpy(joined + o, u ? u : "", n); o += n;
    }
    joined[o] = '\0';
    char warn[4096];
    snprintf(warn, sizeof(warn),
             "%s: not a parameter of %s and was probably ignored. Valid parameters: %s.",
             joined, name ? name : "", valid ? valid : "");

    json_t *prev = json_obj_get(payload, "_warning");
    if (json_truthy(prev)) {
        if (prev->type != JSON_STRING || (prev->str_val && strstr(prev->str_val, warn))) {
            json_free(payload);
            return json_string(result);
        }
        char combined[8192];
        snprintf(combined, sizeof(combined), "%s\n%s", prev->str_val, warn);
        json_set(payload, "_warning", json_string(combined));
    } else {
        json_set(payload, "_warning", json_string(warn));
    }
    char *serialized = json_dumps(payload, 0);
    json_free(payload);
    json_t *out = json_string(serialized ? serialized : result);
    if (serialized) free(serialized);
    return out;
}
