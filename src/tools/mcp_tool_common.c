/*
 * mcp_tool_common.c — C home for upstream Hermes tools/mcp_tool_common.py.
 *
 * PoP: w8_mcptoolcom_f3c09b_env_ref_name @ tools/mcp_tool_common.py:_env_ref_name
 * PoP: w8_mcptoolcom_f3c09b_safe_numeric @ tools/mcp_tool_common.py:_safe_numeric
 * PoP: w8_mcptoolcom_f3c09b_parse_boolish @ tools/mcp_tool_common.py:_parse_boolish
 * PoP: w8_mcptoolcom_f3c09b_mcp_server_enabled @ tools/mcp_tool_common.py:mcp_server_enabled
 * PoP: w8_mcptoolcom_f3c09b_get_lifecycle_seconds @ tools/mcp_tool_common.py:_get_lifecycle_seconds
 * PoP: w8_mcptoolcom_f3c09b_prepend_path @ tools/mcp_tool_common.py:_prepend_path
 */
#include "noc_w8_mcptoolcom_f3c09b.h"
#include "hermes_json.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* Python str.strip() over an ASCII/UTF-8 byte string (whitespace is ASCII). */
static char *strip_ws(const char *s) {
    if (!s) return NULL;
    const unsigned char *a = (const unsigned char *)s;
    while (*a && is_ws(*a)) a++;
    const unsigned char *b = a + strlen((const char *)a);
    while (b > a && is_ws(b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, a, n); out[n] = '\0'; }
    return out;
}

static char *lower_copy(const char *s) {
    if (!s) return NULL;
    char *out = (char *)malloc(strlen(s) + 1);
    if (out) {
        size_t i;
        for (i = 0; s[i]; ++i) out[i] = (char)tolower((unsigned char)s[i]);
        out[i] = '\0';
    }
    return out;
}

char *w8_mcptoolcom_f3c09b_env_ref_name(const char *ref) {
    char *s = strip_ws(ref);
    if (!s) return NULL;
    if (!strncmp(s, "env:", 4)) {
        char *rest = strip_ws(s + 4);
        free(s);
        return rest;
    }
    return s;
}

static int coerce_int(const struct json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isfinite(d)) return 0;
        *out = (long long)d;   /* Python int(2.9) == 2 (truncate toward zero) */
        return 1;
    }
    if (v->type == JSON_STRING) {
        char *end = NULL;
        errno = 0;
        long long r = strtoll(v->str_val, &end, 10);
        if (end == v->str_val || errno == ERANGE) return 0;
        while (*end && is_ws((unsigned char)*end)) end++;
        if (*end) return 0;
        *out = r;
        return 1;
    }
    return 0;
}

static int coerce_double(const struct json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_STRING) {
        char *end = NULL;
        errno = 0;
        double r = strtod(v->str_val, &end);
        if (end == v->str_val || errno == ERANGE) return 0;
        while (*end && is_ws((unsigned char)*end)) end++;
        if (*end) return 0;
        *out = r;
        return 1;
    }
    return 0;
}

struct json_t *w8_mcptoolcom_f3c09b_safe_numeric(const struct json_t *value,
        double def, int coerce_int_flag, double minimum) {
    double result;
    if (coerce_int_flag) {
        long long iv;
        if (!coerce_int(value, &iv)) return json_number(def);
        result = (double)iv;
    } else {
        if (!coerce_double(value, &result)) return json_number(def);
        if (!isfinite(result)) return json_number(def);
    }
    if (result < minimum) result = minimum;
    return json_number(result);
}

