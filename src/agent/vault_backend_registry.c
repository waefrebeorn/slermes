/*
 * vault_backend_registry.c — faithful C11 port of agent/vault_backend_registry.py
 * (gate w16_vaultbacke_84fa80).
 *
 * PoP: w16_vaultbacke_84fa80_namespace_error @ agent/vault_backend_registry.py:_namespace_error
 * PoP: w16_vaultbacke_84fa80_static_name @ agent/vault_backend_registry.py:static_name
 * PoP: w16_vaultbacke_84fa80_enabled_plugin_backends @ agent/vault_backend_registry.py:enabled_plugin_backends
 *
 * inspect.getattr_static on name/prefix/needs_unlock is modelled by plain JSON
 * type checks (a non-string name or prefix stands in for a property/hook).
 * needs_unlock is refused unless it is absent or JSON false (Python: is not False).
 */
#include "noc_w16_vaultbacke_84fa80.h"

#include <stdlib.h>
#include <string.h>

static const char *ERR_NAME = "login backend name must be a lowercase identifier";
static const char *ERR_NS = "builtin login backend namespace is reserved";
static const char *ERR_PREFIX = "login backend prefix must be '<name>:'";
static const char *ERR_SUBCLASS = "login backend must subclass LoginBackend";
static const char *ERR_ACCESS = "login backend must not override attribute access";
static const char *ERR_ATTRS = "login backend name and prefix must be plain string attributes";
static const char *ERR_UNLOCK = "plugin login backends cannot use the Hermes master-password prompt (needs_unlock)";

static int is_lower_ident(const char *s) {
    size_t i;
    if (!s || !s[0] || s[0] < 'a' || s[0] > 'z') return 0;
    for (i = 1; s[i]; ++i) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return 0;
    }
    return 1;
}

static int in_array(const json_t *arr, const char *s) {
    size_t i, n = (arr && arr->type == JSON_ARRAY) ? arr->c.count : 0;
    for (i = 0; i < n; ++i) {
        const json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, s)) return 1;
    }
    return 0;
}

static int startswith_any(const char *s, const json_t *prefixes) {
    size_t i, n = (prefixes && prefixes->type == JSON_ARRAY) ? prefixes->c.count : 0;
    for (i = 0; i < n; ++i) {
        const json_t *v = json_get(prefixes, i);
        if (v && v->type == JSON_STRING && v->str_val) {
            size_t lp = strlen(v->str_val);
            if (strlen(s) >= lp && !memcmp(s, v->str_val, lp)) return 1;
        }
    }
    return 0;
}

static const char *namespace_error(const char *name, const json_t *names, const json_t *prefixes) {
    char with_colon[1024];
    if (!is_lower_ident(name)) return ERR_NAME;
    if (in_array(names, name)) return ERR_NS;
    snprintf(with_colon, sizeof(with_colon), "%s:", name);
    if (startswith_any(with_colon, prefixes)) return ERR_NS;
    return "";
}

json_t *w16_vaultbacke_84fa80_namespace_error(const char *name, const json_t *builtin_names,
                                              const json_t *builtin_prefixes) {
    return json_string(namespace_error(name, builtin_names, builtin_prefixes));
}

json_t *w16_vaultbacke_84fa80_static_name(const json_t *provider) {
    const char *err = NULL, *name = NULL;
    const json_t *v_sub, *v_ga, *v_cls, *v_name, *v_prefix, *v_unlock;
    json_t *out = json_object();

    if (!provider || provider->type != JSON_OBJECT) {
        json_set(out, "ok", json_bool(0));
        json_set(out, "name", json_null());
        json_set(out, "error", json_string(ERR_SUBCLASS));
        return out;
    }
    v_sub = json_obj_get(provider, "is_subclass");
    if (!v_sub || v_sub->type != JSON_BOOL || !v_sub->bool_val) err = ERR_SUBCLASS;
    if (!err) {
        v_ga = json_obj_get(provider, "overrides_getattribute");
        v_cls = json_obj_get(provider, "overrides_class");
        if ((v_ga && v_ga->type == JSON_BOOL && v_ga->bool_val) ||
            (v_cls && v_cls->type == JSON_BOOL && v_cls->bool_val)) err = ERR_ACCESS;
    }
    if (!err) {
        v_name = json_obj_get(provider, "name");
        v_prefix = json_obj_get(provider, "prefix");
        if (!v_name || v_name->type != JSON_STRING || !v_prefix || v_prefix->type != JSON_STRING) {
            err = ERR_ATTRS;
        } else {
            const char *e;
            name = v_name->str_val;
            e = namespace_error(name, json_obj_get(provider, "builtin_names"),
                                json_obj_get(provider, "builtin_prefixes"));
            if (e && e[0]) err = e;
            else {
                char expect[1024];
                snprintf(expect, sizeof(expect), "%s:", name);
                if (strcmp(v_prefix->str_val, expect)) err = ERR_PREFIX;
            }
        }
    }
    if (!err) {
        v_unlock = json_obj_get(provider, "needs_unlock");
        if (v_unlock && !(v_unlock->type == JSON_BOOL && !v_unlock->bool_val)) err = ERR_UNLOCK;
    }
    if (err) {
        json_set(out, "ok", json_bool(0));
        json_set(out, "name", json_null());
        json_set(out, "error", json_string(err));
    } else {
        json_set(out, "ok", json_bool(1));
        json_set(out, "name", json_string(name));
        json_set(out, "error", json_null());
    }
    return out;
}

static int cmp_str_ptr(const void *a, const void *b) {
    const char *const *sa = (const char *const *)a;
    const char *const *sb = (const char *const *)b;
    return strcmp(*sa, *sb);
}

json_t *w16_vaultbacke_84fa80_enabled_plugin_backends(const json_t *registry, const json_t *cfg,
                                                      const json_t *builtin_names,
                                                      const json_t *builtin_prefixes) {
    json_t *out = json_array();
    const char **names = NULL;
    size_t n = 0, i;
    if (registry && registry->type == JSON_ARRAY && registry->c.count) {
        n = registry->c.count;
        names = (const char **)calloc(n, sizeof(char *));
        for (i = 0; i < n; ++i) {
            const json_t *o = json_get(registry, i);
            const json_t *v = o && o->type == JSON_OBJECT ? json_obj_get(o, "name") : NULL;
            names[i] = (v && v->type == JSON_STRING) ? v->str_val : "";
        }
        qsort(names, n, sizeof(char *), cmp_str_ptr);
        for (i = 0; i < n; ++i) {
            const json_t *section = cfg && cfg->type == JSON_OBJECT ? json_obj_get(cfg, names[i]) : NULL;
            const json_t *enabled = section && section->type == JSON_OBJECT ? json_obj_get(section, "enabled") : NULL;
            const char *e = namespace_error(names[i], builtin_names, builtin_prefixes);
            if (!e[0] && section && section->type == JSON_OBJECT &&
                enabled && enabled->type == JSON_BOOL && enabled->bool_val) {
                json_append(out, json_string(names[i]));
            }
        }
        free(names);
    }
    return out;
}
