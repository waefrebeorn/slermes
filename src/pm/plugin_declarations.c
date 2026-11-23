/*
 * plugin_declarations.c — C home for upstream Hermes pm/plugin_declarations.py.
 * PoP: w8_plugindecla_451f92_manifest_version_error @ pm/plugin_declarations.py:manifest_version_error
 * PoP: w8_plugindecla_451f92_declaration_external @ pm/plugin_declarations.py:PythonDeclaration.external
 */
#include "noc_w8_plugindecla_451f92.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        default:          return v->c.count ? 1 : 0;
    }
}
static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("None");
    switch (v->type) {
        case JSON_STRING: return dupstr(v->str_val ? v->str_val : "");
        case JSON_BOOL:   return dupstr(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            char b[64];
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(b, sizeof(b), "%.0f", v->num_val);
            else
                snprintf(b, sizeof(b), "%.17g", v->num_val);
            return dupstr(b);
        }
        case JSON_ARRAY:  return dupstr("[]");
        case JSON_OBJECT: return dupstr("{}");
        default:          return dupstr("None");
    }
}
/* Python int(v): accept decimal strings (with surrounding whitespace/sign),
 * numbers (truncate toward zero) and bools; reject anything else. */
static int py_int(const json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    if (v->type == JSON_BOOL)   { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        while (isspace((unsigned char)*s)) s++;
        if (!*s) return 0;
        char *end = NULL;
        long long n = strtoll(s, &end, 10);
        if (!end || end == s) return 0;
        while (isspace((unsigned char)*end)) end++;
        if (*end != '\0') return 0;
        *out = n;
        return 1;
    }
    return 0;
}

struct json_t *w8_plugindecla_451f92_manifest_version_error(const char *name,
                                                          const struct json_t *manifest,
                                                          const struct json_t *requires_error) {
    if (!name) name = "";
    if (requires_error && requires_error->type == JSON_STRING
            && requires_error->str_val && requires_error->str_val[0]) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "Plugin '%s' %s", name, requires_error->str_val);
        return json_string(buf);
    }
    const json_t *v = manifest ? json_obj_get(manifest, "manifest_version") : NULL;
    if (!v || v->type == JSON_NULL) return json_null();
    long long parsed;
    char buf[1024];
    if (!py_int(v, &parsed)) {
        char *s = py_str(v);
        snprintf(buf, sizeof(buf),
                 "Plugin '%s' has invalid manifest_version '%s' (expected an integer).",
                 name, s ? s : "");
        free(s);
        return json_string(buf);
    }
    if (parsed > 2) {
        char *s = py_str(v);
        snprintf(buf, sizeof(buf),
                 "Plugin '%s' requires manifest_version %s, but this installer only supports up to 2.",
                 name, s ? s : "");
        free(s);
        return json_string(buf);
    }
    return json_null();
}

int w8_plugindecla_451f92_declaration_external(const struct json_t *manifest) {
    const json_t *v = manifest ? json_obj_get(manifest, "python_runtime") : NULL;
    if (!truthy(v)) return 0;
    char *s = py_str(v);
    size_t a = 0, b = strlen(s);
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    char low[32];
    size_t n = b - a;
    if (n >= sizeof(low)) { free(s); return 0; }
    for (size_t i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)s[a + i]);
    low[n] = '\0';
    free(s);
    return strcmp(low, "external") == 0 ? 1 : 0;
}
