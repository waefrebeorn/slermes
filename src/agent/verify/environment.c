/*
 * environment.c - C11 port of upstream agent/verify/environment.py
 * (gate w24_environmen_cdf1db).
 *
 * PoP: w24_environmen_cdf1db_manifest_path @ agent/verify/environment.py:manifest_path
 * PoP: w24_environmen_cdf1db_recipe @ agent/verify/environment.py:load_manifest
 * PoP: w24_environmen_cdf1db_parse_manifest @ agent/verify/environment.py:load_manifest
 * PoP: w24_environmen_cdf1db_source @ agent/verify/environment.py:load_or_detect
 * PoP: w24_environmen_cdf1db_save_payload @ agent/verify/environment.py:save_manifest
 * PoP: w24_environmen_cdf1db_constants @ agent/verify/environment.py:MANIFEST_VERSION
 *
 * The manifest at ``<project>/.hermes/environment.json`` is the user-editable
 * source of truth: when present and valid it wins over fresh detection, and
 * any read/parse/shape problem degrades to fresh detection.  That degradation
 * rule, the tolerant recipe loader behind it (this module's names and grok's
 * camelCase aliases, ``or`` fallbacks, the port/readiness rules) and the
 * versioned save payload are pure functions of values.  Reading the file,
 * creating the directory and the clock are seams.
 */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_environmen_cdf1db.h"

#define ENV_RELPATH ".hermes/environment.json"