int w8_mcptoolcom_f3c09b_parse_boolish(const struct json_t *value, int def) {
    if (!value || value->type == JSON_NULL) return def ? 1 : 0;
    if (value->type == JSON_BOOL) return value->bool_val ? 1 : 0;
    if (value->type == JSON_NUMBER) return value->num_val != 0.0 ? 1 : 0;
    if (value->type == JSON_STRING) {
        char *s = strip_ws(value->str_val);
        char *l = lower_copy(s ? s : "");
        int r = def ? 1 : 0;
        if (l && (!strcmp(l, "true") || !strcmp(l, "1") || !strcmp(l, "yes") || !strcmp(l, "on"))) r = 1;
        else if (l && (!strcmp(l, "false") || !strcmp(l, "0") || !strcmp(l, "no") || !strcmp(l, "off"))) r = 0;
        free(s); free(l);
        return r;
    }
    return def ? 1 : 0;
}

int w8_mcptoolcom_f3c09b_mcp_server_enabled(const struct json_t *cfg) {
    const struct json_t *en = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, "enabled") : NULL;
    const struct json_t *dis = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, "disabled") : NULL;
    int enabled = en ? w8_mcptoolcom_f3c09b_parse_boolish(en, 1)
                     : w8_mcptoolcom_f3c09b_parse_boolish(NULL, 1);
    int disabled = dis ? w8_mcptoolcom_f3c09b_parse_boolish(dis, 0)
                       : w8_mcptoolcom_f3c09b_parse_boolish(NULL, 0);
    return enabled && !disabled;
}

struct json_t *w8_mcptoolcom_f3c09b_get_lifecycle_seconds(const struct json_t *cfg,
        const char *key) {
    if (!cfg || cfg->type != JSON_OBJECT) return json_null();
    const struct json_t *raw = json_obj_get(cfg, key);
    if (!raw) {
        const struct json_t *life = json_obj_get(cfg, "lifecycle");
        if (life && life->type == JSON_OBJECT) raw = json_obj_get(life, key);
    }
    if (!raw || raw->type == JSON_NULL) return json_null();
    double seconds;
    if (!coerce_double(raw, &seconds)) return json_null();
    if (seconds < 0) return json_null();
    if (seconds == 0.0) return json_null();   /* `seconds or None` */
    return json_number(seconds);
}

struct json_t *w8_mcptoolcom_f3c09b_prepend_path(const struct json_t *env,
        const char *directory) {
    struct json_t *out = env && env->type == JSON_OBJECT ? json_copy(env) : json_object();
    if (!directory || !*directory) return out;
    const char *cur = json_get_str(out, "PATH", "");
    /* Split on os.pathsep (';' on this platform), dropping empty components. */
    size_t max_parts = strlen(cur) + 1;
    char **parts = (char **)calloc(max_parts ? max_parts : 1, sizeof(char *));
    size_t n = 0;
    if (parts) {
        const char *p = cur;
        while (*p) {
            const char *q = strchr(p, ';');
            size_t len = q ? (size_t)(q - p) : strlen(p);
            if (len) {
                parts[n] = (char *)malloc(len + 1);
                if (parts[n]) { memcpy(parts[n], p, len); parts[n][len] = '\0'; n++; }
            }
            if (!q) break;
            p = q + 1;
        }
        int present = 0;
        for (size_t i = 0; i < n; ++i) if (!strcmp(parts[i], directory)) { present = 1; break; }
        size_t total = 0;
        if (!present) total = strlen(directory) + (n ? 1 : 0);
        for (size_t i = 0; i < n; ++i) total += strlen(parts[i]) + (i + 1 < n || !present ? 1 : 0);
        char *joined = (char *)malloc(total + 1);
        if (joined) {
            size_t at = 0;
            if (!present) { at += (size_t)sprintf(joined + at, "%s", directory); }
            for (size_t i = 0; i < n; ++i) {
                if (at && joined[at - 1] != ';') joined[at++] = ';';
                at += (size_t)sprintf(joined + at, "%s", parts[i]);
            }
            joined[at] = '\0';
            json_set(out, "PATH", json_string(joined));
            free(joined);
        }
        for (size_t i = 0; i < n; ++i) free(parts[i]);
        free(parts);
    }
    return out;
}
