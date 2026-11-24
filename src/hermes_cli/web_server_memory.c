/*
 * web_server_memory.c — C port of upstream Hermes hermes_cli/web_server_memory.py.
 *
 * PoP: w16_webserver_1a1654_string_list @ hermes_cli/web_server_memory.py:_string_list
 * PoP: w16_webserver_1a1654_schema_field_kind @ hermes_cli/web_server_memory.py:_schema_field_kind
 * PoP: w16_webserver_1a1654_coerce_bool @ hermes_cli/web_server_memory.py:_coerce_bool
 * PoP: w16_webserver_1a1654_normalize_schema @ hermes_cli/web_server_memory.py:_normalize_memory_provider_schema
 *
 * get_config_schema() is injected as the raw_schema argument of normalize_schema.
 */
#include "noc_w16_webserver_1a1654.h"
#include "hermes_json.h"
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

static int json_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}

static char *strip_dup(const char *s) {
    if (!s) return dupstr("");
    const char *a = s;
    while (*a && isspace((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, a, n); p[n] = '\0'; }
    return p;
}

static char *lower_dup(const char *s) {
    char *p = dupstr(s);
    for (char *q = p; *q; q++) *q = (char)tolower((unsigned char)*q);
    return p;
}

static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("None");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        double d = v->num_val;
        if (d == (double)(long long)d && d > -9e15 && d < 9e15)
            snprintf(buf, sizeof(buf), "%lld", (long long)d);
        else
            snprintf(buf, sizeof(buf), "%.17g", d);
        return dupstr(buf);
    }
    return dupstr("");
}

static char *fmt_line(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return dupstr(""); }
    char *b = (char *)malloc((size_t)n + 1);
    if (!b) { va_end(ap2); return dupstr(""); }
    vsnprintf(b, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return b;
}

/* Python: s.replace("_", " ").title() */
static char *title_dup(const char *s) {
    if (!s) return dupstr("");
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (!p) return dupstr("");
    int boundary = 1;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (c == '_') c = ' ';
        if (isalpha((unsigned char)c)) {
            p[i] = boundary ? (char)toupper((unsigned char)c) : (char)tolower((unsigned char)c);
            boundary = 0;
        } else {
            p[i] = c;
            boundary = !isalpha((unsigned char)c);
        }
    }
    p[n] = '\0';
    return p;
}

struct json_t *w16_webserver_1a1654_string_list(const json_t *value) {
    struct json_t *out = json_array();
    if (!json_is_array(value)) return out;
    for (size_t i = 0; i < json_len(value); i++) {
        char *raw = py_str(json_get(value, i));
        char *s = strip_dup(raw);
        free(raw);
        if (s[0]) json_append(out, json_string(s));
        free(s);
    }
    return out;
}

struct json_t *w16_webserver_1a1654_schema_field_kind(const json_t *raw,
                                                      const json_t *choices) {
    char *ek = dupstr("");
    if (json_is_object(raw)) {
        const json_t *k = json_obj_get(raw, "kind");
        const json_t *t = json_obj_get(raw, "type");
        const json_t *hint = json_truthy(k) ? k : (json_truthy(t) ? t : NULL);
        if (hint && json_truthy(hint)) {
            char *s = py_str(hint);
            char *low = lower_dup(s);
            free(ek);
            ek = strip_dup(low);   /* str(hint).strip().lower() */
            free(low);
            free(s);
        }
    }
    const json_t *dflt = json_is_object(raw) ? json_obj_get(raw, "default") : NULL;
    int secret = json_truthy(json_is_object(raw) ? json_obj_get(raw, "secret") : NULL);
    int has_choices = json_is_array(choices) && json_len(choices) > 0;
    int is_bool = dflt && dflt->type == JSON_BOOL;
    int is_int = dflt && dflt->type == JSON_NUMBER &&
                 dflt->num_val == floor(dflt->num_val) && fabs(dflt->num_val) < 9e15;
    int is_float = dflt && dflt->type == JSON_NUMBER && !is_int;

    const char *kind = "text";
    if (secret) kind = "secret";
    else if (has_choices) kind = "select";
    else if (!strcmp(ek, "bool") || !strcmp(ek, "boolean") || is_bool) kind = "boolean";
    else if (!strcmp(ek, "int") || !strcmp(ek, "integer") || is_int) kind = "integer";
    else if (!strcmp(ek, "float") || !strcmp(ek, "number") || is_float) kind = "number";
    free(ek);
    return json_string(kind);
}

