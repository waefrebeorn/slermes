/*
 * config_effective.c — C port of upstream Hermes hermes_cli/config_effective.py.
 * Gate: w15_configeffe_5314b2
 *
 * PoP: w15_configeffe_5314b2_effective @ hermes_cli/config_effective.py:_effective
 *
 * The live module's three collaborators are replayed faithfully here:
 *   config._expand_env_vars  -> expand_env / expand_env_str
 *   managed_scope.apply_managed_overlay -> apply_managed (uses deep_merge)
 *   config._normalize_root_model_keys -> normalize_root_model_keys
 * `env` and `managed` are injected seams from the fixture (the live oracle
 * patches the same collaborators against the injected values).
 */
#include "noc_w15_configeffe_5314b2.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static char *strip_dup(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - s);
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static int json_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default:          return 0;
    }
}

/* Python str(value) for the scalar shapes config keys actually take. */
static void py_str(const json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, cap, "None"); return; }
    if (v->type == JSON_STRING) { snprintf(buf, cap, "%s", v->str_val ? v->str_val : ""); return; }
    if (v->type == JSON_BOOL) { snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); return; }
    if (v->type == JSON_NUMBER) {
        long long i = (long long)v->num_val;
        if ((double)i == v->num_val && i >= -9007199254740992LL && i <= 9007199254740992LL)
            snprintf(buf, cap, "%lld", i);
        else
            snprintf(buf, cap, "%g", v->num_val);
        return;
    }
    char *s = json_dumps(v, 0);
    snprintf(buf, cap, "%s", s ? s : "");
    free(s);
}

/* str(v or "").strip() */
static void py_str_or_empty(const json_t *v, char *buf, size_t cap) {
    if (!json_truthy(v)) { buf[0] = '\0'; return; }
    char tmp[512];
    py_str(v, tmp, sizeof(tmp));
    char *s = strip_dup(tmp);
    snprintf(buf, cap, "%s", s ? s : "");
    free(s);
}

/* config._env_ref_lookup seam: env is a JSON object of string values. */
static const char *env_lookup(const json_t *env, const char *name) {
    json_t *v = json_obj_get(env, name);
    return (v && v->type == JSON_STRING) ? (v->str_val ? v->str_val : "") : NULL;
}

/* config._env_expand_match / _env_ref_var_name, applied to one ${...} token. */
static char *expand_one_ref(const char *raw, size_t rawlen, const json_t *env) {
    /* raw includes the ${ and }; inner is raw[2..rawlen-1]. */
    size_t inner_len = rawlen - 3;
    char *inner_raw = (char *)malloc(inner_len + 1);
    if (!inner_raw) return dupstr("");
    memcpy(inner_raw, raw + 2, inner_len);
    inner_raw[inner_len] = '\0';
    char *inner = strip_dup(inner_raw);
    free(inner_raw);
    const char *name = NULL;
    char *env_name = NULL;
    if (!strncmp(inner, "env:", 4)) {
        env_name = strip_dup(inner + 4);
        name = (env_name && env_name[0]) ? env_name : NULL;
    } else {
        /* non-env secret source: ^[a-z][a-z0-9_-]*: */
        int is_source = 0;
        if (inner[0] && islower((unsigned char)inner[0])) {
            size_t i = 1;
            while (inner[i] && (isalnum((unsigned char)inner[i]) || inner[i] == '_' || inner[i] == '-')) i++;
            if (inner[i] == ':') is_source = 1;
        }
        if (!is_source) name = inner;
    }
    char *result = NULL;
    const char *val = name ? env_lookup(env, name) : NULL;
    if (val) {
        result = dupstr(val);
    } else {
        result = (char *)malloc(rawlen + 1);
        if (result) { memcpy(result, raw, rawlen); result[rawlen] = '\0'; }
    }
    free(env_name);
    free(inner);
    return result;
}

