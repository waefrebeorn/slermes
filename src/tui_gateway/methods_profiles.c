/*
 * methods_profiles.c - C home for upstream Hermes tui_gateway/methods_profiles.py.
 * PoP: w16_methodspro_1e5967_model_provider_params @ tui_gateway/methods_profiles.py:_model_provider_params
 * PoP: w16_methodspro_1e5967_clean_revisions @ tui_gateway/methods_profiles.py:_clean_revisions
 * PoP: w16_methodspro_1e5967_clean_names @ tui_gateway/methods_profiles.py:_clean_names
 * PoP: w16_methodspro_1e5967_bots_title @ tui_gateway/methods_profiles.py:_bots_title
 *
 * Faithful ports of the module's pure request/name/revision helpers. The
 * registered profile RPCs own live config/DB state and stay with live Python.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_methodspro_1e5967.h"

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}
static json_t *jcopy(const json_t *v) { return v ? json_copy(v) : json_null(); }

static int is_falsy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 1;
    switch (v->type) {
        case JSON_BOOL:   return !v->bool_val;
        case JSON_NUMBER: return v->num_val == 0.0;
        case JSON_STRING: return !v->str_val || !v->str_val[0];
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count == 0;
        default:          return 1;
    }
}
/* Python str() for the scalar kinds used by these helpers. */
static char *py_str(const json_t *v) {
    char buf[64];
    if (!v) return dupstr("None");
    switch (v->type) {
        case JSON_NULL:   return dupstr("None");
        case JSON_STRING: return dupstr(v->str_val);
        case JSON_BOOL:   return dupstr(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
            else
                snprintf(buf, sizeof(buf), "%.17g", v->num_val);
            return dupstr(buf);
        default: return dupstr("");
    }
}
/* str(params.get(k) or "") */
static char *str_or_empty(const json_t *v) {
    return is_falsy(v) ? dupstr("") : py_str(v);
}
/* ASCII whitespace strip in place; returns the ORIGINAL allocation start. */
static char *strip(char *s) {
    if (!s) return s;
    size_t n = strlen(s), lead = 0;
    while (lead < n && isspace((unsigned char)s[lead])) lead++;
    if (lead) memmove(s, s + lead, n - lead + 1);
    n -= lead;
    while (n && isspace((unsigned char)s[n - 1])) n--;
    s[n] = '\0';
    return s;
}
static int cmp_strptr(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

struct json_t *w16_methodspro_1e5967_model_provider_params(const struct json_t *params) {
    char *m = strip(str_or_empty(json_obj_get(params, "model")));
    char *p = strip(str_or_empty(json_obj_get(params, "provider")));
    json_t *out = json_array();
    json_append(out, json_string(m ? m : ""));
    json_append(out, json_string(p ? p : ""));
    free(m);
    free(p);
    return out;
}

struct json_t *w16_methodspro_1e5967_clean_revisions(const struct json_t *raw) {
    json_t *out = json_object();
    size_t n = json_object_size(raw);
    for (size_t i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(raw, i);
        json_t *v = json_object_get_at(raw, i);
        /* isinstance(v, int) and not isinstance(v, bool): integral JSON numbers only */
        if (k && v && v->type == JSON_NUMBER &&
                v->num_val == (double)(long long)v->num_val) {
            long long iv = (long long)v->num_val;   /* max(0, int(v)) */
            if (iv < 0) iv = 0;
            json_set(out, k, json_number((double)iv));
        }
    }
    return out;
}

struct json_t *w16_methodspro_1e5967_clean_names(const struct json_t *values) {
    char **items = NULL;
    size_t cnt = 0, cap = 0;
    json_t *out = json_array();
    size_t n = (values && values->type == JSON_ARRAY) ? values->c.count : 0;
    for (size_t i = 0; i < n; i++) {
        char *s = strip(py_str(json_get(values, i)));
        if (s && *s) {
            if (cnt == cap) {
                cap = cap ? cap * 2 : 8;
                char **tmp = (char **)realloc(items, cap * sizeof(char *));
                if (!tmp) { free(s); break; }
                items = tmp;
            }
            items[cnt++] = s;
        } else {
            free(s);
        }
    }
    if (cnt > 1) qsort(items, cnt, sizeof(char *), cmp_strptr);
    for (size_t i = 0; i < cnt; i++) {
        if (i == 0 || strcmp(items[i], items[i - 1]) != 0)
            json_append(out, json_string(items[i]));
        free(items[i]);
    }
    free(items);
    return out;
}

struct json_t *w16_methodspro_1e5967_bots_title(const struct json_t *ui_meta) {
    json_t *bots = json_obj_get(ui_meta, "hermes-bots");
    return (bots && bots->type == JSON_OBJECT)
        ? jcopy(json_obj_get(bots, "title")) : json_null();
}
