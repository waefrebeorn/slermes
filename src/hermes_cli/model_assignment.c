/*
 * model_assignment.c — C port of upstream Hermes hermes_cli/model_assignment.py.
 * Gate: w15_modelassig_d6ff99
 *
 * PoP: w15_modelassig_d6ff99_same_endpoint @ hermes_cli/model_assignment.py:_same_endpoint
 * PoP: w15_modelassig_d6ff99_is_explicit_custom_endpoint @ hermes_cli/model_assignment.py:is_explicit_custom_endpoint
 * PoP: w15_modelassig_d6ff99_entry_points_elsewhere @ hermes_cli/model_assignment.py:entry_points_elsewhere
 *
 * Python:
 *   _same_endpoint(a, b): str(a or "").strip().rstrip("/").lower() == str(b or "").strip().rstrip("/").lower()
 *   is_explicit_custom_endpoint(provider, base_url):
 *       provider.strip().lower() in {"custom", "local"} and bool((base_url or "").strip())
 *   entry_points_elsewhere(entry, base_url):
 *       isinstance(entry, dict) and not _same_endpoint(entry.get("base_url"), base_url)
 */
#include "noc_w15_modelassig_d6ff99.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static char *strip_ws(const char *s) {
    size_t a = 0, b = s ? strlen(s) : 0;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (out) { if (b > a) memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}

static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && *v->str_val) ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
    }
    return 0;
}

/* str(value or "") for the scalar kinds exercised by the fixtures. */
static char *str_or_empty(const json_t *v) {
    if (!py_truthy(v)) return dupstr("");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char tmp[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(tmp, sizeof(tmp), "%.0f", v->num_val);
        else {
            snprintf(tmp, sizeof(tmp), "%.15g", v->num_val);
            if (strtod(tmp, NULL) != v->num_val) snprintf(tmp, sizeof(tmp), "%.17g", v->num_val);
        }
        return dupstr(tmp);
    }
    return dupstr("");
}

/* str(v or "").strip().rstrip("/").lower() */
static char *norm_endpoint(const json_t *v) {
    char *s = str_or_empty(v);
    char *t = strip_ws(s ? s : "");
    free(s);
    size_t n = strlen(t);
    while (n > 0 && t[n - 1] == '/') {
        t[n - 1] = '\0';
        n--;
    }
    for (char *p = t; *p; p++) *p = (char)tolower((unsigned char)*p);
    return t;
}

static int same_endpoint_raw(const json_t *a, const json_t *b) {
    char *x = norm_endpoint(a);
    char *y = norm_endpoint(b);
    int eq = !strcmp(x, y);
    free(x); free(y);
    return eq;
}

json_t *w15_modelassig_d6ff99_same_endpoint(const struct json_t *a, const struct json_t *b) {
    return json_bool(same_endpoint_raw(a, b) ? true : false);
}

json_t *w15_modelassig_d6ff99_is_explicit_custom_endpoint(const struct json_t *provider,
                                                          const struct json_t *base_url) {
    const char *p = (provider && provider->type == JSON_STRING) ? provider->str_val : NULL;
    char *t = strip_ws(p ? p : "");
    for (char *q = t; *q; q++) *q = (char)tolower((unsigned char)*q);
    int bare = (!strcmp(t, "custom") || !strcmp(t, "local"));
    free(t);
    char *u = strip_ws(base_url && base_url->type == JSON_STRING && base_url->str_val ? base_url->str_val : "");
    int has_url = *u != '\0';
    free(u);
    return json_bool((bare && has_url) ? true : false);
}

json_t *w15_modelassig_d6ff99_entry_points_elsewhere(const struct json_t *entry,
                                                     const struct json_t *base_url) {
    if (!json_is_object(entry)) return json_bool(false);
    json_t *other = json_obj_get(entry, "base_url");
    return json_bool(same_endpoint_raw(other, base_url) ? false : true);
}
