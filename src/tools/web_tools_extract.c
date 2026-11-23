/*
 * web_tools_extract.c — faithful C11 port of tools/web_tools_extract.py.
 * Gate w10_webtoolse_7dbad1.
 *
 * PoP: w10_webtoolse_7dbad1_web_extract_url @ tools/web_tools_extract.py:_web_extract_url
 * PoP: w10_webtoolse_7dbad1_disabled_plugin_error @ tools/web_tools_extract.py:_disabled_plugin_error
 * PoP: w10_webtoolse_7dbad1_result_entry @ tools/web_tools_extract.py:_result_entry
 * PoP: w10_webtoolse_7dbad1_extract_error_json @ tools/web_tools_extract.py:_extract_error_json
 * PoP: w10_webtoolse_7dbad1_merge_in_order @ tools/web_tools_extract.py:_merge_in_order
 */
#include "hermes_json.h"
#include "noc_w10_webtoolse_7dbad1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NO_RESULT_ERROR "Extract backend returned no result for this URL"

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}

static char *dupn(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n && s) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static char *py_strip(const char *s) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && (s[a]==' '||s[a]=='\t'||s[a]=='\n'||s[a]=='\r'||s[a]=='\v'||s[a]=='\f')) a++;
    while (b > a && (s[b-1]==' '||s[b-1]=='\t'||s[b-1]=='\n'||s[b-1]=='\r'||s[b-1]=='\v'||s[b-1]=='\f')) b--;
    return dupn(s + a, b - a);
}

char *w10_webtoolse_7dbad1_web_extract_url(const json_t *value) {
    const json_t *v = value;
    if (v && v->type == JSON_OBJECT) {
        const json_t *u = json_obj_get(v, "url");
        v = py_truthy(u) ? u : json_obj_get(v, "href");
    }
    if (!v || v->type != JSON_STRING) return NULL;
    char *s = py_strip(v->str_val ? v->str_val : "");
    if (!s || !*s) { free(s); return NULL; }
    return s;
}

char *w10_webtoolse_7dbad1_disabled_plugin_error(const char *capability, const char *disabled_key) {
    if (!capability) capability = "";
    if (!disabled_key) disabled_key = "";
    const char *slash = strchr(disabled_key, '/');
    const char *vendor = slash ? slash + 1 : disabled_key;
    size_t cap = strlen(disabled_key) * 2 + strlen(capability) * 2 + strlen(vendor) * 2 + 256;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    snprintf(out, cap,
        "web.%s_backend is set to '%s', but its plugin ('%s') is disabled in config. "
        "Re-enable it with `hermes plugins enable %s` (or remove it from plugins.disabled).",
        capability, vendor, disabled_key, disabled_key);
    return out;
}

json_t *w10_webtoolse_7dbad1_result_entry(const char *url, const json_t *error) {
    json_t *o = json_object();
    json_set_hdr(o, "url", url ? url : "");
    json_set_hdr(o, "title", "");
    json_set_hdr(o, "content", "");
    json_set(o, "error", error ? json_copy(error) : json_null());
    return o;
}

char *w10_webtoolse_7dbad1_extract_error_json(const char *error) {
    if (!error) error = "";
    char *esc = json_escape_string(error);
    if (!esc) return NULL;
    size_t n = strlen(esc) + strlen("{\"success\": false, \"error\": }") + 4;
    char *out = (char *)malloc(n);
    if (!out) { free(esc); return NULL; }
    snprintf(out, n, "{\"success\": false, \"error\": %s}", esc);
    free(esc);
    return out;
}

json_t *w10_webtoolse_7dbad1_merge_in_order(long total, const json_t *fixed,
        const json_t *fetch_positions, const json_t *fetch_urls, const json_t *results) {
    long cap = total;
    size_t nf = fetch_positions ? json_len(fetch_positions) : 0;
    for (size_t i = 0; i < nf; i++) {
        long p = (long)json_number_value(json_get(fetch_positions, i));
        if (p >= 0 && p + 1 > cap) cap = p + 1;
    }
    json_t **merged = (json_t **)calloc((size_t)(cap > 0 ? cap : 1), sizeof(json_t *));
    if (fixed && fixed->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(fixed); i++) {
            long k = strtol(json_object_get_key_at(fixed, i), NULL, 10);
            if (k >= 0 && k < cap) merged[k] = json_copy(json_object_get_at(fixed, i));
        }
    }
    size_t nu = fetch_urls ? json_len(fetch_urls) : 0;
    size_t nr = results ? json_len(results) : 0;
    for (size_t pos = 0; pos < nf; pos++) {
        long p = (long)json_number_value(json_get(fetch_positions, pos));
        if (p < 0 || p >= cap) continue;
        json_t *val;
        if ((long)pos < (long)nr) {
            val = json_copy(json_get(results, pos));
        } else {
            const char *u = (pos < nu && json_get(fetch_urls, pos)->type == JSON_STRING)
                            ? json_string_value(json_get(fetch_urls, pos)) : "";
            val = w10_webtoolse_7dbad1_result_entry(u, json_string(NO_RESULT_ERROR));
        }
        json_free(merged[p]);
        merged[p] = val;
    }
    json_t *out = json_array();
    for (long i = 0; i < total; i++) json_append(out, merged[i] ? json_copy(merged[i]) : json_null());
    for (long i = 0; i < cap; i++) json_free(merged[i]);
    free(merged);
    return out;
}
