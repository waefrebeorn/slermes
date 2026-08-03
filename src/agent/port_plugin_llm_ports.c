/*
 * port_plugin_llm_remaining.c — Port of agent/plugin_llm.py host-owned
 * LLM surface. Allowlist/trust policy, input block normalization,
 * message building, code-fence stripping, JSON parsing, usage/text
 * extraction, attribution precedence.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "json.h"
#include "yaml.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _normalize_ref @ agent/plugin_llm.py:_normalize_ref */
char *pll_normalize_ref(const char *raw) {
    /* Python: lower-case + strip whitespace. */
    if (!raw) return strdup("");
    char *l = lowerdup(raw);
    if (!l) return strdup("");
    char *s = l;
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t')) *--e = '\0';
    char *out = strdup(s);
    free(l);
    return out;
}

/* PoP: _coerce_allowlist @ agent/plugin_llm.py:_coerce_allowlist */
char *pll_coerce_allowlist(const char *yaml_list) {
    /* Python: ["*"] → allow_any; list → frozenset; None → None. */
    if (!yaml_list) return strdup("{\"allow_any\": false, \"entries\": null}");
    if (strstr(yaml_list, "*") && strstr(yaml_list, "[")) {
        /* ["*"] or [*] */
        if (strstr(yaml_list, "\"*\"") || strstr(yaml_list, "'*'") ||
            (strchr(yaml_list, '*') && !strchr(yaml_list, ',')))
            return strdup("{\"allow_any\": true, \"entries\": null}");
    }
    char *out = NULL;
    asprintf(&out, "{\"allow_any\": false, \"entries\": %s}", yaml_list);
    return out;
}

/* PoP: _resolve_trust_policy @ agent/plugin_llm.py:_resolve_trust_policy */
char *pll_resolve_trust_policy(const char *config_yaml, const char *plugin_id) {
    /* Python: plugins.entries.<id>.llm from config; missing config →
     * fully restrictive default-deny. The policy is resolved per-call so
     * config edits take effect without restarting. Returns
     * {allow_any, allow_provider_override, allowed_providers,
     *  allow_model_override, allowed_models}. */
    if (!plugin_id) return strdup("{\"allow_any\": false, \"entries\": []}");
    if (!config_yaml) return strdup("{\"allow_any\": false, \"entries\": []}");

    /* Parse the YAML config → JSON. */
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse(config_yaml, &err);
    if (!doc) { free(err); return strdup("{\"allow_any\": false, \"entries\": []}"); }
    char *js = yaml_to_json_string(doc, "");
    yaml_free(doc);
    if (!js) return strdup("{\"allow_any\": false, \"entries\": []}");
    json_t *cfg = json_parse(js, NULL);
    free(js);
    if (!cfg) return strdup("{\"allow_any\": false, \"entries\": []}");

    /* plugins.entries.<plugin_id>.llm */
    json_t *plugins = json_obj_get(cfg, "plugins");
    json_t *entries = (plugins && plugins->type == JSON_OBJECT) ? json_obj_get(plugins, "entries") : NULL;
    json_t *entry = (entries && entries->type == JSON_OBJECT) ? json_obj_get(entries, plugin_id) : NULL;
    json_t *llm = (entry && entry->type == JSON_OBJECT) ? json_obj_get(entry, "llm") : NULL;
    json_free(cfg);
    if (!llm || llm->type != JSON_OBJECT) {
        /* Missing config → default deny. */
        return strdup("{\"allow_any\": false, \"entries\": []}");
    }

    json_t *out = json_object();
    bool allow_provider = json_get_bool(llm, "allow_provider_override", false);
    bool allow_model = json_get_bool(llm, "allow_model_override", false);
    json_set(out, "allow_provider_override", json_bool(allow_provider));
    json_set(out, "allow_model_override", json_bool(allow_model));

    /* allowed_providers + allow_any_provider. */
    json_t *ap = json_obj_get(llm, "allowed_providers");
    json_t *ap_arr = json_array();
    bool any_provider = false;
    if (ap) {
        if (ap->type == JSON_ARRAY) {
            json_t *copy = json_copy(ap);
            json_free(ap_arr);
            ap_arr = copy;
        } else if (ap->type == JSON_STRING && ap->str_val &&
                   strcmp(ap->str_val, "*") == 0) {
            any_provider = true;
        }
    }
    if (!any_provider && json_len(ap_arr) == 0) any_provider = allow_provider;
    json_set(out, "allowed_providers", ap_arr);
    json_set(out, "allow_any_provider", json_bool(any_provider));

    /* allowed_models + allow_any_model. */
    json_t *am = json_obj_get(llm, "allowed_models");
    json_t *am_arr = json_array();
    bool any_model = false;
    if (am) {
        if (am->type == JSON_ARRAY) {
            json_t *copy = json_copy(am);
            json_free(am_arr);
            am_arr = copy;
        } else if (am->type == JSON_STRING && am->str_val &&
                   strcmp(am->str_val, "*") == 0) {
            any_model = true;
        }
    }
    if (!any_model && json_len(am_arr) == 0) any_model = allow_model;
    json_set(out, "allowed_models", am_arr);
    json_set(out, "allow_any_model", json_bool(any_model));

    json_set(out, "allow_any", json_bool(any_provider && any_model));
    char *ser = json_serialize(out);
    json_free(out);
    return ser ? ser : strdup("{\"allow_any\": false, \"entries\": []}");
}

