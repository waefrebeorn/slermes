/*
 * api_server_request_options.c -- real C11 port of the per-request
 * `model_options` readers of upstream Hermes
 *   gateway/platforms/api_server_request_options.py.
 *
 * PoP: w21_apiserverr_960557_reasoning_config @ gateway/platforms/api_server_request_options.py:_request_reasoning_config
 * PoP: w21_apiserverr_960557_service_tier @ gateway/platforms/api_server_request_options.py:_request_service_tier
 * PoP: w21_apiserverr_960557_reasoning_efforts @ gateway/platforms/api_server.py:_REASONING_EFFORTS
 *
 * Both functions are pure readers of the decoded request dict; the three
 * facade helpers they import lazily (`_REASONING_EFFORTS`,
 * `_clean_request_string`, `_coerce_request_bool`) and the
 * `_REQUEST_OPTION_MISSING` sentinel are re-implemented here verbatim, since
 * the lazy import exists only to dodge the facade's import cycle.
 */
#include "hermes_json.h"
#include "noc_w21_apiserverr_960557.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The facade's ladder, as a sorted list for the introspection helper. */
static const char *const ASRV_EFFORTS[] = {
    "high", "low", "max", "medium", "minimal", "none", "ultra", "xhigh", NULL
};

static int asrv_in_efforts(const char *norm) {
    int i;
    if (!norm) return 0;
    for (i = 0; ASRV_EFFORTS[i]; i++) {
        if (!strcmp(norm, ASRV_EFFORTS[i])) return 1;
    }
    return 0;
}

static int asrv_is_int_val(const json_t *v) {
    double d;
    if (!v || v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d) || floor(d) != d) return 0;
    if (d < -9.007199254740992e15 || d > 9.007199254740992e15) return 0;
    return 1;
}

static void asrv_strip_lower(const char *src, char *dst, size_t cap) {
    const char *b = src, *e;
    size_t n = 0;
    while (*b && isspace((unsigned char)*b)) b++;
    e = b + strlen(b);
    while (e > b && isspace((unsigned char)e[-1])) e--;
    while (b < e && n + 1 < cap) {
        dst[n++] = (char)tolower((unsigned char)*b++);
    }
    dst[n] = '\0';
}

/*
 * `str(effort).strip().lower() if effort is not None else ""`.
 *
 * Only the *membership* of the result in _REASONING_EFFORTS matters, so a
 * container's Python repr (which can never equal an effort name and can never
 * be "none") is folded to a byte no name contains. Scalars are faithful:
 * str(True) -> "true", str(5) -> "5", str(1.5) -> "1.5".
 */
static void asrv_effort_norm(const json_t *effort, char *dst, size_t cap) {
    char buf[64];
    if (!effort || effort->type == JSON_NULL) {
        dst[0] = '\0';
        return;
    }
    switch (effort->type) {
    case JSON_STRING:
        asrv_strip_lower(effort->str_val ? effort->str_val : "", dst, cap);
        return;
    case JSON_BOOL:
        asrv_strip_lower(effort->bool_val ? "True" : "False", dst, cap);
        return;
    case JSON_NUMBER:
        if (asrv_is_int_val(effort)) {
            snprintf(buf, sizeof(buf), "%lld", (long long)effort->num_val);
        } else {
            snprintf(buf, sizeof(buf), "%.17g", effort->num_val);
        }
        asrv_strip_lower(buf, dst, cap);
        return;
    default:
        dst[0] = '\001';                 /* never a name, never "none" */
        dst[1] = '\0';
        return;
    }
}

/* `_clean_request_string(value)`: stripped string, or None when empty. */
static json_t *asrv_clean_string(const char *value) {
    const char *b, *e;
    char *out;
    size_t n;
    json_t *node;
    if (!value) return json_null();
    b = value;
    while (*b && isspace((unsigned char)*b)) b++;
    e = b + strlen(b);
    while (e > b && isspace((unsigned char)e[-1])) e--;
    n = (size_t)(e - b);
    if (n == 0) return json_null();
    out = (char *)malloc(n + 1);
    if (!out) return json_null();
    memcpy(out, b, n);
    out[n] = '\0';
    node = json_string(out);
    free(out);
    return node;
}

