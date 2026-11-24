/*
 * plugins_aux_tasks.c - C home for upstream Hermes hermes_cli/plugins_aux_tasks.py.
 * PoP: w16_pluginsaux_851d61_build_entry @ hermes_cli/plugins_aux_tasks.py:build_auxiliary_task_entry
 *
 * Faithful port of the registry-entry builder: key validation, reserved-key
 * and duplicate-owner refusal, inheritance degradation, and the schema-shaped
 * defaults merge. Validation failures are returned as {"error": <ValueError>}.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_pluginsaux_851d61.h"

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static json_t *err(const char *fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    json_t *res = json_object();
    json_set(res, "error", json_string(buf));
    return res;
}

/* Python str(v) for scalar interpolations (message uses f"...{value}"). */
static char *py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return dupstr("None");
    switch (v->type) {
        case JSON_STRING: return dupstr(v->str_val ? v->str_val : "");
        case JSON_BOOL:   return dupstr(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
            else
                snprintf(buf, sizeof(buf), "%.17g", v->num_val);
            return dupstr(buf);
        case JSON_ARRAY:  return dupstr("[]");
        case JSON_OBJECT: return dupstr("{}");
        default:          return dupstr("");
    }
}

/* Python repr(v) for the {key!r} in error messages. */
static char *py_repr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("None");
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        int has_s = strchr(s, '\'') != NULL, has_d = strchr(s, '"') != NULL;
        char q = (has_s && !has_d) ? '"' : '\'';
        size_t n = strlen(s);
        char *out = (char *)malloc(2 * n + 4);
        if (!out) return dupstr("");
        size_t o = 0;
        out[o++] = q;
        for (size_t i = 0; i < n; i++) {
            char c = s[i];
            if (c == '\\' || c == q) out[o++] = '\\';
            out[o++] = c;
        }
        out[o++] = q;
        out[o] = '\0';
        return out;
    }
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_ARRAY) return dupstr("[]");
    if (v->type == JSON_OBJECT) return dupstr("{}");
    return py_str(v);
}

static int in_set(const char *s, const char *const *set, size_t n) {
    if (!s) return 0;
    for (size_t i = 0; i < n; i++) if (!strcmp(s, set[i])) return 1;
    return 0;
}

/* Keys of hermes_cli.main_provider_setup._AUX_TASKS (built-in aux tasks). */
static const char *const BUILTIN_AUX_KEYS[] = {
    "vision", "compression", "approval", "mcp", "title_generation", "review",
    "memory_query_rewrite", "tts_audio_tags", "voice_chat", "triage_specifier",
    "kanban_decomposer", "profile_describer", "curator", "group_router"
};
#define NBUILTIN (sizeof(BUILTIN_AUX_KEYS) / sizeof(BUILTIN_AUX_KEYS[0]))

/* _INHERITABLE_CORE_BLOCKS */
static const char *const INHERITABLE_BLOCKS[] = {"background_review", "curator"};
#define NBLOCKS (sizeof(INHERITABLE_BLOCKS) / sizeof(INHERITABLE_BLOCKS[0]))

struct json_t *w16_pluginsaux_851d61_build_entry(const char *me, const char *owner_id,
                                                 const struct json_t *key,
                                                 const char *display_name, const char *description,
                                                 const struct json_t *defaults,
                                                 const struct json_t *inherit_from,
                                                 const struct json_t *registered) {
    char *me_s = me ? (char *)me : (char *)"";
    char *owner_s = owner_id ? (char *)owner_id : (char *)"";

    /* if not key or not isinstance(key, str) */
    int key_str = key && key->type == JSON_STRING;
    int key_truthy = key && ((key->type == JSON_STRING && key->str_val && key->str_val[0]) ||
                             (key->type != JSON_STRING && key->type != JSON_NULL));
    if (!key_str || !key_truthy) {
        char *kr = py_repr(key);
        json_t *res = err("Plugin '%s' tried to register auxiliary task with invalid key %s", me_s, kr);
        free(kr);
        return res;
    }
    const char *key_s = key->str_val;

    /* only alphanumeric characters and underscores */
    for (const char *p = key_s; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '_')) {
            char *kr = py_repr(key);
            json_t *res = err("Plugin '%s' auxiliary task key %s must contain only alphanumeric "
                              "characters and underscores", me_s, kr);
            free(kr);
            return res;
        }
    }

    /* reserved built-in key */
    if (in_set(key_s, BUILTIN_AUX_KEYS, NBUILTIN)) {
        char *kr = py_repr(key);
        json_t *res = err("Plugin '%s' cannot register auxiliary task %s — that key is reserved "
                          "for a built-in task. Pick a plugin-namespaced key (e.g. '%s_%s').",
                          me_s, kr, me_s, key_s);
        free(kr);
        return res;
    }

    /* duplicate registration by another owner */
    json_t *existing = json_obj_get(registered, key_s);
    if (existing && existing->type != JSON_NULL) {
        json_t *ep = json_obj_get(existing, "plugin");
        char *eps = py_str(ep);
        if (strcmp(eps, owner_s) != 0) {
            char *kr = py_repr(key);
            json_t *res = err("Plugin '%s' cannot register auxiliary task %s — already registered "
                              "by plugin '%s'", me_s, kr, eps);
            free(kr); free(eps);
            return res;
        }
        free(eps);
    }

    /* inheritance validation -> degrade to None */
    int inherit_ok = 0;
    if (inherit_from && inherit_from->type == JSON_STRING && inherit_from->str_val &&
        inherit_from->str_val[0]) {
        const char *inh = inherit_from->str_val;
        int allowed = in_set(inh, BUILTIN_AUX_KEYS, NBUILTIN) ||
                      in_set(inh, INHERITABLE_BLOCKS, NBLOCKS) ||
                      json_obj_get(registered, inh) != NULL;
        if (strcmp(inh, key_s) != 0 && allowed) inherit_ok = 1;
    }

    const json_t *defaults_obj = (defaults && defaults->type == JSON_OBJECT) ? defaults : NULL;

    json_t *task_defaults = json_object();
    if (inherit_ok) {
        if (defaults_obj)
            for (size_t i = 0; i < json_object_size(defaults_obj); i++) {
                const char *k = json_object_get_key_at(defaults_obj, i);
                if (k) { char *kc = dupstr(k); json_set(task_defaults, kc,
                             json_copy(json_object_get_at(defaults_obj, i))); free(kc); }
            }
    } else {
        json_set(task_defaults, "provider", json_string("auto"));
        json_set(task_defaults, "model", json_string(""));
        json_set(task_defaults, "base_url", json_string(""));
        json_set(task_defaults, "api_key", json_string(""));
        json_set(task_defaults, "timeout", json_number(60.0));
        json_set(task_defaults, "extra_body", json_object());
        if (defaults_obj)
            for (size_t i = 0; i < json_object_size(defaults_obj); i++) {
                const char *k = json_object_get_key_at(defaults_obj, i);
                if (k) { char *kc = dupstr(k); json_set(task_defaults, kc,
                             json_copy(json_object_get_at(defaults_obj, i))); free(kc); }
            }
    }

    json_t *res = json_object();
    json_set(res, "key", json_string(key_s));
    json_set(res, "display_name", json_string(display_name ? display_name : ""));
    json_set(res, "description", json_string(description ? description : ""));
    json_set(res, "defaults", task_defaults);
    json_set(res, "inherit_from", inherit_ok ? json_copy(inherit_from) : json_null());
    json_set(res, "plugin", json_string(owner_s));
    json_set(res, "plugin_key", json_string(owner_s));
    return res;
}