static char *expand_env_str(const char *s, const json_t *env) {
    size_t cap = 256, len = 0;
    char *out = (char *)malloc(cap);
    if (!out) return dupstr("");
    while (*s) {
        if (*s == '$' && s[1] == '{') {
            const char *close = strchr(s + 2, '}');
            if (close) {
                size_t rawlen = (size_t)(close - s) + 1;
                char *rep = expand_one_ref(s, rawlen, env);
                size_t rl = rep ? strlen(rep) : 0;
                if (len + rl + 1 > cap) {
                    while (len + rl + 1 > cap) cap *= 2;
                    char *tmp = (char *)realloc(out, cap);
                    if (tmp) out = tmp;
                }
                if (rl) memcpy(out + len, rep, rl);
                len += rl;
                out[len] = '\0';
                free(rep);
                s = close + 1;
                continue;
            }
        }
        if (len + 2 > cap) { cap *= 2; char *tmp = (char *)realloc(out, cap); if (tmp) out = tmp; }
        out[len++] = *s++;
        out[len] = '\0';
    }
    return out;
}

/* Recursive ${VAR} / ${env:VAR} expansion; keys and non-strings untouched. */
static json_t *expand_env(const json_t *obj, const json_t *env) {
    if (!obj) return json_null();
    if (obj->type == JSON_STRING) return json_string(expand_env_str(obj->str_val ? obj->str_val : "", env));
    if (obj->type == JSON_OBJECT) {
        json_t *out = json_object();
        for (size_t i = 0; i < json_object_size(obj); i++)
            json_set(out, json_object_get_key_at(obj, i),
                     expand_env(json_object_get_at(obj, i), env));
        return out;
    }
    if (obj->type == JSON_ARRAY) {
        json_t *out = json_array();
        for (size_t i = 0; i < json_len(obj); i++) json_append(out, expand_env(json_get(obj, i), env));
        return out;
    }
    return json_copy(obj);
}

/* hermes_cli.config_providers.coerce_provider_id */
static void coerce_provider_id(const json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) { buf[0] = '\0'; return; }
    char tmp[512];
    py_str(v, tmp, sizeof(tmp));
    char *s = strip_dup(tmp);
    snprintf(buf, cap, "%s", s ? s : "");
    free(s);
}

/* hermes_cli.config_model_section._normalize_root_model_keys (faithful port). */
static json_t *normalize_root_model_keys(const json_t *config) {
    const json_t *model_in = json_obj_get(config, "model");
    const json_t *model_provider = json_is_object(model_in) ? json_obj_get(model_in, "provider") : NULL;
    int needs = 0;
    if (model_provider && model_provider->type != JSON_NULL && model_provider->type != JSON_STRING)
        needs = 1;
    if (json_is_object(model_in)) {
        const char *keys[] = {"api_base", "model", "name"};
        for (int i = 0; i < 3; i++)
            if (json_truthy(json_obj_get(model_in, keys[i]))) needs = 1;
        const char *dictkeys[] = {"default", "model", "name"};
        for (int i = 0; i < 3; i++)
            if (json_is_object(json_obj_get(model_in, dictkeys[i]))) needs = 1;
    }
    const char *roots[] = {"provider", "base_url", "context_length", "api_base"};
    int has_root = 0;
    for (int i = 0; i < 4; i++)
        if (json_truthy(json_obj_get(config, roots[i]))) has_root = 1;
    if (!has_root && !needs) return json_copy(config);

    json_t *out = json_copy(config);
    const json_t *model = json_obj_get(out, "model");
    json_t *m;
    if (json_is_object(model)) m = json_copy(model);
    else if (json_truthy(model)) {
        m = json_object();
        json_set(m, "default", json_copy(model));
    } else m = json_object();
    json_set(out, "model", m);

    /* Flatten dict-valued default/model/name. Copy strings out before any set/del. */
    const char *idkeys[] = {"default", "model", "name"};
    for (int i = 0; i < 3; i++) {
        const char *k = idkeys[i];
        json_t *val = json_obj_get(m, k);
        if (!json_is_object(val)) continue;
        char nested_model[512], nested_provider[512], outer_provider[512];
        const json_t *nm = json_obj_get(val, "model");
        if (!json_truthy(nm)) nm = json_obj_get(val, "default");
        py_str_or_empty(nm, nested_model, sizeof(nested_model));
        py_str_or_empty(json_obj_get(val, "provider"), nested_provider, sizeof(nested_provider));
        py_str_or_empty(json_obj_get(m, "provider"), outer_provider, sizeof(outer_provider));
        json_set(m, k, json_string(nested_model));
        if (nested_provider[0] && (!outer_provider[0] || !strcmp(outer_provider, "auto")))
            json_set(m, "provider", json_string(nested_provider));
    }

    const char *movekeys[] = {"provider", "base_url", "context_length"};
    for (int i = 0; i < 3; i++) {
        const json_t *root_val = json_obj_get(out, movekeys[i]);
        if (json_truthy(root_val) && !json_truthy(json_obj_get(m, movekeys[i])))
            json_set(m, movekeys[i], json_copy(root_val));
        json_obj_del(out, movekeys[i]);
    }

    const json_t *pv = json_obj_get(m, "provider");
    if (pv) {
        char s[512];
        coerce_provider_id(pv, s, sizeof(s));
        json_set(m, "provider", json_string(s));
    }

    const json_t *a1 = json_obj_get(out, "api_base");
    const json_t *a2 = json_obj_get(m, "api_base");
    if (json_truthy(a1) && !json_truthy(json_obj_get(m, "base_url")))
        json_set(m, "base_url", json_copy(a1));
    if (json_truthy(a2) && !json_truthy(json_obj_get(m, "base_url")))
        json_set(m, "base_url", json_copy(a2));
    json_obj_del(out, "api_base");
    json_obj_del(m, "api_base");

    const json_t *alias = json_obj_get(m, "model");
    if (!json_truthy(alias)) alias = json_obj_get(m, "name");
    if (!json_truthy(json_obj_get(m, "default")) && json_truthy(alias))
        json_set(m, "default", json_copy(alias));
    if (json_truthy(json_obj_get(m, "default"))) {
        json_obj_del(m, "model");
        json_obj_del(m, "name");
    }
    return out;
}

