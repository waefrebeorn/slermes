/*
 * reasoning_params.c — C port of upstream Hermes agent/reasoning_params.py.
 *
 * PoP: w8_reasoningpa_062303_should_sanitize_tool_calls @ agent/reasoning_params.py:ReasoningParamsMixin._should_sanitize_tool_calls
 * PoP: w8_reasoningpa_062303_reasoning_echo_opt_in @ agent/reasoning_params.py:ReasoningParamsMixin._reasoning_echo_opt_in
 * PoP: w8_reasoningpa_062303_needs_kimi_tool_reasoning @ agent/reasoning_params.py:ReasoningParamsMixin._needs_kimi_tool_reasoning
 * PoP: w8_reasoningpa_062303_needs_deepseek_tool_reasoning @ agent/reasoning_params.py:ReasoningParamsMixin._needs_deepseek_tool_reasoning
 * PoP: w8_reasoningpa_062303_needs_mimo_tool_reasoning @ agent/reasoning_params.py:ReasoningParamsMixin._needs_mimo_tool_reasoning
 * PoP: w8_reasoningpa_062303_sanitize_tool_calls_for_strict_api @ agent/reasoning_params.py:ReasoningParamsMixin._sanitize_tool_calls_for_strict_api
 * PoP: w8_reasoningpa_062303_supports_reasoning_extra_body @ agent/reasoning_params.py:ReasoningParamsMixin._supports_reasoning_extra_body
 *
 * External collaborators (matches_reasoning_echo_family,
 * base_url_host_matches, _model_consumes_thought_signature, ask_profile,
 * endpoint profile / probe helpers) are injected from the fixture.
 */
#include "noc_w8_reasoningpa_062303.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: return v->c.count > 0 ? 1 : 0;
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default: return 0;
    }
}
static char *lower_dup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    for (size_t i = 0; i < n; i++) o[i] = (char)tolower((unsigned char)s[i]);
    o[n] = '\0';
    return o;
}
static char *strip_lower_dup(const char *s) {
    if (!s) return NULL;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    for (size_t i = 0; i < n; i++) o[i] = (char)tolower((unsigned char)s[i]);
    o[n] = '\0';
    return o;
}
/* injected base_url_host_matches(url, host) == (host in matches) */
static int host_matches(const json_t *matches, const char *host) {
    if (!matches || matches->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < matches->c.count; i++) {
        const json_t *m = matches->c.items[i];
        if (m && m->type == JSON_STRING && m->str_val && !strcmp(m->str_val, host)) return 1;
    }
    return 0;
}

int w8_reasoningpa_062303_should_sanitize_tool_calls(const struct json_t *api_mode) {
    return strcmp(sval(api_mode), "codex_responses") != 0 ? 1 : 0;
}

int w8_reasoningpa_062303_reasoning_echo_opt_in(const struct json_t *flag) {
    return truthy(flag);
}

/* key = "fam|provider|model|base_url" with None -> "" */
static int family_match(const json_t *args, const char *fam, const char *provider,
                        const char *model, const char *base_url) {
    const json_t *map = json_obj_get(args, "family_matches");
    char key[1024];
    snprintf(key, sizeof(key), "%s|%s|%s|%s", fam, provider ? provider : "",
             model ? model : "", base_url ? base_url : "");
    const json_t *v = map ? json_obj_get(map, key) : NULL;
    return truthy(v);
}

int w8_reasoningpa_062303_needs_kimi_tool_reasoning(const struct json_t *args) {
    return family_match(args, "kimi", sval(json_obj_get(args, "provider")), "",
                        sval(json_obj_get(args, "base_url")));
}
int w8_reasoningpa_062303_needs_deepseek_tool_reasoning(const struct json_t *args) {
    char *prov = lower_dup(sval(json_obj_get(args, "provider")));
    int r = family_match(args, "deepseek", prov, sval(json_obj_get(args, "model")),
                        sval(json_obj_get(args, "base_url")));
    free(prov);
    return r;
}
int w8_reasoningpa_062303_needs_mimo_tool_reasoning(const struct json_t *args) {
    char *prov = lower_dup(sval(json_obj_get(args, "provider")));
    int r = family_match(args, "mimo", prov, sval(json_obj_get(args, "model")),
                        sval(json_obj_get(args, "base_url")));
    free(prov);
    return r;
}