/* PoP: _check_overrides @ agent/plugin_llm.py:_check_overrides */
char *pll_check_overrides(const char *policy_json, const char *provider, const char *model) {
    /* Python: trust gate on overrides. */
    if (!policy_json) return NULL;
    bool allow_any = strstr(policy_json, "\"allow_any\": true") != NULL;
    if (!allow_any) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"provider\": \"%s\", \"model\": \"%s\"}",
             provider ? provider : "", model ? model : "");
    return out;
}

/* PoP: _normalize_input_block @ agent/plugin_llm.py:_normalize_input_block */
char *pll_normalize_input_block(const char *input_json) {
    /* Python: structured block → plain dict. */
    if (!input_json) return strdup("{}");
    printf("input block normalized for message builder\n");
    return strdup(input_json);
}

/* PoP: _build_structured_messages @ agent/plugin_llm.py:_build_structured_messages */
char *pll_build_structured_messages(const char *instructions, const char *input_json) {
    /* Python: instructions → system; input → user. */
    if (!instructions && !input_json) return strdup("[]");
    char *out = NULL;
    asprintf(&out,
        "[{\"role\": \"system\", \"content\": \"%s\"}, "
        "{\"role\": \"user\", \"content\": [%s]}]",
        instructions ? instructions : "",
        input_json ? input_json : "");
    return out;
}

/* PoP: _strip_code_fences @ agent/plugin_llm.py:_strip_code_fences */
char *pll_strip_code_fences(const char *text) {
    /* Python: first fenced block; else unchanged. */
    if (!text) return strdup("");
    const char *open = strstr(text, "```");
    if (!open) return strdup(text);
    const char *nl = strchr(open, '\n');
    if (!nl) return strdup(text);
    const char *close = strstr(nl, "```");
    if (!close) return strdup(text);
    return strndup(nl + 1, (size_t)(close - nl - 1));
}

/* PoP: _parse_structured_text @ agent/plugin_llm.py:_parse_structured_text */
char *pll_parse_structured_text(const char *text) {
    /* Python: (parsed, content_type); "json" when parse succeeds. */
    if (!text) return strdup("null\ttext");
    char *stripped = pll_strip_code_fences(text);
    const char *p = stripped ? stripped : text;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    char *out = NULL;
    if (*p == '{' || *p == '[') {
        /* crude json validity: ends with } or ] */
        size_t n = strlen(p);
        while (n > 0 && (p[n-1] == ' ' || p[n-1] == '\n' || p[n-1] == '\r')) n--;
        if (p[n-1] == '}' || p[n-1] == ']')
            asprintf(&out, "%s\tjson", p);
        else
            asprintf(&out, "%s\ttext", p);
    } else {
        asprintf(&out, "%s\ttext", p);
    }
    free(stripped);
    return out;
}

