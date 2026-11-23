/*
 * plugins_settings.c - C home for upstream Hermes hermes_cli/plugins_settings.py.
 * PoP: w15_pluginsset_b71c40_secret_env_name @ hermes_cli/plugins_settings.py:secret_env_name
 * PoP: w15_pluginsset_b71c40_field_type @ hermes_cli/plugins_settings.py:_field_type
 * PoP: w15_pluginsset_b71c40_save_plugin_settings @ hermes_cli/plugins_settings.py:save_plugin_settings
 *
 * Pure manifest-schema helpers: the manifest, config and .env writer are injected as
 * JSON inputs, so nothing here touches the process environment or the filesystem.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_pluginsset_b71c40.h"

static char *dupstr(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static int is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* Python str() for the scalar kinds used here. */
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char b[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(b, sizeof(b), "%lld", (long long)v->num_val);
        else
            snprintf(b, sizeof(b), "%g", v->num_val);
        return dupstr(b);
    }
    char *s = json_dumps(v, 0);
    return s ? s : dupstr("");
}

/* .strip() in place; writes the NUL terminator at the NEW length. */
static char *strip_dup(const char *s) {
    const char *a = s ? s : "";
    const char *b = a + strlen(a);
    while (a < b && is_ws((unsigned char)a[0])) a++;
    while (b > a && is_ws((unsigned char)b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, a, n); out[n] = 0; }
    return out;
}

/* _ENV_NAME_CLEAN_RE = [^A-Z0-9]+ -> "_" on the uppercased name, then .strip("_"). */
static char *env_name_clean(const char *s) {
    size_t n = strlen(s);
    char *upper = (char *)malloc(n + 1);
    if (!upper) return dupstr("");
    for (size_t i = 0; i < n; i++) upper[i] = (char)toupper((unsigned char)s[i]);
    upper[n] = 0;
    char *out = (char *)malloc(n + 2);
    if (!out) { free(upper); return dupstr(""); }
    size_t m = 0;
    size_t i = 0;
    while (i < n) {
        unsigned char c = (unsigned char)upper[i];
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            out[m++] = (char)c;
            i++;
        } else {
            out[m++] = '_';
            while (i < n) {
                unsigned char d = (unsigned char)upper[i];
                if ((d >= 'A' && d <= 'Z') || (d >= '0' && d <= '9')) break;
                i++;
            }
        }
    }
    out[m] = 0;
    free(upper);
    /* .strip("_") */
    size_t a = 0, b = m;
    while (a < b && out[a] == '_') a++;
    while (b > a && out[b - 1] == '_') b--;
    size_t k = b - a;
    memmove(out, out + a, k);
    out[k] = 0;
    return out;
}

struct json_t *w15_pluginsset_b71c40_secret_env_name(const char *plugin_id, const char *key,
                                                    const struct json_t *spec) {
    json_t *env = spec && spec->type == JSON_OBJECT ? json_obj_get(spec, "env") : NULL;
    char *declared = NULL;
    if (env && env->type != JSON_NULL) {
        char *raw = py_str(env);
        declared = strip_dup(raw);
        free(raw);
    } else {
        declared = dupstr("");
    }
    if (declared[0]) {
        json_t *r = json_string(declared);
        free(declared);
        return r;
    }
    free(declared);
    size_t n = strlen(plugin_id ? plugin_id : "") + strlen(key ? key : "") + 2;
    char *joined = (char *)malloc(n);
    if (!joined) return json_string("");
    snprintf(joined, n, "%s_%s", plugin_id ? plugin_id : "", key ? key : "");
    char *clean = env_name_clean(joined);
    free(joined);
    json_t *r = json_string(clean);
    free(clean);
    return r;
}

/* manifest type -> wire field type */
static const char *field_type_of(const json_t *spec) {
    if (!spec || spec->type != JSON_OBJECT) return "string";
    json_t *secret = json_obj_get(spec, "secret");
    if (secret && secret->type == JSON_BOOL && secret->bool_val) return "secret";
    json_t *tv = json_obj_get(spec, "type");
    char *raw = py_str(tv); /* (spec.get("type") or "str").lower() */
    char *low = strip_dup(raw);
    free(raw);
    for (char *p = low; *p; p++) *p = (char)tolower((unsigned char)*p);
    const char *kind = "string";
    static const char *const str_types[] = {"str", "string", NULL};
    static const char *const num_types[] = {"int", "integer", "float", "number", NULL};
    static const char *const bool_types[] = {"bool", "boolean", NULL};
    static const char *const json_types[] = {"list", "array", "dict", "object", NULL};
    for (int i = 0; str_types[i]; i++) if (!strcmp(low, str_types[i])) { kind = "string"; break; }
    for (int i = 0; num_types[i]; i++) if (!strcmp(low, num_types[i])) { kind = "number"; break; }
    for (int i = 0; bool_types[i]; i++) if (!strcmp(low, bool_types[i])) { kind = "boolean"; break; }
    for (int i = 0; json_types[i]; i++) if (!strcmp(low, json_types[i])) { kind = "json"; break; }
    if (!strcmp(low, "secret")) kind = "string"; /* secret only via secret:True */
    free(low);
    /* choices = spec.get("choices", spec.get("enum")); string + non-empty list -> enum */
    json_t *choices = json_obj_get(spec, "choices");
    if (!choices) choices = json_obj_get(spec, "enum");
    if (!strcmp(kind, "string") && choices && choices->type == JSON_ARRAY && choices->c.count > 0)
        return "enum";
    return kind;
}

