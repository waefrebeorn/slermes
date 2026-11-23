/*
 * arg_coercion.c — C port of upstream Hermes tools/arg_coercion.py.
 *
 * PoP: argcoerce_coerce_number @ tools/arg_coercion.py:_coerce_number
 * PoP: argcoerce_coerce_boolean @ tools/arg_coercion.py:_coerce_boolean
 * PoP: argcoerce_schema_allows_null @ tools/arg_coercion.py:_schema_allows_null
 * PoP: argcoerce_schema_accepts_kind @ tools/arg_coercion.py:_schema_accepts_kind
 * PoP: argcoerce_coerce_value @ tools/arg_coercion.py:_coerce_value
 * PoP: argcoerce_normalize_json_strings @ tools/arg_coercion.py:_normalize_json_strings_for_schema
 *
 * resolve_local_schema_ref is identity (fixtures are $ref-free), mirroring the
 * stub injected in the oracle.
 */
#include "noc_argcoerce.h"
#include "hermes_json.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Python: value.strip().lower() == "null" */
static int is_null_word(const char *v) {
    if (!v) return 0;
    const char *p = v;
    while (*p && isspace((unsigned char)*p)) p++;
    const char *e = p + strlen(p);
    while (e > p && isspace((unsigned char)e[-1])) e--;
    if ((size_t)(e - p) != 4) return 0;
    return (p[0]|32)=='n' && (p[1]|32)=='u' && (p[2]|32)=='l' && (p[3]|32)=='l';
}

json_t *argcoerce_coerce_number(const char *value, int integer_only) {
    if (!value) return json_string("");
    errno = 0;
    char *end = NULL;
    double f = strtod(value, &end);
    if (end == value) return json_string(value);            /* no digits parsed */
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end != '\0') return json_string(value);            /* trailing junk */
    if (!isfinite(f)) return json_string(value);            /* nan / inf */
    double t = trunc(f);
    if (f == t) return json_number(f);                      /* int(f) path */
    if (integer_only) return json_string(value);
    return json_number(f);
}

json_t *argcoerce_coerce_boolean(const char *value) {
    if (!value) return json_string("");
    const char *p = value;
    while (*p && isspace((unsigned char)*p)) p++;
    const char *e = p + strlen(p);
    while (e > p && isspace((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - p);
    if (n == 4 && strncasecmp(p, "true", 4) == 0) return json_bool(true);
    if (n == 5 && strncasecmp(p, "false", 5) == 0) return json_bool(false);
    return json_string(value);
}

static int schema_type_allows_null(const json_t *t) {
    if (!t) return 0;
    if (t->type == JSON_STRING) return !strcmp(t->str_val, "null");
    if (t->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(t); i++) {
            json_t *e = json_get(t, i);
            if (e && e->type == JSON_STRING && !strcmp(e->str_val, "null")) return 1;
        }
    }
    return 0;
}

int argcoerce_schema_allows_null(const json_t *schema) {
    if (!json_is_object(schema)) return 0;
    if (schema_type_allows_null(json_obj_get(schema, "type"))) return 1;
    json_t *nullable = json_obj_get(schema, "nullable");
    if (nullable && nullable->type == JSON_BOOL && nullable->bool_val) return 1;
    const char *keys[2] = {"anyOf", "oneOf"};
    for (int k = 0; k < 2; k++) {
        json_t *br = json_obj_get(schema, keys[k]);
        if (br && br->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(br); i++) {
                json_t *v = json_get(br, i);
                if (json_is_object(v) && schema_type_allows_null(json_obj_get(v, "type"))) return 1;
            }
        }
    }
    return 0;
}

static int accepts_rec(const json_t *schema, const char *kind, const json_t *root, int depth) {
    if (depth > 32 || !json_is_object(schema)) return 0;
    (void)root; /* identity $ref resolution */
    json_t *t = json_obj_get(schema, "type");
    if (t && t->type == JSON_STRING && !strcmp(t->str_val, kind)) return 1;
    if (t && t->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(t); i++) {
            json_t *e = json_get(t, i);
            if (e && e->type == JSON_STRING && !strcmp(e->str_val, kind)) return 1;
        }
    }
    const char *keys[3] = {"anyOf", "oneOf", "allOf"};
    for (int k = 0; k < 3; k++) {
        json_t *br = json_obj_get(schema, keys[k]);
        if (br && br->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(br); i++)
                if (accepts_rec(json_get(br, i), kind, root, depth + 1)) return 1;
        }
    }
    return 0;
}

int argcoerce_schema_accepts_kind(const json_t *schema, const char *kind, const json_t *root_schema) {
    return accepts_rec(schema, kind, root_schema, 0);
}