/* hermes_cli.config._deep_merge */
static json_t *deep_merge(const json_t *base, const json_t *override) {
    json_t *out = json_copy(base);
    if (!json_is_object(out) || !json_is_object(override)) return out;
    for (size_t i = 0; i < json_object_size(override); i++) {
        const char *key = json_object_get_key_at(override, i);
        json_t *value = json_object_get_at(override, i);
        json_t *existing = json_obj_get(out, key);
        if (json_is_object(existing) && json_is_object(value))
            json_set(out, key, deep_merge(existing, value));
        else if (!(json_is_object(existing) && value && value->type == JSON_NULL))
            json_set(out, key, json_copy(value));
    }
    return out;
}

/* hermes_cli.managed_scope.apply_managed_overlay (fixture-injected managed/env). */
static json_t *apply_managed(const json_t *config, const json_t *managed, const json_t *env) {
    if (!json_truthy(managed)) return json_copy(config);
    json_t *expanded = expand_env(managed, env);
    json_t *norm = normalize_root_model_keys(expanded);
    json_free(expanded);
    const json_t *mm = json_obj_get(norm, "model");
    if (mm && mm->type == JSON_STRING) {
        json_t *o = json_object();
        json_set(o, "default", json_copy(mm));
        json_set(norm, "model", o);
    }
    json_t *out = deep_merge(config, norm);
    json_free(norm);
    return out;
}

json_t *w15_configeffe_5314b2_effective(const json_t *raw,
                                       const json_t *env,
                                       const json_t *managed) {
    json_t *expanded = expand_env(raw, env);
    json_t *merged;
    if (json_is_object(expanded))
        merged = apply_managed(expanded, managed, env);
    else {
        json_t *empty = json_object();
        merged = apply_managed(empty, managed, env);
        json_free(empty);
    }
    json_free(expanded);
    json_t *result;
    if (json_is_object(merged)) result = normalize_root_model_keys(merged);
    else {
        json_t *empty = json_object();
        result = normalize_root_model_keys(empty);
        json_free(empty);
    }
    json_free(merged);
    return result;
}

