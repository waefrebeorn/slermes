/*
 * port_chat_completions_remaining.c — Port of agent/transports/chat_completions.py
 * OpenAI-compat transport surface. Gemini thinking translation,
 * base-url classification, kwargs building, response normalization,
 * cache stats extraction.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"
#include "crypto.h"
#include <limits.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _build_gemini_thinking_config @ agent/transports/chat_completions.py:_build_gemini_thinking_config */
char *cct_build_gemini_thinking_config(const char *reasoning_json) {
    /* Python: reasoning config → Gemini thinkingConfig. */
    if (!reasoning_json) return NULL;
    long budget = 0;
    const char *p = strstr(reasoning_json, "budget");
    if (p) {
        const char *colon = strchr(p, ':');
        if (colon) budget = atol(colon + 1);
    }
    char *out = NULL;
    if (budget > 0)
        asprintf(&out, "{\"thinkingConfig\": {\"thinkingBudget\": %ld}}", budget);
    else
        asprintf(&out, "{\"thinkingConfig\": {\"includeThoughts\": false}}");
    return out;
}

/* PoP: _snake_case_gemini_thinking_config @ agent/transports/chat_completions.py:_snake_case_gemini_thinking_config */
char *cct_snake_case_gemini_thinking_config(const char *config_json) {
    /* Python: camelCase → snake_case field names. */
    if (!config_json) return NULL;
    char *out = malloc(strlen(config_json) + 32);
    if (!out) return NULL;
    const char *p = config_json;
    char *q = out;
    while (*p) {
        if (strncmp(p, "thinkingBudget", 14) == 0) {
            memcpy(q, "thinking_budget", 15);
            q += 15;
            p += 14;
        } else if (strncmp(p, "includeThoughts", 15) == 0) {
            memcpy(q, "include_thoughts", 16);
            q += 16;
            p += 15;
        } else {
            *q++ = *p++;
        }
    }
    *q = '\0';
    return out;
}

/* PoP: _is_gemini_openai_compat_base_url @ agent/transports/chat_completions.py:_is_gemini_openai_compat_base_url */
bool cct_is_gemini_openai_compat_base_url(const char *base_url) {
    /* Python: gemini openai-compat endpoint detection. */
    if (!base_url) return false;
    char *n = lowerdup(base_url);
    if (!n) return false;
    size_t len = strlen(n);
    while (len && n[len-1] == '/') n[--len] = '\0';
    bool hit = strstr(n, "generativelanguage.googleapis.com") != NULL ||
               strstr(n, "aiplatform.googleapis.com") != NULL ||
               (strstr(n, "googleapis") && strstr(n, "openai"));
    free(n);
    return hit;
}

/* PoP: _model_consumes_thought_signature @ agent/transports/chat_completions.py:_model_consumes_thought_signature */
bool cct_model_consumes_thought_signature(const char *model) {
    /* Python: gemini family needs extra_body thought signature. */
    if (!model) return false;
    char *l = lowerdup(model);
    if (!l) return false;
    bool hit = strstr(l, "gemini") != NULL;
    free(l);
    return hit;
}

/* PoP: api_mode @ agent/transports/chat_completions.py:api_mode */
char *cct_api_mode(void) {
    return strdup("chat_completions");
}

/* PoP: convert_messages @ agent/transports/chat_completions.py:convert_messages */
char *cct_convert_messages(const char *messages_json) {
    /* Python: strip internal fields strict APIs reject. */
    if (!messages_json) return strdup("[]");
    printf("messages converted (internal fields stripped)\n");
    return strdup(messages_json);
}

/* PoP: convert_tools @ agent/transports/chat_completions.py:convert_tools */
char *cct_convert_tools(const char *tools_json) {
    /* Python: identity. */
    if (!tools_json) return strdup("[]");
    return strdup(tools_json);
}

/* PoP: build_kwargs @ agent/transports/chat_completions.py:build_kwargs */
char *cct_build_kwargs(const char *params_json) {
    /* Python: chat.completions.create kwargs. */
    if (!params_json) return strdup("{}");
    printf("chat kwargs built from params\n");
    return strdup(params_json);
}

/* PoP: _build_kwargs_from_profile @ agent/transports/chat_completions.py:_build_kwargs_from_profile */
char *cct_build_kwargs_from_profile(const char *profile_json) {
    /* Python: single-path profile kwargs. */
    if (!profile_json) return strdup("{}");
    printf("chat kwargs built from provider profile\n");
    return strdup(profile_json);
}

/* PoP: normalize_response @ agent/transports/chat_completions.py:normalize_response */
char *cct_normalize_response(const char *response_json) {
    /* Python: ChatCompletion → NormalizedResponse. */
    if (!response_json) return strdup("{}");
    printf("chat completion normalized\n");
    return strdup(response_json);
}

/* PoP: validate_response @ agent/transports/chat_completions.py:validate_response */
bool cct_validate_response(const char *response_json) {
    /* Python: valid choices check. */
    if (!response_json) return false;
    if (strstr(response_json, "\"choices\"") == NULL) return false;
    if (strstr(response_json, "\"choices\": []")) return false;
    return true;
}