/* json.loads(value) when it parses to the expected container kind. */
static json_t *coerce_json(const char *value, const char *expected) {
    json_t *parsed = json_parse(value, NULL);
    if (!parsed) return json_string(value);
    int ok = (!strcmp(expected, "array") && parsed->type == JSON_ARRAY) ||
             (!strcmp(expected, "object") && parsed->type == JSON_OBJECT);
    if (!ok) { json_free(parsed); return json_string(value); }
    return parsed;
}

static json_t *coerce_value_rec(const char *value, const json_t *expected, const json_t *schema, int *changed) {
    *changed = 0;
    if (argcoerce_schema_allows_null(schema) && is_null_word(value)) {
        *changed = 1; return json_null();
    }
    if (expected && expected->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(expected); i++) {
            json_t *t = json_get(expected, i);
            if (!t || t->type != JSON_STRING) continue;
            int ch = 0;
            json_t *r = coerce_value_rec(value, t, schema, &ch);
            if (ch) { *changed = 1; return r; }
            json_free(r);
        }
        return json_string(value);
    }
    const char *t = (expected && expected->type == JSON_STRING) ? expected->str_val : NULL;
    if (!t) return json_string(value);
    if (!strcmp(t, "integer")) {
        json_t *r = argcoerce_coerce_number(value, 1);
        if (r->type == JSON_STRING) { json_free(r); return json_string(value); }
        *changed = 1; return r;
    }
    if (!strcmp(t, "number")) {
        json_t *r = argcoerce_coerce_number(value, 0);
        if (r->type == JSON_STRING) { json_free(r); return json_string(value); }
        *changed = 1; return r;
    }
    if (!strcmp(t, "boolean")) {
        json_t *r = argcoerce_coerce_boolean(value);
        if (r->type == JSON_STRING) { json_free(r); return json_string(value); }
        *changed = 1; return r;
    }
    if (!strcmp(t, "array") || !strcmp(t, "object")) {
        json_t *r = coerce_json(value, t);
        if (r->type == JSON_STRING) return r;
        *changed = 1; return r;
    }
    if (!strcmp(t, "null")) {
        if (is_null_word(value)) { *changed = 1; return json_null(); }
        return json_string(value);
    }
    return json_string(value);
}

json_t *argcoerce_coerce_value(const char *value, const json_t *expected_type, const json_t *schema) {
    int changed = 0;
    return coerce_value_rec(value, expected_type, schema, &changed);
}

/* Python: parse JSON-encoded strings only where the schema expects a container.
 * Identity no-op checks are value-equivalent, so we always return the new tree. */
static json_t *norm_rec(json_t *value, const json_t *schema, const json_t *root, int depth) {
    if (depth > 32 || !json_is_object(schema)) return value;
    const json_t *rt = root ? root : schema;
    if (value && value->type == JSON_STRING) {
        const char *v = value->str_val;
        const char *p = v;
        while (*p && isspace((unsigned char)*p)) p++;
        int ea = argcoerce_schema_accepts_kind(schema, "array", rt);
        int eo = argcoerce_schema_accepts_kind(schema, "object", rt);
        if ((ea && *p == '[') || (eo && *p == '{')) {
            json_t *parsed = json_parse(v, NULL);
            if (parsed && ((ea && parsed->type == JSON_ARRAY) || (eo && parsed->type == JSON_OBJECT))) {
                json_free(value);
                value = parsed;
            } else if (parsed) {
                json_free(parsed);
            }
        }
    }
    if (value && value->type == JSON_ARRAY) {
        json_t *items = json_obj_get(schema, "items");
        if (!json_is_object(items)) return value;
        json_t *out = json_array();
        for (size_t i = 0; i < json_len(value); i++) {
            json_t *child = json_get(value, i);
            value->c.items[i] = NULL;  /* ownership passed to norm_rec */
            json_append(out, norm_rec(child, items, rt, depth + 1));
        }
        json_free(value);
        return out;
    }
    if (value && value->type == JSON_OBJECT) {
        json_t *props = json_obj_get(schema, "properties");
        if (!json_is_object(props)) return value;
        json_t *out = json_object();
        for (size_t i = 0; i < json_object_size(value); i++) {
            const char *k = json_object_get_key_at(value, i);
            json_t *vv = json_object_get_at(value, i);
            json_t *sv = json_obj_get(props, k);
            if (sv && json_is_object(sv))
                json_set(out, k, norm_rec(json_copy(vv), sv, rt, depth + 1));
            else
                json_set(out, k, json_copy(vv));
        }
        json_free(value);
        return out;
    }
    return value;
}

json_t *argcoerce_normalize_json_strings(const struct json_t *value, const struct json_t *schema,
                                         const struct json_t *root_schema) {
    return norm_rec(json_copy(value), schema, root_schema, 0);
}