/* `_coerce_request_bool(value, default=False)`. */
static int asrv_coerce_bool(const json_t *v, int def) {
    if (v) {
        switch (v->type) {
        case JSON_BOOL:
            return v->bool_val ? 1 : 0;
        case JSON_NULL:
            return def;
        case JSON_STRING: {
            char norm[64];
            asrv_strip_lower(v->str_val ? v->str_val : "", norm, sizeof(norm));
            if (!strcmp(norm, "1") || !strcmp(norm, "true") ||
                !strcmp(norm, "yes") || !strcmp(norm, "on")) return 1;
            if (!strcmp(norm, "0") || !strcmp(norm, "false") ||
                !strcmp(norm, "no") || !strcmp(norm, "off")) return 0;
            return def;
        }
        case JSON_NUMBER:
            return v->num_val != 0.0 ? 1 : 0;
        default:
            return def;
        }
    }
    return def;
}

json_t *w21_apiserverr_960557_reasoning_config(const json_t *model_options) {
    const json_t *reasoning, *enabled = NULL, *effort = NULL;
    char norm[64];
    int enabled_false, enabled_true;
    json_t *out;

    if (!model_options || model_options->type != JSON_OBJECT) return json_null();

    reasoning = json_obj_get(model_options, "reasoning");
    if (reasoning && reasoning->type == JSON_OBJECT) {
        enabled = json_obj_get(reasoning, "enabled");
        effort = json_obj_get(reasoning, "effort");
    }
    /* `model_options.get("reasoning_effort", effort)`: presence wins, even null. */
    if (json_has(model_options, "reasoning_effort")) {
        effort = json_obj_get(model_options, "reasoning_effort");
    }
    asrv_effort_norm(effort, norm, sizeof(norm));

    /* `enabled is False` -- a strict bool, not 0/""/None. */
    enabled_false = enabled && enabled->type == JSON_BOOL && !enabled->bool_val;
    enabled_true = enabled && enabled->type == JSON_BOOL && enabled->bool_val;

    if (enabled_false || !strcmp(norm, "none")) {
        out = json_object();
        json_set(out, "enabled", json_bool(0));
        return out;
    }
    if (asrv_in_efforts(norm) && strcmp(norm, "none")) {
        out = json_object();
        json_set(out, "enabled", json_bool(1));
        json_set(out, "effort", json_string(norm));
        return out;
    }
    if (enabled_true) {
        out = json_object();
        json_set(out, "enabled", json_bool(1));
        return out;
    }
    return json_null();
}

json_t *w21_apiserverr_960557_service_tier(const json_t *model_options) {
    json_t *out = json_object();

    if (model_options && model_options->type == JSON_OBJECT) {
        if (json_has(model_options, "service_tier")) {
            const json_t *raw = json_obj_get(model_options, "service_tier");
            json_set(out, "present", json_bool(1));
            if (raw && raw->type == JSON_STRING) {
                json_set(out, "value", asrv_clean_string(raw->str_val));
            } else {
                json_set(out, "value", raw ? json_copy(raw) : json_null());
            }
            return out;
        }
        if (json_has(model_options, "fast")) {
            int fast = asrv_coerce_bool(json_obj_get(model_options, "fast"), 0);
            json_set(out, "present", json_bool(1));
            json_set(out, "value", fast ? json_string("priority") : json_null());
            return out;
        }
    }
    json_set(out, "present", json_bool(0));
    json_set(out, "value", json_null());
    return out;
}

json_t *w21_apiserverr_960557_reasoning_efforts(void) {
    json_t *arr = json_array();
    int i;
    for (i = 0; ASRV_EFFORTS[i]; i++) json_append(arr, json_string(ASRV_EFFORTS[i]));
    return arr;
}
