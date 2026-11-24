/*
 * config_model_section.c -- real C11 port of the ``model:`` section load/save
 * canon of upstream Hermes hermes_cli/config_model_section.py.
 *
 * PoP: w20_configmode_b2613d_normalize @ hermes_cli/config_model_section.py:_normalize_root_model_keys
 * PoP: w20_configmode_b2613d_coerce_provider @ hermes_cli/config_model_section.py:coerce_provider_id
 *
 * The upstream helper has no I/O: it is a pure dict rewrite that runs at the
 * single config load/save chokepoint.  Only the ``_warn_once_per_provider``
 * side effect is modelled -- as the recorded ``discarded:<key>.<extra>``
 * signature list, which is exactly what that helper's dedupe key holds.
 */
#include "noc_w20_configmode_b2613d.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- small helpers -------------------------------------------------------- */

static char *cm2613_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* Python truthiness of a decoded JSON value; absent (NULL) reads as None. */
static int cm2613_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

static int cm2613_is_dict(const json_t *v) {
    return v && v->type == JSON_OBJECT;
}

/* ``type(x) is int``: a JSON bool is not an int, and only integral numbers are. */
static int cm2613_is_int(const json_t *v) {
    double d;
    if (!v || v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d) || floor(d) != d) return 0;
    return 1;
}

/* str(value) for the scalar shapes a config value can carry. */
static char *cm2613_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return cm2613_dup("None");
    switch (v->type) {
    case JSON_NULL:   return cm2613_dup("None");
    case JSON_STRING: return cm2613_dup(v->str_val ? v->str_val : "");
    case JSON_BOOL:   return cm2613_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (cm2613_is_int(v)) snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return cm2613_dup(buf);
    case JSON_ARRAY: {
        /* str([...]) for the shallow scalar lists a config can hold. */
        size_t i, len = 8;
        char *out;
        for (i = 0; i < v->c.count; i++) {
            char *elt = cm2613_str(v->c.items[i]);
            len += (elt ? strlen(elt) : 4) + 6;
            free(elt);
        }
        out = (char *)malloc(len);
        if (!out) return cm2613_dup("[]");
        out[0] = '\0';
        strcat(out, "[");
        for (i = 0; i < v->c.count; i++) {
            char *elt = cm2613_str(v->c.items[i]);
            if (i) strcat(out, ", ");
            if (v->c.items[i] && v->c.items[i]->type == JSON_STRING) {
                strcat(out, "'");
                strcat(out, elt ? elt : "");
                strcat(out, "'");
            } else {
                strcat(out, elt ? elt : "None");
            }
            free(elt);
        }
        strcat(out, "]");
        return out;
    }
    case JSON_OBJECT:
        return cm2613_dup("{}");
    }
    return cm2613_dup("None");
}

/* str.strip() for the whitespace set Python strips. */
static void cm2613_strip(char *s) {
    size_t n, i = 0;
    if (!s) return;
    n = strlen(s);
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') i++;
        else break;
    }
    if (i) memmove(s, s + i, n - i + 1);
    n = strlen(s);
    while (n > 0) {
        unsigned char c = (unsigned char)s[n - 1];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') s[--n] = '\0';
        else break;
    }
}

/* str(value or "").strip() */
static char *cm2613_str_strip(const json_t *v) {
    char *s = cm2613_truthy(v) ? cm2613_str(v) : cm2613_dup("");
    if (!s) return NULL;
    cm2613_strip(s);
    return s;
}

char *w20_configmode_b2613d_coerce_provider(const json_t *value) {
    /* coerce_provider_id: None -> "", else str(value).strip() */
    if (!value || value->type == JSON_NULL) return cm2613_dup("");
    {
        char *s = cm2613_str(value);
        if (!s) return NULL;
        cm2613_strip(s);
        return s;
    }
}

/* --- _normalize_root_model_keys ------------------------------------------ */

static int cm2613_any_truthy_key(const json_t *cfg, const char *const *keys) {
    int i;
    for (i = 0; keys[i]; i++) {
        if (cm2613_truthy(json_obj_get(cfg, keys[i]))) return 1;
    }
    return 0;
}

static json_t *cm2613_report(json_t *cfg, json_t *warnings) {
    json_t *out = json_object();
    json_set(out, "config", cfg ? cfg : json_object());
    json_set(out, "warnings", warnings ? warnings : json_array());
    return out;
}

