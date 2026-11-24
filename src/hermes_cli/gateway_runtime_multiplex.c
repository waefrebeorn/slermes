/*
 * gateway_runtime_multiplex.c — C11 port of upstream Hermes
 *                               hermes_cli/gateway_runtime_multiplex.py
 *                               (gate w25_gatewayrun_b1c386).
 *
 * PoP: w25_gatewayrun_b1c386_normalize_name @ hermes_cli/profiles.py:normalize_profile_name
 * PoP: w25_gatewayrun_b1c386_settled_flag @ hermes_cli/gateway_multiplex_mode.py:settled_explicit_flag
 * PoP: w25_gatewayrun_b1c386_serves_home @ hermes_cli/gateway_runtime_multiplex.py:multiplexer_serves_home
 * PoP: w25_gatewayrun_b1c386_implied_serves @ hermes_cli/gateway_runtime_multiplex.py:implied_host_root
 *
 * The profile layout, config.yaml, gateway_state.json and the profile registry
 * are seams: their facts arrive as inputs (is this a named profile, the boot
 * verdict for it, the last recorded served_profiles) and the decision is what
 * is ported.  ``multiplexer_root_for``'s path arithmetic is the caller's.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_gatewayrun_b1c386.h"

static json_t *gm_error(const char *name) {
    json_t *o = json_object();
    json_set(o, "__error__", json_string(name));
    return o;
}

/* Python truthiness over a JSON value; NULL json_t* is None. */
static int gm_truthy(const json_t *v) {
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

/* Python str.strip() over ASCII whitespace. */
static char *gm_strip(const char *s) {
    size_t a = 0, b;
    char *out;
    if (!s) s = "";
    b = strlen(s);
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' ||
                     s[a] == '\r' || s[a] == '\v' || s[a] == '\f')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\n' ||
                     s[b - 1] == '\r' || s[b - 1] == '\v' || s[b - 1] == '\f')) b--;
    out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

/* str.strip().casefold() == "default" -- ASCII casefold is tolower. */
static int gm_is_default_alias(const char *stripped) {
    static const char *want = "default";
    size_t i;
    if (strlen(stripped) != strlen(want)) return 0;
    for (i = 0; stripped[i]; i++) {
        char c = stripped[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c != want[i]) return 0;
    }
    return 1;
}

static char *gm_lower(const char *s) {
    char *out = (char *)malloc(strlen(s) + 1);
    size_t i;
    if (!out) return NULL;
    for (i = 0; s[i]; i++) {
        char c = s[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    out[strlen(s)] = '\0';
    return out;
}

/* normalize_profile_name over a C string: <normalized> or NULL with *err set. */
static char *gm_normalize_str(const char *s, const char **err) {
    char *stripped, *out;
    *err = NULL;
    stripped = gm_strip(s);
    if (!stripped) { *err = "MemoryError"; return NULL; }
    if (stripped[0] == '\0') { free(stripped); *err = "ValueError"; return NULL; }
    if (gm_is_default_alias(stripped)) {
        free(stripped);
        out = gm_lower("default");
    } else {
        out = gm_lower(stripped);
        free(stripped);
    }
    if (!out) *err = "MemoryError";
    return out;
}

/* normalize_profile_name as a value: <normalized> or NULL with *err set. */
static char *gm_normalize(const json_t *name, const char **err) {
    *err = NULL;
    if (!name || name->type != JSON_STRING) {
        *err = "ValueError";
        return NULL;
    }
    return gm_normalize_str(name->str_val ? name->str_val : "", err);
}

/* str(value) for the shapes a served_profiles entry can hold. */
static char *gm_str(const json_t *v) {
    char buf[64];
    if (!v) return strdup("None");
    switch (v->type) {
    case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
    case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return strdup(buf);
    default: return strdup("None");
    }
}

json_t *w25_gatewayrun_b1c386_normalize_name(const json_t *name) {
    const char *err;
    char *norm = gm_normalize(name, &err);
    if (!norm) return gm_error(err ? err : "MemoryError");
    json_t *out = json_string(norm);
    free(norm);
    return out;
}

json_t *w25_gatewayrun_b1c386_settled_flag(const json_t *value) {
    return gm_truthy(value) ? json_bool(1) : json_null();
}

json_t *w25_gatewayrun_b1c386_serves_home(const json_t *named,
                                          const json_t *verdict,
                                          const json_t *served,
                                          const json_t *home_name) {
    int have_verdict = verdict && verdict->type != JSON_NULL;
    const char *err;
    char *home_norm;
    size_t i, n;
    int found = 0;

    if (!gm_truthy(named)) return json_bool(0);
    if (have_verdict) return json_bool(gm_truthy(verdict) ? 1 : 0);
    if (!served || served->type != JSON_ARRAY) return json_bool(0);
    home_norm = gm_normalize(home_name, &err);
    if (!home_norm) return gm_error(err ? err : "MemoryError");
    n = json_len(served);
    for (i = 0; i < n && !found; i++) {
        const json_t *entry = json_get(served, i);
        char *text, *norm;
        if (!gm_truthy(entry)) continue;
        text = gm_str(entry);
        if (!text) continue;
        norm = gm_normalize_str(text, &err);
        free(text);
        if (norm && !strcmp(norm, home_norm)) found = 1;
        free(norm);
    }
    free(home_norm);
    return json_bool(found ? 1 : 0);
}

json_t *w25_gatewayrun_b1c386_implied_serves(const json_t *named,
                                             const json_t *verdict) {
    /* ``host_serves_named_profile(...) is False`` -- identity, not truthiness. */
    int standalone = verdict && verdict->type == JSON_BOOL && !verdict->bool_val;
    if (!gm_truthy(named)) return json_bool(0);
    return json_bool(standalone ? 0 : 1);
}
