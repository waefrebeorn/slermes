/*
 * doctor_tools.c — faithful C11 port of upstream Hermes hermes_cli/doctor_tools.py.
 *
 * PoP: w9_doctortools_317c87_plural @ hermes_cli/doctor_tools.py:_plural
 * PoP: w9_doctortools_317c87_setup_gated @ hermes_cli/doctor_tools.py:_setup_gated
 * PoP: w9_doctortools_317c87_kanban_env_gate @ hermes_cli/doctor_tools.py:_is_kanban_worker_env_gate
 * PoP: w9_doctortools_317c87_tool_availability_detail @ hermes_cli/doctor_tools.py:_doctor_tool_availability_detail
 * PoP: w9_doctortools_317c87_apply_overrides @ hermes_cli/doctor_tools.py:_apply_doctor_tool_availability_overrides
 * PoP: w9_doctortools_317c87_missing_api_key_toolsets @ hermes_cli/doctor_tools.py:_missing_api_key_toolsets_for_summary
 * PoP: w9_doctortools_317c87_safe_which @ hermes_cli/doctor_tools.py:_safe_which
 * PoP: w9_doctortools_317c87_chromium_not_needed_reason @ hermes_cli/doctor_tools.py:_chromium_not_needed_reason
 *
 * Environment/PATH/config reads are fixture seams (the Python functions are
 * called with os.environ / shutil.which / load_config_readonly injected by the
 * oracle), so no C process environment is mutated.
 */
#define _GNU_SOURCE
#include "noc_w9_doctortools_317c87.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count != 0;
        default:          return 0;
    }
}

static const char *strval(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

json_t *w9_doctortools_317c87_plural(int n) {
    return json_string(n == 1 ? "vulnerability" : "vulnerabilities");
}

/* is_truthy_value(): None->default, str->strip().lower() in {1,true,yes,on}, else bool(). */
static int env_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) {
        const char *p = v->str_val ? v->str_val : "";
        char word[128];
        size_t n = 0;
        while (*p && isspace((unsigned char)*p)) p++;
        while (*p && n + 1 < sizeof word) {
            word[n++] = (char)tolower((unsigned char)*p);
            p++;
        }
        while (n > 0 && isspace((unsigned char)word[n - 1])) n--;
        word[n] = '\0';
        return !strcmp(word, "1") || !strcmp(word, "true") ||
               !strcmp(word, "yes") || !strcmp(word, "on");
    }
    return py_truthy(v);
}

static int starts_kanban(const json_t *v) {
    const char *s = strval(v);
    return s && !strncmp(s, "kanban_", 7);
}

int w9_doctortools_317c87_setup_gated(const json_t *item) {
    if (!json_is_object(item)) return 0;
    if (py_truthy(json_obj_get(item, "missing_vars"))) return 1;
    if (py_truthy(json_obj_get(item, "env_vars"))) return 1;
    const char *name = strval(json_obj_get(item, "name"));
    return name && !strcmp(name, "image_gen");
}

int w9_doctortools_317c87_kanban_env_gate(const json_t *item, const json_t *env_value) {
    if (!json_is_object(item)) return 0;
    const char *name = strval(json_obj_get(item, "name"));
    if (!name || strcmp(name, "kanban")) return 0;
    if (env_truthy(env_value)) return 0;
    json_t *tools = json_obj_get(item, "tools");
    if (!py_truthy(tools) || tools->type != JSON_ARRAY || json_len(tools) == 0) return 0;
    for (size_t i = 0; i < json_len(tools); i++)
        if (!starts_kanban(json_get(tools, i))) return 0;
    return 1;
}

json_t *w9_doctortools_317c87_tool_availability_detail(const char *toolset,
                                                       const json_t *env_value) {
    if (toolset && !strcmp(toolset, "kanban") && !env_truthy(env_value))
        return json_string("(runtime-gated; loaded only for dispatcher-spawned workers)");
    return json_string("");
}

static int array_has_string(const json_t *arr, const char *name) {
    if (!arr || arr->type != JSON_ARRAY || !name) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *s = strval(json_get(arr, i));
        if (s && !strcmp(s, name)) return 1;
    }
    return 0;
}

json_t *w9_doctortools_317c87_apply_overrides(const json_t *available,
                                              const json_t *unavailable,
                                              const json_t *env_value) {
    json_t *out_avail = json_array();
    if (available && available->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(available); i++)
            json_append(out_avail, json_copy(json_get(available, i)));
    json_t *out_unavail = json_array();
    if (unavailable && unavailable->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(unavailable); i++) {
            json_t *item = json_get(unavailable, i);
            if (!w9_doctortools_317c87_kanban_env_gate(item, env_value)) {
                json_append(out_unavail, json_copy(item));
            } else if (!array_has_string(out_avail, "kanban")) {
                json_append(out_avail, json_string("kanban"));
            }
        }
    }
    json_t *out = json_object();
    json_set(out, "available", out_avail);
    json_set(out, "unavailable", out_unavail);
    return out;
}

json_t *w9_doctortools_317c87_missing_api_key_toolsets(const json_t *unavailable,
                                                       const json_t *enabled) {
    json_t *out = json_array();
    if (!unavailable || unavailable->type != JSON_ARRAY) return out;
    int have_enabled = enabled && enabled->type == JSON_ARRAY;
    for (size_t i = 0; i < json_len(unavailable); i++) {
        json_t *item = json_get(unavailable, i);
        if (!w9_doctortools_317c87_setup_gated(item)) continue;
        if (have_enabled) {
            const char *name = strval(json_obj_get(item, "name"));
            if (!name || !array_has_string(enabled, name)) continue;
        }
        json_append(out, json_copy(item));
    }
    return out;
}

json_t *w9_doctortools_317c87_safe_which(const char *cmd, const json_t *path_map) {
    const char *p = strval(json_obj_get(path_map, cmd));
    return p ? json_string(p) : json_null();
}

static void strip_lower(char *out, size_t cap, const char *s) {
    const char *p = s ? s : "";
    while (*p && isspace((unsigned char)*p)) p++;
    size_t n = 0;
    while (*p && n + 1 < cap) { out[n++] = (char)tolower((unsigned char)*p); p++; }
    while (n > 0 && isspace((unsigned char)out[n - 1])) n--;
    out[n] = '\0';
}

static json_t *cfg_get(const json_t *config, const char *section, const char *key) {
    return json_obj_get(json_obj_get(config, section), key);
}

json_t *w9_doctortools_317c87_chromium_not_needed_reason(const json_t *config,
                                                         const json_t *cdp_env) {
    if (py_truthy(cdp_env) || py_truthy(cfg_get(config, "browser", "cdp_url")))
        return json_string("a CDP override is set");
    json_t *engine = cfg_get(config, "browser", "engine");
    char e[128];
    strip_lower(e, sizeof e, strval(engine));
    if (!strcmp(e, "lightpanda")) return json_string("browser.engine is lightpanda");
    json_t *provider = cfg_get(config, "browser", "cloud_provider");
    char p[128];
    strip_lower(p, sizeof p, strval(provider));
    if (p[0] && strcmp(p, "local")) {
        char msg[256];
        snprintf(msg, sizeof msg, "cloud provider %s is selected", p);
        return json_string(msg);
    }
    return json_string("");
}