struct json_t *w16_webserver_1a1654_coerce_bool(const json_t *value,
                                                const struct json_t *def) {
    if (value && value->type == JSON_BOOL) return json_bool(value->bool_val);
    if (!value || value->type == JSON_NULL) return def ? json_copy(def) : json_bool(false);
    if (value->type == JSON_STRING && value->str_val && !value->str_val[0])
        return def ? json_copy(def) : json_bool(false);
    if (value->type == JSON_NUMBER) return json_bool(value->num_val != 0.0);
    char *s = py_str(value);
    char *low = lower_dup(s);
    char *t = strip_dup(low);
    int yes = !strcmp(t, "1") || !strcmp(t, "true") || !strcmp(t, "yes") || !strcmp(t, "on");
    int no = !strcmp(t, "0") || !strcmp(t, "false") || !strcmp(t, "no") || !strcmp(t, "off");
    free(t); free(low); free(s);
    if (yes) return json_bool(true);
    if (no) return json_bool(false);
    char *shown = py_str(value);
    char *msg = fmt_line("Invalid boolean value: %s", shown);
    struct json_t *err = json_object();
    json_set(err, "error", json_string(msg));
    free(msg); free(shown);
    return err;
}

struct json_t *w16_webserver_1a1654_normalize_schema(const json_t *raw_schema) {
    struct json_t *fields = json_array();
    if (!json_is_array(raw_schema)) return fields;
    for (size_t i = 0; i < json_len(raw_schema); i++) {
        const json_t *raw = json_get(raw_schema, i);
        if (!json_is_object(raw)) continue;
        char *key_raw = py_str(json_obj_get(raw, "key"));
        char *key = strip_dup(key_raw);
        free(key_raw);
        if (!key[0]) { free(key); continue; }

        const json_t *choices = json_obj_get(raw, "choices");
        if (!json_truthy(choices)) choices = json_obj_get(raw, "options");
        if (!json_is_array(choices)) choices = NULL;

        char *label = NULL;
        const json_t *lbl = json_obj_get(raw, "label");
        if (json_truthy(lbl)) label = py_str(lbl);
        else label = title_dup(key);
        char *kind = dupstr("");
        {
            struct json_t *k = w16_webserver_1a1654_schema_field_kind(raw, choices);
            if (k && k->type == JSON_STRING && k->str_val) { free(kind); kind = dupstr(k->str_val); }
            json_free(k);
        }
        char *desc = json_truthy(json_obj_get(raw, "description"))
                     ? py_str(json_obj_get(raw, "description")) : dupstr("");
        char *ph = json_truthy(json_obj_get(raw, "placeholder"))
                   ? py_str(json_obj_get(raw, "placeholder")) : dupstr("");
        char *url = json_truthy(json_obj_get(raw, "url"))
                    ? py_str(json_obj_get(raw, "url")) : dupstr("");
        const json_t *when = json_obj_get(raw, "when");
        const json_t *dflt = json_obj_get(raw, "default");
        const json_t *mn = json_obj_get(raw, "minimum");
        const json_t *mx = json_obj_get(raw, "maximum");
        const json_t *st = json_obj_get(raw, "step");
        const json_t *ev = json_obj_get(raw, "env_var");

        struct json_t *field = json_object();
        json_set(field, "key", json_string(key));
        json_set(field, "label", json_string(label));
        json_set(field, "kind", json_string(kind));
        json_set(field, "description", json_string(desc));
        json_set(field, "placeholder", json_string(ph));
        json_set(field, "required", json_bool(json_truthy(json_obj_get(raw, "required"))));
        json_set(field, "default", dflt ? json_copy(dflt) : json_string(""));
        struct json_t *opts = json_array();
        if (choices) {
            for (size_t j = 0; j < json_len(choices); j++) {
                char *cv = py_str(json_get(choices, j));
                struct json_t *o = json_object();
                json_set(o, "value", json_string(cv));
                json_set(o, "label", json_string(cv));
                json_set(o, "description", json_string(""));
                json_append(opts, o);
                free(cv);
            }
        }
        json_set(field, "options", opts);
        json_set(field, "url", json_string(url));
        json_set(field, "when", json_is_object(when) ? json_copy(when) : json_null());
        json_set(field, "minimum", mn ? json_copy(mn) : json_null());
        json_set(field, "maximum", mx ? json_copy(mx) : json_null());
        json_set(field, "step", st ? json_copy(st) : json_null());
        if (json_truthy(ev)) {
            char *evs = py_str(ev);
            json_set(field, "_env_key", json_string(evs));
            free(evs);
        } else json_set(field, "_env_key", json_null());
        json_append(fields, field);

        free(key); free(label); free(kind); free(desc); free(ph); free(url);
    }
    return fields;
}