static int env_truthy(const json_t *v) {
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

/* str.strip(): the whitespace runs Python's str.strip() removes. */
static char *env_strip(const char *s) {
    const char *start = s ? s : "";
    const char *end;
    size_t len;
    char *out;
    while (*start && isspace((unsigned char)*start)) start++;
    end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) end--;
    len = (size_t)(end - start);
    out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

/* `a or b or ...`: the first truthy value among the aliases. */
static const json_t *env_first(const json_t *obj, const char *a, const char *b) {
    const json_t *v;
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    v = json_obj_get(obj, a);
    if (env_truthy(v)) return v;
    v = json_obj_get(obj, b);
    if (env_truthy(v)) return v;
    return NULL;
}

static int env_all_digits(const char *s) {
    if (!s || !*s) return 0;
    while (*s) {
        if (!isdigit((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

/* _as_strings: a string is one value, a list is filtered, anything else is (). */
static json_t *env_as_strings(const json_t *value) {
    json_t *out = json_array();
    size_t i, n;
    if (!value) return out;
    if (value->type == JSON_STRING) {
        char *text = env_strip(value->str_val);
        if (text && *text) json_append(out, json_string(text));
        free(text);
        return out;
    }
    if (value->type != JSON_ARRAY) return out;
    n = json_len(value);
    for (i = 0; i < n; i++) {
        const json_t *item = json_get(value, i);
        char *text;
        if (!item || item->type != JSON_STRING) continue;
        text = env_strip(item->str_val);
        if (text && *text) json_append(out, json_string(text));
        free(text);
    }
    return out;
}

/* Recipe.from_dict(raw).to_dict(), with None for a refused shape. */
static json_t *env_recipe_from_dict(const json_t *raw) {
    const json_t *name_v, *kind_v, *start_v, *port_v, *readiness_v;
    char *name, *kind, *start, *readiness;
    json_t *out, *port;
    double port_num = 0.0;
    int port_ok = 0;

    if (!raw || raw->type != JSON_OBJECT) return json_null();
    name_v = env_first(raw, "name", "appLabel");
    if (!name_v || name_v->type != JSON_STRING) return json_null();
    name = env_strip(name_v->str_val);
    if (!name || !*name) { free(name); return json_null(); }

    kind_v = env_first(raw, "kind", "appKind");
    kind = NULL;
    if (kind_v && kind_v->type == JSON_STRING) kind = env_strip(kind_v->str_val);
    if (!kind || !*kind) {
        free(kind);
        kind = (char *)malloc(8);
        if (kind) strcpy(kind, "unknown");
    }

    start_v = env_first(raw, "start", "startCommand");
    start = NULL;
    if (start_v && start_v->type == JSON_STRING) {
        char *trimmed = env_strip(start_v->str_val);
        if (trimmed && *trimmed) start = trimmed;
        else free(trimmed);
    }

    port_v = env_first(raw, "port", "startPort");
    if (port_v && port_v->type == JSON_STRING && port_v->str_val) {
        char *trimmed = env_strip(port_v->str_val);
        if (trimmed && env_all_digits(trimmed)) {
            port_num = strtod(trimmed, NULL);
            port_ok = 1;
        }
        free(trimmed);
    } else if (port_v && port_v->type == JSON_NUMBER) {
        port_num = port_v->num_val;
        port_ok = 1;
    } else if (port_v && port_v->type == JSON_BOOL) {
        port_num = port_v->bool_val ? 1.0 : 0.0;
        port_ok = 1;
    }
    port = (port_ok && port_num > 0.0 && port_num < 65536.0)
               ? json_number(port_num) : json_null();

    readiness_v = env_first(raw, "readinessPath", "readiness_path");
    readiness = NULL;
    /* Not stripped: the loader only tests startswith("/"). */
    if (readiness_v && readiness_v->type == JSON_STRING && readiness_v->str_val &&
        readiness_v->str_val[0] == '/') {
        readiness = (char *)malloc(strlen(readiness_v->str_val) + 1);
        if (readiness) strcpy(readiness, readiness_v->str_val);
    }
    if (!readiness) {
        readiness = (char *)malloc(2);
        if (readiness) strcpy(readiness, "/");
    }

    out = json_object();
    json_set(out, "name", json_string(name));
    json_set(out, "kind", json_string(kind ? kind : "unknown"));
    json_set(out, "bootstrap", env_as_strings(env_first(raw, "bootstrap", "installCommands")));
    json_set(out, "build", env_as_strings(env_first(raw, "build", "buildCommands")));
    json_set(out, "test", env_as_strings(env_first(raw, "test", "testCommands")));
    json_set(out, "start", start ? json_string(start) : json_null());
    json_set(out, "port", port);
    json_set(out, "readinessPath", json_string(readiness ? readiness : "/"));
    json_set(out, "evidence", env_as_strings(json_obj_get(raw, "evidence")));
    free(name);
    free(kind);
    free(start);
    free(readiness);
    return out;
}

/* manifest_recipe: the wrapped {version, recipe} shape or a bare recipe. */
static json_t *env_manifest_recipe(const json_t *manifest) {
    const json_t *recipe;
    if (!manifest || manifest->type != JSON_OBJECT) return json_null();
    recipe = json_obj_get(manifest, "recipe");
    if (!recipe) recipe = manifest;   /* manifest.get("recipe", manifest) */
    return env_recipe_from_dict(recipe);
}

json_t *w24_environmen_cdf1db_recipe(const json_t *req) {
    return env_recipe_from_dict(json_obj_get(req, "raw"));
}

json_t *w24_environmen_cdf1db_manifest_path(const json_t *req) {
    const char *root = json_get_str(req, "root", "");
    size_t len = strlen(root), i, j = 0;
    char *clean, *out;
    json_t *result;

    clean = (char *)malloc(len + 2);
    if (!clean) return json_string("");
    /* Path() drops the trailing separators and collapses repeats. */
    for (i = 0; i < len; i++) {
        if (root[i] == '/' && j > 0 && clean[j - 1] == '/') continue;
        clean[j++] = root[i];
    }
    while (j > 1 && clean[j - 1] == '/') j--;
    clean[j] = '\0';
    if (j == 0) {
        out = (char *)malloc(strlen(ENV_RELPATH) + 1);
        if (out) strcpy(out, ENV_RELPATH);
    } else if (j == 1 && clean[0] == '/') {
        out = (char *)malloc(strlen(ENV_RELPATH) + 2);
        if (out) sprintf(out, "/%s", ENV_RELPATH);
    } else {
        out = (char *)malloc(j + strlen(ENV_RELPATH) + 2);
        if (out) sprintf(out, "%s/%s", clean, ENV_RELPATH);
    }
    free(clean);
    result = json_string(out ? out : "");
    free(out);
    return result;
}

json_t *w24_environmen_cdf1db_parse_manifest(const json_t *req) {
    const json_t *text = json_obj_get(req, "text");
    const char *body;
    json_t *manifest, *result;

    if (!text || text->type != JSON_STRING || !text->str_val) return json_null();
    body = text->str_val;
    /* read_text(encoding="utf-8-sig") */
    if ((unsigned char)body[0] == 0xEF && (unsigned char)body[1] == 0xBB &&
        (unsigned char)body[2] == 0xBF) body += 3;
    manifest = json_parse(body, NULL);
    if (!manifest) return json_null();   /* ValueError includes JSONDecodeError */
    result = env_manifest_recipe(manifest);
    json_free(manifest);
    return result;
}

json_t *w24_environmen_cdf1db_source(const json_t *req) {
    return json_string(env_truthy(json_obj_get(req, "saved_present")) ? "manifest" : "detected");
}

json_t *w24_environmen_cdf1db_save_payload(const json_t *req) {
    json_t *recipe = env_recipe_from_dict(json_obj_get(req, "recipe"));
    const char *updated = json_get_str(req, "updated_at", "");
    json_t *out;

    if (!recipe || recipe->type == JSON_NULL) {
        json_free(recipe);
        out = json_object();
        json_set(out, "error", json_string("attribute"));
        return out;
    }
    out = json_object();
    json_set(out, "version", json_number(W24_ENVIRONMEN_CDF1DB_MANIFEST_VERSION));
    json_set(out, "recipe", recipe);
    json_set(out, "updatedAt", json_string(updated));
    return out;
}

json_t *w24_environmen_cdf1db_constants(void) {
    json_t *o = json_object();
    json_set(o, "manifest_version", json_number(W24_ENVIRONMEN_CDF1DB_MANIFEST_VERSION));
    json_set(o, "manifest_relpath", json_string(ENV_RELPATH));
    return o;
}