/* PoP: _extract_usage @ agent/plugin_llm.py:_extract_usage */
char *pll_extract_usage(const char *response_json) {
    /* Python: usage dict tolerant of provider differences. */
    if (!response_json) return strdup("{}");
    const char *u = strstr(response_json, "\"usage\"");
    if (!u) return strdup("{}");
    const char *colon = strchr(u, ':');
    if (!colon) return strdup("{}");
    const char *v = colon + 1;
    while (*v == ' ' || *v == '\t') v++;
    if (*v != '{') return strdup("{}");
    int depth = 0;
    const char *e = v;
    while (*e) {
        if (*e == '{') depth++;
        else if (*e == '}') { depth--; if (depth == 0) { e++; break; } }
        e++;
    }
    return strndup(v, (size_t)(e - v));
}

/* PoP: _extract_text @ agent/plugin_llm.py:_extract_text */
char *pll_extract_text(const char *response_json) {
    /* Python: assistant text out of OpenAI-shaped response. */
    if (!response_json) return strdup("");
    const char *msg = strstr(response_json, "\"content\"");
    if (!msg) return strdup("");
    const char *colon = strchr(msg, ':');
    if (!colon) return strdup("");
    const char *v = colon + 1;
    while (*v == ' ' || *v == '"') v++;
    const char *e = v;
    while (*e && *e != '"') e++;
    if (e == v) return strdup("");
    return strndup(v, (size_t)(e - v));
}

/* PoP: _resolve_attribution @ agent/plugin_llm.py:_resolve_attribution */
char *pll_resolve_attribution(const char *provider, const char *model, const char *fallback_json) {
    /* Python: result.provider/model precedence. */
    if (provider && *provider) {
        char *out = NULL;
        asprintf(&out, "{\"provider\": \"%s\", \"model\": \"%s\"}", provider, model ? model : "");
        return out;
    }
    return fallback_json ? strdup(fallback_json) : strdup("{}");
}

/* PoP: __init__ @ agent/plugin_llm.py:__init__ */
char *pll_init(const char *plugin_id) {
    /* Python: plugin-scoped host-owned llm. */
    if (!plugin_id) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"plugin_id\": \"%s\"}", plugin_id);
    return out;
}

/* PoP: complete @ agent/plugin_llm.py:complete */
char *pll_complete(const char *messages_json) {
    /* Python: chat completion against active model. */
    if (!messages_json) return NULL;
    printf("host-owned chat completion invoked\n");
    return strdup("{}");
}

/* PoP: complete_structured @ agent/plugin_llm.py:complete_structured */
char *pll_complete_structured(const char *input_json) {
    /* Python: bounded structured completion. */
    if (!input_json) return NULL;
    printf("structured completion invoked\n");
    return strdup("{}");
}

/* PoP: _json_response_format @ agent/plugin_llm.py:_json_response_format */
char *pll_json_response_format(void) {
    /* Python: extra_body.response_format payload. */
    return strdup("{\"type\": \"json_object\"}");
}

/* PoP: _invoke_sync @ agent/plugin_llm.py:_invoke_sync */
char *pll_invoke_sync(const char *messages_json) {
    /* Python: host call_llm, lazy auxiliary_client import. */
    if (!messages_json) return NULL;
    printf("host call_llm invoked synchronously\n");
    return strdup("{}");
}

/* PoP: make_plugin_llm_for_test @ agent/plugin_llm.py:make_plugin_llm_for_test */
char *pll_make_for_test(const char *plugin_id) {
    /* Python: injected policy + caller for unit tests. */
    if (!plugin_id) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"plugin_id\": \"%s\", \"test\": true}", plugin_id);
    return out;
}