json_t *w8_reasoningpa_062303_sanitize_tool_calls_for_strict_api(
    const json_t *api_msg, int keep_signature) {
    json_t *out = json_copy(api_msg);
    if (!out || out->type != JSON_OBJECT) return out ? out : json_null();
    const json_t *tcs = json_obj_get(out, "tool_calls");
    if (!tcs || tcs->type != JSON_ARRAY) return out;

    json_t *arr = json_array();
    for (size_t i = 0; i < tcs->c.count; i++) {
        json_t *tc = tcs->c.items[i];
        if (!tc || tc->type != JSON_OBJECT) { json_append(arr, json_copy(tc)); continue; }
        json_t *nt = json_object();
        for (size_t j = 0; j < tc->c.count; j++) {
            const char *k = tc->c.keys[j];
            if (!k) continue;
            if (!strcmp(k, "call_id") || !strcmp(k, "response_item_id")) continue;
            if (!keep_signature && !strcmp(k, "extra_content")) continue;
            json_set(nt, k, json_copy(tc->c.items[j]));
        }
        json_append(arr, nt);
    }
    json_set(out, "tool_calls", arr);
    return out;
}

static const char *PREFIXES[] = {
    "deepseek/", "anthropic/", "openai/", "x-ai/", "google/gemini-2",
    "google/gemma-4", "qwen/qwen3", "tencent/hy", "xiaomi/",
};

int w8_reasoningpa_062303_supports_reasoning_extra_body(const struct json_t *args) {
    const json_t *matches = json_obj_get(args, "matches");
    const json_t *provider = json_obj_get(args, "provider");
    const json_t *model = json_obj_get(args, "model");
    const json_t *profile = json_obj_get(args, "profile");
    const json_t *ask = json_obj_get(args, "ask_profile_value");
    const json_t *opts = json_obj_get(args, "lmstudio_options");
    const json_t *ollama = json_obj_get(args, "ollama_thinking");
    const json_t *is_or = json_obj_get(args, "is_openrouter");
    const json_t *caps = json_obj_get(args, "caps");

    if (host_matches(matches, "nousresearch.com") ||
        host_matches(matches, "ai-gateway.vercel.sh")) return 1;

    if (profile && profile->type != JSON_NULL)
        return ask && ask->type != JSON_NULL ? 1 : 0;

    char *prov = strip_lower_dup(sval(provider));
    if (prov && !strcmp(prov, "lmstudio")) {
        int ok = 0;
        if (opts && opts->type == JSON_ARRAY) {
            for (size_t i = 0; i < opts->c.count; i++) {
                const json_t *o = opts->c.items[i];
                if (truthy(o) && !(o->type == JSON_STRING && o->str_val && !strcmp(o->str_val, "off"))) {
                    ok = 1; break;
                }
            }
        }
        free(prov);
        return ok;
    }
    free(prov);

    if (host_matches(matches, "ollama.com")) return truthy(ollama);

    if (!truthy(is_or) || host_matches(matches, "api.mistral.ai")) return 0;

    if (caps && caps->type == JSON_OBJECT)
        return truthy(json_obj_get(caps, "supports_reasoning"));

    char *ml = lower_dup(sval(model));
    int ok = 0;
    for (size_t i = 0; i < sizeof(PREFIXES) / sizeof(PREFIXES[0]); i++) {
        if (ml && strncmp(ml, PREFIXES[i], strlen(PREFIXES[i])) == 0) { ok = 1; break; }
    }
    free(ml);
    return ok;
}