struct json_t *w15_pluginsset_b71c40_field_type(const struct json_t *spec) {
    return json_string(field_type_of(spec));
}

static const char *py_type_name(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
        case JSON_NULL: return "NoneType";
        case JSON_BOOL: return "bool";
        case JSON_NUMBER:
            return v->num_val == (double)(long long)v->num_val ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}

static int value_matches(const char *kind, const json_t *v) {
    if (!strcmp(kind, "string") || !strcmp(kind, "enum")) return v && v->type == JSON_STRING;
    if (!strcmp(kind, "number")) return v && v->type == JSON_NUMBER; /* bool is a separate JSON type */
    if (!strcmp(kind, "boolean")) return v && v->type == JSON_BOOL;
    if (!strcmp(kind, "json")) return v && (v->type == JSON_ARRAY || v->type == JSON_OBJECT);
    return 0;
}

/* repr() for the simple string keys used in error messages. */
static char *py_repr_str(const json_t *key) {
    char *s = py_str(key);
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 3);
    if (!out) { free(s); return dupstr("''"); }
    out[0] = '\'';
    memcpy(out + 1, s, n);
    out[n + 1] = '\'';
    out[n + 2] = 0;
    free(s);
    return out;
}

static json_t *value_error(const char *fmt, const char *a, const char *b, const char *c) {
    char buf[1024];
    snprintf(buf, sizeof(buf), fmt, a, b, c);
    return json_string(buf);
}

struct json_t *w15_pluginsset_b71c40_save_plugin_settings(const char *plugin_id,
                                                         const struct json_t *schema,
                                                         const struct json_t *values) {
    (void)plugin_id; /* the injected writer is a no-op in the fixture seam */
    json_t *written = json_array();
    if (!values || values->type != JSON_OBJECT) return written;
    for (size_t i = 0; i < json_object_size(values); i++) {
        const char *key = json_object_get_key_at(values, i);
        json_t *value = json_object_get_at(values, i);
        char *k = dupstr(key ? key : ""); /* copy out before any structural edit */
        json_t *spec = (schema && schema->type == JSON_OBJECT) ? json_obj_get(schema, key) : NULL;
        if (!spec) {
            char *r = py_repr_str(value ? value : NULL);
            /* Python reprs the KEY, not the value */
            free(r);
            json_t *kj = json_string(k);
            r = py_repr_str(kj);
            json_free(kj);
            json_t *err = value_error("ValueError: %s is not declared in the plugin's config_schema", r, "", "");
            free(r); free(k); json_free(written); return err;
        }
        const char *kind = field_type_of(spec);
        if (!strcmp(kind, "secret")) {
            json_t *kj = json_string(k);
            char *r = py_repr_str(kj);
            json_free(kj);
            json_t *err = value_error("ValueError: %s is a secret; it is stored in .env, not config.yaml", r, "", "");
            free(r); free(k); json_free(written); return err;
        }
        if (!value_matches(kind, value)) {
            json_t *kj = json_string(k);
            char *r = py_repr_str(kj);
            json_free(kj);
            json_t *err = value_error("ValueError: %s should be %s (got %s)", r, kind, py_type_name(value));
            free(r); free(k); json_free(written); return err;
        }
        if (!strcmp(kind, "enum")) {
            json_t *choices = json_obj_get(spec, "choices");
            if (!choices) choices = json_obj_get(spec, "enum");
            int ok = 0;
            if (choices && choices->type == JSON_ARRAY) {
                for (size_t j = 0; j < choices->c.count; j++) {
                    json_t *c = json_get(choices, j);
                    char *cs = py_str(c);
                    if (!strcmp(cs, value->str_val ? value->str_val : "")) ok = 1;
                    free(cs);
                    if (ok) break;
                }
            }
            if (!ok) {
                json_t *kj = json_string(k);
                char *r = py_repr_str(kj);
                json_free(kj);
                json_t *err = value_error("ValueError: %s must be one of the declared choices", r, "", "");
                free(r); free(k); json_free(written); return err;
            }
        }
        json_append(written, json_string(k));
        free(k);
    }
    return written;
}

