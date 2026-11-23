/*
 * mcp_oauth_device.c - C home for upstream Hermes tools/mcp_oauth_device.py.
 *
 * PoP: w9_mcpoauthde_c3e807_positive_seconds @ tools/mcp_oauth_device.py:_positive_seconds
 * PoP: w9_mcpoauthde_c3e807_payload @ tools/mcp_oauth_device.py:_payload
 * PoP: w9_mcpoauthde_c3e807_constants @ tools/mcp_oauth_device.py:DEVICE_GRANT
 * PoP: w9_mcpoauthde_c3e807_device_grant_supported @ tools/mcp_oauth_device.py:_device_metadata
 */
#include "noc_w9_mcpoauthde_c3e807.h"
#include "hermes_json.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const DEVICE_GRANT = "urn:ietf:params:oauth:grant-type:device_code";

/* Result object: {"ok": false, "error": msg}. */
static struct json_t *err_json(const char *msg) {
    struct json_t *o = json_object();
    json_set(o, "ok", json_bool(false));
    json_set(o, "error", json_string(msg));
    return o;
}

static double value_as_double(const struct json_t *v) {
    if (!v) return NAN;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_STRING) return strtod(v->str_val ? v->str_val : "", NULL);
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return NAN;
}

struct json_t *w9_mcpoauthde_c3e807_positive_seconds(const struct json_t *value, const char *label) {
    double d = value_as_double(value);
    if (!isfinite(d) || d <= 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Device authorization has invalid %s", label ? label : "");
        return err_json(msg);
    }
    struct json_t *o = json_object();
    json_set(o, "ok", json_bool(true));
    json_set(o, "value", json_number(d));
    return o;
}

struct json_t *w9_mcpoauthde_c3e807_payload(int status, const struct json_t *data, int bad_json, const char *label) {
    const char *lbl = label ? label : "";
    if (bad_json) {
        char msg[256];
        snprintf(msg, sizeof(msg), "%s: invalid JSON response", lbl);
        return err_json(msg);
    }
    if (!data || data->type != JSON_OBJECT) {
        char msg[256];
        snprintf(msg, sizeof(msg), "%s: expected a JSON object", lbl);
        return err_json(msg);
    }
    if (status < 200 || status >= 300) {
        char msg[256];
        snprintf(msg, sizeof(msg), "%s failed (HTTP %d)", lbl, status);
        return err_json(msg);
    }
    struct json_t *o = json_object();
    json_set(o, "ok", json_bool(true));
    json_set(o, "value", json_copy(data));
    return o;
}

struct json_t *w9_mcpoauthde_c3e807_constants(void) {
    struct json_t *o = json_object();
    json_set(o, "device_grant", json_string(DEVICE_GRANT));
    return o;
}

int w9_mcpoauthde_c3e807_device_grant_supported(const struct json_t *grants) {
    if (!grants || grants->type == JSON_NULL) return 1; /* None => no restriction */
    if (grants->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(grants); ++i) {
        struct json_t *g = json_get(grants, i);
        if (g && g->type == JSON_STRING && g->str_val && !strcmp(g->str_val, DEVICE_GRANT))
            return 1;
    }
    return 0;
}
