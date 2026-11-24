/*
 * config_version_stamp.c — C home for upstream Hermes hermes_cli/config_version_stamp.py.
 * PoP: w26_configvers_b21168_coerce @ hermes_cli/config_version_stamp.py:_coerce_config_version
 * PoP: w26_configvers_b21168_read_stamp @ hermes_cli/config_version_stamp.py:read_config_version_stamp
 * PoP: w26_configvers_b21168_check_version @ hermes_cli/config_version_stamp.py:check_config_version
 * PoP: w26_configvers_b21168_newer_message @ hermes_cli/config_version_stamp.py:newer_stamp_message
 * PoP: w26_configvers_b21168_constants @ hermes_cli/config_version_stamp.py:<module constants>
 *
 * The raw read of ``_config_version`` -- deliberately not the deep-merged
 * ``load_config()``, which would make an unstamped file inherit the latest
 * version and hide that its schema was never migrated.  Reading config.yaml is
 * a seam: the parsed document, its existence and whether it parsed arrive as
 * values, and only what those values decide is ported here.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_configvers_b21168.h"

static const char *const CONFIGVERS_ERROR = "InvalidUserConfigError";
static const char *const CONFIGVERS_VERSION_KEY = "_config_version";

static int cv_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

static int cv_isspace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* int(text): optional surrounding ASCII whitespace, optional sign, digits only. */
static int cv_parse_int_text(const char *s, long long *out) {
    const char *p = s;
    int neg = 0;
    long long value = 0;
    int digits = 0;
    if (!s) return 0;
    while (*p && cv_isspace(*p)) p++;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    while (*p >= '0' && *p <= '9') {
        if (value > 922337203685477580LL) return 0; /* overflow would raise */
        value = value * 10 + (*p - '0');
        digits++;
        p++;
    }
    if (!digits) return 0;
    while (*p && cv_isspace(*p)) p++;
    if (*p) return 0;
    *out = neg ? -value : value;
    return 1;
}

/* _coerce_config_version: a bool is legacy, int() failures are legacy, and the
 * result never goes below zero. */
static long long cv_coerce_ll(const json_t *v) {
    long long value = 0;
    if (!v || v->type == JSON_BOOL) return 0;
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (isnan(d) || isinf(d)) return 0;      /* int(nan/inf) raises */
        if (d >= 9.2e18 || d <= -9.2e18) return 0;
        value = (long long)d;                    /* truncates toward zero */
    } else if (v->type == JSON_STRING) {
        if (!cv_parse_int_text(v->str_val, &value)) return 0;
    } else {
        return 0;                                /* None / list / dict -> TypeError */
    }
    return value < 0 ? 0 : value;
}

json_t *w26_configvers_b21168_coerce(const json_t *value) {
    return json_number((double)cv_coerce_ll(value));
}

/* The shared (stamp, latest) decision behind both readers.  Returns 0 when the
 * caller asked to raise and the document is unusable. */
static int cv_read(const json_t *state, int *have_stamp, long long *stamp) {
    int exists, parse_error, raise_on_error;
    const json_t *doc;
    *have_stamp = 0;
    *stamp = 0;
    exists = cv_truthy(state ? json_obj_get(state, "exists") : NULL);
    parse_error = cv_truthy(state ? json_obj_get(state, "parse_error") : NULL);
    raise_on_error = cv_truthy(state ? json_obj_get(state, "raise") : NULL);
    if (!exists) { *have_stamp = 1; *stamp = W26_CONFIGVERS_B21168_LATEST; return 1; }
    if (parse_error) {
        if (raise_on_error) return 0;
        *have_stamp = 1; *stamp = W26_CONFIGVERS_B21168_LATEST;
        return 1;
    }
    doc = state ? json_obj_get(state, "doc") : NULL;
    if (doc && doc->type == JSON_NULL) doc = NULL;   /* empty file / bare doc -> {} */
    if (doc && doc->type != JSON_OBJECT) {
        if (raise_on_error) return 0;
        doc = NULL;
    }
    if (!doc || !json_obj_get(doc, CONFIGVERS_VERSION_KEY)) {
        /* "_config_version" not in config -> (None, latest) */
        *have_stamp = 0;
        return 1;
    }
    *have_stamp = 1;
    *stamp = cv_coerce_ll(json_obj_get(doc, CONFIGVERS_VERSION_KEY));
    return 1;
}

json_t *w26_configvers_b21168_read_stamp(const json_t *state) {
    int have_stamp;
    long long stamp;
    json_t *out;
    if (!cv_read(state, &have_stamp, &stamp)) {
        out = json_object();
        json_set(out, "error", json_string(CONFIGVERS_ERROR));
        return out;
    }
    out = json_array();
    json_append(out, have_stamp ? json_number((double)stamp) : json_null());
    json_append(out, json_number((double)W26_CONFIGVERS_B21168_LATEST));
    return out;
}

json_t *w26_configvers_b21168_check_version(const json_t *state) {
    int have_stamp;
    long long stamp;
    json_t *out;
    if (!cv_read(state, &have_stamp, &stamp)) {
        out = json_object();
        json_set(out, "error", json_string(CONFIGVERS_ERROR));
        return out;
    }
    out = json_array();
    json_append(out, json_number(have_stamp ? (double)stamp : 0.0));
    json_append(out, json_number((double)W26_CONFIGVERS_B21168_LATEST));
    return out;
}

json_t *w26_configvers_b21168_newer_message(int stamp) {
    char buf[512];
    snprintf(buf, sizeof(buf),
             "config.yaml is at version %d, written by a newer Hermes (this build knows up to "
             "%d). No migration runs on it, so this build does not rewrite settings the newer one "
             "wrote. Update Hermes to migrate it.",
             stamp, W26_CONFIGVERS_B21168_LATEST);
    return json_string(buf);
}

json_t *w26_configvers_b21168_constants(void) {
    json_t *o = json_object();
    json_set(o, "latest_version", json_number((double)W26_CONFIGVERS_B21168_LATEST));
    json_set(o, "legacy_fallback_version", json_number(1));
    json_set(o, "version_key", json_string(CONFIGVERS_VERSION_KEY));
    json_set(o, "error_class", json_string(CONFIGVERS_ERROR));
    return o;
}
