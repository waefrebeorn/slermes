/*
 * jobs_model_pin.c — real C11 port of upstream Hermes cron/jobs_model_pin.py.
 *
 * PoP: jobspin_main_model_pin @ cron/jobs_model_pin.py:_main_model_pin
 *
 * The upstream function reads config.yaml and resolves the provider through
 * I/O; this module ports its deterministic decision core: config model
 * selection -> _normalize_job_optional_text -> provider.lower(). The I/O
 * boundary is represented by the `cfg` object and `provider_raw` argument,
 * which the differential oracle feeds from controlled stand-ins.
 * Self-contained (libc + libjson only).
 */
#include "noc_jobspin.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Python truthiness for the JSON value shapes cfg/model may take. */
static bool py_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
    case JSON_NULL:   return false;
    case JSON_BOOL:   return v->bool_val;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && *v->str_val;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}

/* Faithful core of cron/jobs_normalize.py::_normalize_job_optional_text:
 * str.strip() then `or None`; a non-str (NULL here) yields None. */
static char *opt_text(const char *s) {
    size_t a = 0, b;
    char *out;
    if (!s) return NULL;
    b = strlen(s);
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    if (b == a) return NULL;
    out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

static char *ascii_lower(const char *s) {
    size_t i, n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}

/* PoP: jobspin_main_model_pin @ cron/jobs_model_pin.py:_main_model_pin */
json_t *jobspin_main_model_pin(const json_t *cfg, const char *provider_raw) {
    json_t *out = json_array();
    const json_t *model_cfg = NULL;
    const json_t *default_v, *model_raw = NULL;
    char *model = NULL, *provider = NULL, *lower = NULL;

    if (!out) return NULL;

    /* cfg.get("model") or {} */
    if (cfg && cfg->type == JSON_OBJECT)
        model_cfg = json_obj_get(cfg, "model");
    if (!py_truthy(model_cfg))
        model_cfg = NULL;              /* the `or {}` branch: an empty dict */

    /* default or model_cfg.get("model") if dict else model_cfg */
    if (model_cfg && model_cfg->type == JSON_OBJECT) {
        default_v = json_obj_get(model_cfg, "default");
        model_raw = py_truthy(default_v) ? default_v
                                         : json_obj_get(model_cfg, "model");
    } else if (model_cfg) {
        model_raw = model_cfg;
    }

    if (model_raw && model_raw->type == JSON_STRING)
        model = opt_text(model_raw->str_val);
    if (!model) {
        json_append(out, json_null());
        json_append(out, json_null());
        return out;
    }

    provider = opt_text(provider_raw);
    if (provider) {
        lower = ascii_lower(provider);
        free(provider);
        json_append(out, lower ? json_string(lower) : json_null());
        free(lower);
    } else {
        json_append(out, json_null());
    }
    json_append(out, json_string(model));
    free(model);
    return out;
}

