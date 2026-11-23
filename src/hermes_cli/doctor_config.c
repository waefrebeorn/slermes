/*
 * doctor_config.c — C port of upstream Hermes hermes_cli/doctor_config.py.
 *
 * PoP: w10_doctorconf_741cee_collect_deprecated_config_keys @ hermes_cli/doctor_config.py:collect_deprecated_config_keys
 * PoP: w10_doctorconf_741cee_collect_deprecated_env_vars @ hermes_cli/doctor_config.py:collect_deprecated_env_vars
 * PoP: w10_doctorconf_741cee_endpoint_url @ hermes_cli/doctor_config.py:_endpoint_url
 * PoP: w10_doctorconf_741cee_has_provider_env_config @ hermes_cli/doctor_config.py:_has_provider_env_config
 *
 * Constants below are literal mirrors of _DEPRECATED_CONFIG_KEYS,
 * _DEPRECATED_ENV_VARS and doctor._PROVIDER_ENV_HINTS in the Python source.
 */
#include "noc_w10_doctorconf_741cee.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* (section, key, replacement) */
static const char *const CFG_KEYS[][3] = {
    {"display", "tool_progress_overrides", "display.platforms"},
    {"delegation", "max_async_children", "delegation.max_concurrent_children"},
    {"compression", "summary_model", "auxiliary.compression"},
    {"compression", "summary_provider", "auxiliary.compression"},
    {"compression", "summary_base_url", "auxiliary.compression"},
};
/* (name, replacement) */
static const char *const ENV_VARS[][2] = {
    {"HERMES_TOOL_PROGRESS", "display.tool_progress in config.yaml — ignored/unsupported since config floor v12"},
    {"HERMES_TOOL_PROGRESS_MODE", "display.tool_progress in config.yaml"},
    {"TERMINAL_CWD", "terminal.cwd in config.yaml"},
    {"MESSAGING_CWD", "terminal.cwd in config.yaml"},
    {"QQ_HOME_CHANNEL", "QQBOT_HOME_CHANNEL"},
    {"QQ_HOME_CHANNEL_NAME", "QQBOT_HOME_CHANNEL_NAME"},
};
static const char *const PROVIDER_HINTS[] = {
    "DEEPINFRA_API_KEY", "OPENROUTER_API_KEY", "OPENAI_API_KEY", "ANTHROPIC_API_KEY", "ANTHROPIC_TOKEN",
    "OPENAI_BASE_URL", "NOUS_API_KEY", "GLM_API_KEY", "ZAI_API_KEY", "Z_AI_API_KEY", "KIMI_API_KEY",
    "KIMI_CN_API_KEY", "GMI_API_KEY", "FIREWORKS_API_KEY", "ACTUAL_API_KEY", "ACTUAL_BASE_URL", "MINIMAX_API_KEY",
    "MINIMAX_CN_API_KEY", "KILOCODE_API_KEY", "DEEPSEEK_API_KEY", "DASHSCOPE_API_KEY", "HF_TOKEN",
    "AI_GATEWAY_API_KEY", "OPENCODE_ZEN_API_KEY", "OPENCODE_GO_API_KEY", "COMMANDCODE_API_KEY", "XIAOMI_API_KEY",
    "TOKENHUB_API_KEY", "TOKENPLAN_API_KEY",
};

static int truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}

/* Python str() for the JSON scalar types used by these ports. Returns malloc'd. */
static char *py_str(const json_t *v) {
    if (!v) return (char *)calloc(1, 1);
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_NUMBER) {
        char b[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(b, sizeof(b), "%lld", (long long)v->num_val);
        else
            snprintf(b, sizeof(b), "%g", v->num_val);
        return strdup(b);
    }
    return strdup(v->type == JSON_ARRAY ? "[]" : "{}");
}

/* Python str.strip() for ASCII whitespace. Returns malloc'd. */
static char *strip(const char *s) {
    if (!s) return strdup("");
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    char *o = (char *)malloc(n + 1);
    memcpy(o, s, n); o[n] = 0;
    return o;
}

static void add_pair(json_t *out, const char *a, const char *b) {
    json_t *pair = json_array();
    json_append(pair, json_string(a));
    json_append(pair, json_string(b));
    json_append(out, pair);
}

json_t *w10_doctorconf_741cee_collect_deprecated_config_keys(const json_t *raw_config) {
    json_t *out = json_array();
    if (!json_is_object(raw_config)) return out;
    for (size_t i = 0; i < sizeof(CFG_KEYS) / sizeof(CFG_KEYS[0]); i++) {
        const char *section = CFG_KEYS[i][0], *key = CFG_KEYS[i][1], *repl = CFG_KEYS[i][2];
        const json_t *sub = json_obj_get(raw_config, section);
        if (!json_is_object(sub)) continue;              /* isinstance(raw_config.get(section), dict) */
        if (!json_obj_get(sub, key)) continue;           /* key in raw_config[section] (null counts) */
        char path[256];
        snprintf(path, sizeof(path), "%s.%s", section, key);
        add_pair(out, path, repl);
    }
    return out;
}

json_t *w10_doctorconf_741cee_collect_deprecated_env_vars(const json_t *env_map) {
    json_t *out = json_array();
    if (!json_is_object(env_map)) return out;
    for (size_t i = 0; i < sizeof(ENV_VARS) / sizeof(ENV_VARS[0]); i++) {
        const char *name = ENV_VARS[i][0], *repl = ENV_VARS[i][1];
        const json_t *v = json_obj_get(env_map, name);
        if (!v || v->type == JSON_NULL) continue;        /* env_map.get(name) is not None */
        char *s = py_str(v), *t = strip(s);
        int nonempty = t[0] != '\0';                     /* str(env_map[name]).strip() != "" */
        free(t); free(s);
        if (nonempty) add_pair(out, name, repl);
    }
    return out;
}

json_t *w10_doctorconf_741cee_endpoint_url(const json_t *entry) {
    const char *keys[3] = {"api", "base_url", "url"};
    const json_t *url = NULL;
    for (int i = 0; i < 3; i++) {
        const json_t *v = json_obj_get(entry, keys[i]);
        if (truthy(v)) { url = v; break; }               /* Python `or` chain */
    }
    if (!url) return json_string("");
    char *raw = py_str(url);
    char *s = strip(raw); free(raw);
    size_t n = strlen(s);
    while (n && s[n - 1] == '/') s[--n] = 0;             /* rstrip("/") */
    for (size_t i = 0; i < n; i++) s[i] = (char)tolower((unsigned char)s[i]);
    json_t *out = json_string(s);
    free(s);
    return out;
}

int w10_doctorconf_741cee_has_provider_env_config(const char *content) {
    if (!content) return 0;
    for (size_t i = 0; i < sizeof(PROVIDER_HINTS) / sizeof(PROVIDER_HINTS[0]); i++)
        if (strstr(content, PROVIDER_HINTS[i])) return 1;
    return 0;
}