json_t *w20_configmode_b2613d_normalize(const json_t *config) {
    static const char *const ROOT_KEYS[] = {"provider", "base_url", "context_length",
                                            "api_base", NULL};
    static const char *const MOVE_KEYS[] = {"provider", "base_url", "context_length", NULL};
    static const char *const ID_KEYS[] = {"default", "model", "name", NULL};
    json_t *cfg, *model_in, *model_provider, *model, *warnings;
    int model_is_dict, needs_model_work, has_root, i;

    cfg = cm2613_is_dict(config) ? json_copy(config) : json_object();
    if (!cfg) cfg = json_object();

    model_in = json_obj_get(cfg, "model");
    model_is_dict = cm2613_is_dict(model_in);
    model_provider = model_is_dict ? json_obj_get(model_in, "provider") : NULL;

    needs_model_work =
        (model_provider && model_provider->type != JSON_STRING &&
         model_provider->type != JSON_NULL) ||
        (model_is_dict &&
         (cm2613_truthy(json_obj_get(model_in, "api_base")) ||
          cm2613_truthy(json_obj_get(model_in, "model")) ||
          cm2613_truthy(json_obj_get(model_in, "name")) ||
          cm2613_is_dict(json_obj_get(model_in, "default")) ||
          cm2613_is_dict(json_obj_get(model_in, "model")) ||
          cm2613_is_dict(json_obj_get(model_in, "name"))));
    has_root = cm2613_any_truthy_key(cfg, ROOT_KEYS);

    if (!has_root && !needs_model_work) return cm2613_report(cfg, json_array());

    /* model = dict(model) if isinstance(model, dict) else {"default": model} if model else {} */
    if (model_is_dict) {
        model = json_copy(model_in);
    } else if (cm2613_truthy(model_in)) {
        model = json_object();
        json_set(model, "default", json_copy(model_in));
    } else {
        model = json_object();
    }
    json_set(cfg, "model", model);

    warnings = json_array();

    /* Flatten a dict-valued default/model/name, warning on dropped extras. */
    for (i = 0; ID_KEYS[i]; i++) {
        json_t *val = json_obj_get(model, ID_KEYS[i]);
        json_t *nested_model, *nested_provider_val;
        char *nested_model_s, *nested_provider, *outer;
        size_t j;
        if (!cm2613_is_dict(val)) continue;
        /* Warn first: writing model[<key>] below frees ``val`` (json_set replaces). */
        for (j = 0; j < val->c.count; j++) {
            const char *extra = val->c.keys[j];
            char sig[512];
            if (!extra) continue;
            if (!strcmp(extra, "model") || !strcmp(extra, "default") ||
                !strcmp(extra, "provider")) continue;
            snprintf(sig, sizeof(sig), "discarded:%s.%s", ID_KEYS[i], extra);
            json_append(warnings, json_string(sig));
        }
        nested_model = json_obj_get(val, "model");
        if (!cm2613_truthy(nested_model)) nested_model = json_obj_get(val, "default");
        nested_provider_val = json_obj_get(val, "provider");
        nested_provider = cm2613_str_strip(nested_provider_val);
        nested_model_s = cm2613_str_strip(nested_model);
        json_set(model, ID_KEYS[i], json_string(nested_model_s ? nested_model_s : ""));
        free(nested_model_s);
        if (nested_provider && nested_provider[0]) {
            outer = cm2613_str_strip(json_obj_get(model, "provider"));
            if (!outer || !outer[0] || !strcmp(outer, "auto")) {
                json_set(model, "provider", json_string(nested_provider));
            }
            free(outer);
        }
        free(nested_provider);
    }

    /* Root-level legacy keys move under model only when model.* is empty. */
    for (i = 0; MOVE_KEYS[i]; i++) {
        json_t *root_val = json_obj_get(cfg, MOVE_KEYS[i]);
        if (cm2613_truthy(root_val) && !cm2613_truthy(json_obj_get(model, MOVE_KEYS[i]))) {
            json_set(model, MOVE_KEYS[i], json_copy(root_val));
        }
        json_obj_del(cfg, MOVE_KEYS[i]);
    }

    /* Provider identity is a string (#117345). Guard on presence. */
    if (json_obj_get(model, "provider")) {
        char *p = w20_configmode_b2613d_coerce_provider(json_obj_get(model, "provider"));
        json_set(model, "provider", json_string(p ? p : ""));
        free(p);
    }

    /* api_base is an alias for base_url (fallback only), then dropped. */
    {
        json_t *sources[2];
        sources[0] = json_obj_get(cfg, "api_base");
        sources[1] = json_obj_get(model, "api_base");
        for (i = 0; i < 2; i++) {
            if (cm2613_truthy(sources[i]) &&
                !cm2613_truthy(json_obj_get(model, "base_url"))) {
                json_set(model, "base_url", json_copy(sources[i]));
            }
        }
    }
    json_obj_del(cfg, "api_base");
    json_obj_del(model, "api_base");

    /* model/name are last-resort aliases for default, then dropped. */
    {
        json_t *alias = json_obj_get(model, "model");
        if (!cm2613_truthy(alias)) alias = json_obj_get(model, "name");
        if (!cm2613_truthy(json_obj_get(model, "default")) && cm2613_truthy(alias)) {
            json_set(model, "default", json_copy(alias));
        }
        if (cm2613_truthy(json_obj_get(model, "default"))) {
            json_obj_del(model, "model");
            json_obj_del(model, "name");
        }
    }

    return cm2613_report(cfg, warnings);
}

json_t *w20_configmode_b2613d_normalize_config(const json_t *config) {
    json_t *rep = w20_configmode_b2613d_normalize(config);
    json_t *cfg = json_copy(json_obj_get(rep, "config"));
    json_free(rep);
    return cfg ? cfg : json_object();
}