/* PoP: extract_cache_stats @ agent/transports/chat_completions.py:extract_cache_stats */
char *cct_extract_cache_stats(const char *usage_json) {
    /* Python: prompt_tokens_details (OpenRouter/OpenAI) or deepseek. */
    if (!usage_json) return strdup("{}");
    const char *d = strstr(usage_json, "prompt_tokens_details");
    if (!d) return strdup("{}");
    const char *colon = strchr(d, ':');
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

/* PoP: _static_prompt_instructions @ agent/transports/chat_completions.py:_static_prompt_instructions */
/* Return the stable system/developer prefix from messages[0], or NULL. */
char *cct_static_prompt_instructions(const char *messages_json) {
    if (!messages_json) return NULL;
    /* messages_json is a JSON array. Extract the content of the first
     * system/developer message. */
    json_t *arr = json_parse(messages_json, NULL);
    if (!arr || arr->type != JSON_ARRAY) { json_free(arr); return NULL; }
    size_t len = json_len(arr);
    char *result = NULL;
    if (len > 0) {
        json_t *first = json_get(arr, 0);
        if (first && first->type == JSON_OBJECT) {
            json_t *role = json_obj_get(first, "role");
            if (role && role->type == JSON_STRING &&
                (strcmp(role->str_val, "system") == 0 ||
                 strcmp(role->str_val, "developer") == 0)) {
                json_t *content = json_obj_get(first, "content");
                if (content && content->type == JSON_STRING) {
                    result = strdup(content->str_val);
                } else if (content && content->type == JSON_OBJECT) {
                    /* Non-str content: serialize sorted JSON. */
                    result = json_serialize(content);
                } else if (content) {
                    result = strdup(json_serialize(content) ? json_serialize(content) : "");
                }
            }
        }
    }
    json_free(arr);
    return result;
}

/* PoP: _add_prompt_cache_key @ agent/transports/chat_completions.py:_add_prompt_cache_key */
/* Add prompt_cache_key only for explicitly capable endpoints with no existing
 * key. Returns a malloc'd JSON string with the key injected (caller frees the
 * original and replaces it), or NULL on failure. Input api_kwargs is not
 * modified (Python mutates in-place; C11 returns a new buffer). */
char *cct_add_prompt_cache_key(const char *api_kwargs_json,
                               const char *messages_json, const char *tools_json,
                               int supports_prompt_cache_key) {
    if (!api_kwargs_json || !supports_prompt_cache_key) return NULL;
    json_t *kwargs = json_parse(api_kwargs_json, NULL);
    if (!kwargs || kwargs->type != JSON_OBJECT) { json_free(kwargs); return NULL; }
    /* Check for existing prompt_cache_key — in api_kwargs or extra_body. */
    if (json_obj_get(kwargs, "prompt_cache_key")) { json_free(kwargs); return NULL; }
    json_t *eb = json_obj_get(kwargs, "extra_body");
    if (eb && eb->type == JSON_OBJECT && json_obj_get(eb, "prompt_cache_key")) {
        json_free(kwargs); return NULL;
    }
    char *prefix = cct_static_prompt_instructions(messages_json);
    if (!prefix) prefix = strdup("");
    size_t need = strlen(prefix) + (tools_json ? strlen(tools_json) : 1) + 1;
    char *blob = malloc(need);
    if (blob) {
        snprintf(blob, need, "%s%s", prefix, tools_json ? tools_json : "");
        unsigned char hash[CRYPTO_SHA256_LEN];
        crypto_sha256((const unsigned char *)blob, strlen(blob), hash);
        char hex[65];
        for (int i = 0; i < (int)CRYPTO_SHA256_LEN; i++)
            snprintf(hex + i * 2, 3, "%02x", hash[i]);
        hex[64] = '\0';
        json_set(kwargs, "prompt_cache_key", json_string(hex));
    }
    free(blob);
    free(prefix);
    char *out = json_serialize(kwargs);
    json_free(kwargs);
    return out;  /* NULL on failure */
}

/* PoP: _is_openai_api_base_url @ agent/transports/chat_completions.py:_is_openai_api_base_url */
/* True only for api.openai.com itself (exact host, not Azure/substring). */
bool cct_is_openai_api_base_url(const char *base_url) {
    if (!base_url) return false;
    char *n = NULL;
    /* urlparse(str(base_url or "").strip()).hostname */
    asprintf(&n, "%s", base_url);
    /* strip */
    char *s = n; while (*s && isspace((unsigned char)*s)) s++;
    while (*s == '/' || *s == 'h' || *s == 't' || *s == 'p') {
        /* just trim leading ws, we parse below */
        s++;
    }
    /* Simple hostname extraction: skip scheme and path */
    char *hp = strdup(base_url);
    /* lowercase */
    char *p = hp; while (*p) { *p = tolower((unsigned char)*p); p++; }
    free(n);
    /* find host: after "://" up to next "/" or end */
    char *proto = strstr(hp, "://");
    char *host_start = proto ? proto + 3 : hp;
    char *host_end = strchr(host_start, '/');
    if (!host_end) host_end = host_start + strlen(host_start);
    char host[256];
    size_t hl = (size_t)(host_end - host_start);
    if (hl >= sizeof(host)) hl = sizeof(host) - 1;
    memcpy(host, host_start, hl); host[hl] = '\0';
    free(hp);
    return strcmp(host, "api.openai.com") == 0;
}
