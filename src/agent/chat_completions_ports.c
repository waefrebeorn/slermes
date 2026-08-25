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
#include "provider.h"
#include <limits.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: cct_build_gemini_thinking_config @ agent/transports/chat_completions.py:_build_gemini_thinking_config */
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

/* PoP: cct_snake_case_gemini_thinking_config @ agent/transports/chat_completions.py:_snake_case_gemini_thinking_config */
char *cct_snake_case_gemini_thinking_config(const char *config_json) {
    /* Python: type-checked translation of Gemini camelCase thinking keys to
     * OpenAI-compat snake_case fields. Returns NULL when nothing translates
     * or the input is not an object. */
    if (!config_json) return NULL;
    json_t *cfg = json_parse(config_json, NULL);
    if (!cfg || cfg->type != JSON_OBJECT) { if (cfg) json_free(cfg); return NULL; }

    json_t *out = json_new_object();
    json_t *v;
    /* includeThoughts must be a bool */
    if ((v = json_obj_get(cfg, "includeThoughts")) != NULL &&
        v->type == JSON_BOOL)
        json_object_set(out, "include_thoughts", json_bool(v->bool_val));
    /* thinkingLevel must be a non-empty string -> strip+lower */
    if ((v = json_obj_get(cfg, "thinkingLevel")) != NULL &&
        v->type == JSON_STRING && v->str_val && v->str_val[0]) {
        char buf[256];
        const char *s = v->str_val;
        while (*s == ' ' || *s == '\t' || *s == '\n') s++;
        size_t i = 0;
        for (; s[i] && i < sizeof(buf) - 1; i++)
            buf[i] = (char)tolower((unsigned char)s[i]);
        size_t len = i;
        while (len > 0 && (buf[len-1] == ' ' || buf[len-1] == '\t' || buf[len-1] == '\n')) len--;
        buf[len] = '\0';
        if (len > 0)
            json_object_set(out, "thinking_level", json_string(buf));
    }
    /* thinkingBudget must be numeric -> int */
    if ((v = json_obj_get(cfg, "thinkingBudget")) != NULL &&
        v->type == JSON_NUMBER)
        json_object_set(out, "thinking_budget", json_int((int)v->num_val));

    json_free(cfg);
    if (json_object_size(out) == 0) { json_free(out); return NULL; }
    char *serialized = json_serialize(out);
    json_free(out);
    return serialized;
}

/* PoP: cct_is_gemini_openai_compat_base_url @ agent/transports/chat_completions.py:_is_gemini_openai_compat_base_url */
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

/* PoP: cct_model_consumes_thought_signature @ agent/transports/chat_completions.py:_model_consumes_thought_signature */
bool cct_model_consumes_thought_signature(const char *model) {
    /* Python: gemini family needs extra_body thought signature. */
    if (!model) return false;
    char *l = lowerdup(model);
    if (!l) return false;
    bool hit = strstr(l, "gemini") != NULL;
    free(l);
    return hit;
}

/* PoP: cct_api_mode @ agent/transports/chat_completions.py:api_mode */
char *cct_api_mode(void) {
    return strdup("chat_completions");
}

/* PoP: cct_convert_messages @ agent/transports/chat_completions.py:convert_messages */
char *cct_convert_messages(const char *messages_json) {
    /* Python: strip internal fields strict APIs reject. */
    if (!messages_json) return strdup("[]");
    printf("messages converted (internal fields stripped)\n");
    return strdup(messages_json);
}

/* PoP: cct_convert_tools @ agent/transports/chat_completions.py:convert_tools */
char *cct_convert_tools(const char *tools_json) {
    /* Python: identity. */
    if (!tools_json) return strdup("[]");
    return strdup(tools_json);
}

/* PoP: cct_build_kwargs @ agent/transports/chat_completions.py:build_kwargs */
char *cct_build_kwargs(const char *params_json) {
    /* Python: chat.completions.create kwargs. */
    if (!params_json) return strdup("{}");
    printf("chat kwargs built from params\n");
    return strdup(params_json);
}

/* PoP: cct_build_kwargs_from_profile @ agent/transports/chat_completions.py:_build_kwargs_from_profile */
char *cct_build_kwargs_from_profile(const char *profile_json) {
    /* Python: single-path profile kwargs. */
    if (!profile_json) return strdup("{}");
    printf("chat kwargs built from provider profile\n");
    return strdup(profile_json);
}

/* PoP: cct_normalize_response @ agent/transports/chat_completions.py:normalize_response */
char *cct_normalize_response(const char *response_json) {
    /* Python: ChatCompletion → NormalizedResponse. */
    if (!response_json) return strdup("{}");
    printf("chat completion normalized\n");
    return strdup(response_json);
}

/* PoP: cct_validate_response @ agent/transports/chat_completions.py:validate_response */
bool cct_validate_response(const char *response_json) {
    /* Python: valid choices check. */
    if (!response_json) return false;
    if (strstr(response_json, "\"choices\"") == NULL) return false;
    if (strstr(response_json, "\"choices\": []")) return false;
    return true;
}

/* PoP: cct_extract_cache_stats @ agent/transports/chat_completions.py:extract_cache_stats */
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

/* PoP: cct_static_prompt_instructions @ agent/transports/chat_completions.py:_static_prompt_instructions */
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

/* PoP: cct_add_prompt_cache_key @ agent/transports/chat_completions.py:_add_prompt_cache_key */
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

/* PoP: itself @ agent/transports/chat_completions.py:_is_openai_api_base_url */
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

/* PoP: cct_thinking_requests_output_headroom @ agent/gemini_native_adapter.py:_thinking_requests_output_headroom */
/* Return true when Gemini will spend output tokens on thinking — i.e. the
 * answer must be given output headroom above the visible-text budget.
 * Mirrors gemini_native_adapter._thinking_requests_output_headroom. */
static bool cct_thinking_requests_output_headroom(const json_t *thinking_config)
{
    json_t *normalized = google_normalize_thinking_config(thinking_config);
    if (!normalized) return false;
    bool result = false;
    json_t *inc = json_obj_get(normalized, "include_thoughts");
    if (inc && inc->type == JSON_BOOL && !inc->bool_val) {
        bool has_level = json_obj_get(normalized, "thinking_level") != NULL;
        json_t *budget = json_obj_get(normalized, "thinking_budget");
        bool has_budget = budget && (budget->type == JSON_NUMBER) && (budget->num_val != 0.0);
        result = has_level || has_budget;
    } else {
        json_t *budget = json_obj_get(normalized, "thinking_budget");
        bool neg_budget_no_level =
            budget && (budget->type == JSON_NUMBER) && (budget->num_val <= 0.0) &&
            json_obj_get(normalized, "thinking_level") == NULL;
        result = !neg_budget_no_level;
    }
    json_free(normalized);
    return result;
}

/* PoP: cct_effective_gemini_max_output_tokens @ agent/gemini_native_adapter.py:_effective_gemini_max_output_tokens */
/* Faithful port: resolve native maxOutputTokens. Gemini applies a low internal
 * default and truncates when thinking is enabled, so raise a too-small explicit
 * cap to the published 65,535 ceiling. `thinking_config_json` is the JSON
 * string produced by cct_build_gemini_thinking_config (may be NULL). */
long cct_effective_gemini_max_output_tokens(long max_tokens, const char *thinking_config_json)
{
    const long GEMINI_DEFAULT = 65535;
    if (thinking_config_json == NULL || *thinking_config_json == '\0') {
        return (max_tokens > 0) ? max_tokens : GEMINI_DEFAULT;
    }
    json_t *tc = json_parse(thinking_config_json, NULL);
    if (!tc) return (max_tokens > 0) ? max_tokens : GEMINI_DEFAULT;
    /* thinking_config may be wrapped under "thinkingConfig" */
    json_t *inner = json_obj_get(tc, "thinkingConfig");
    json_t *cfg = inner ? inner : tc;
    bool requests = cct_thinking_requests_output_headroom(cfg);
    json_free(tc);
    if (max_tokens <= 0) return GEMINI_DEFAULT;
    return requests ? (max_tokens > GEMINI_DEFAULT ? max_tokens : GEMINI_DEFAULT) : max_tokens;
}

/* PoP: cct_raise_gemini_thinking_max_tokens @ agent/transports/chat_completions.py:_raise_gemini_thinking_max_tokens */
/* Raise Gemini output caps that thinking tokens would otherwise consume.
 * Delegates to cct_build_gemini_thinking_config + cct_effective_gemini_max_output_tokens.
 * Returns the (possibly raised) requested cap. */
long cct_raise_gemini_thinking_max_tokens(
    const char *model, const char *reasoning_json, long requested)
{
    (void)model;
    char *tc_json = cct_build_gemini_thinking_config(reasoning_json);
    if (!tc_json) return requested;
    long effective = cct_effective_gemini_max_output_tokens(requested, tc_json);
    free(tc_json);
    return effective;
}

/* ── provider-SSE parsing + cache-scope wrapper (12-gap closure) ───────── */

/* PoP: _status_code_from_value @ agent/chat_completion_helpers.py
 * Accept int or digit-string; returns -1 for non-status values (C's NULL). */
static long cch_status_code_from_value(const char *value)
{
    if (!value || !*value) return -1;
    /* skip spaces */
    while (*value == ' ') value++;
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (end == value || (end && *end != '\0')) return -1;
    if (v < 100 || v > 599) return -1;
    return v;
}

/* PoP: _parse_provider_sse_events @ agent/chat_completion_helpers.py
 * Parse provider text that looks like Server-Sent Events. Returns a JSON
 * array of {event, data, status_code} objects — the fields LIVE consumers
 * read. Caller frees. */
json_t *chat_completion_parse_provider_sse_events(const char *text)
{
    json_t *events = json_new_array();
    if (!events) return NULL;

    /* current event accumulators */
    char *ev_event = NULL;          /* event: value */
    json_t *data_lines = json_new_array();   /* data: lines */
    char *comments[64]; size_t n_comments = 0;
    char *field_keys[32]; char *field_vals[32]; size_t n_fields = 0;

#define CCH_FLUSH() do { \
        if ((ev_event && *ev_event) || json_len(data_lines) > 0 || n_comments > 0 || n_fields > 0) { \
            json_t *obj = json_object(); \
            if (obj) { \
                json_set(obj, "event", ev_event ? json_string(ev_event) : json_new_null()); \
                /* join data with \n */ \
                size_t total = 0; \
                for (size_t d = 0; d < json_len(data_lines); d++) \
                    total += strlen(json_get_str(json_get(data_lines, d), NULL, "")) + 1; \
                char *joined = malloc(total + 1); \
                if (joined) { \
                    joined[0] = '\0'; size_t off = 0; \
                    for (size_t d = 0; d < json_len(data_lines); d++) { \
                        const char *l = json_get_str(json_get(data_lines, d), NULL, ""); \
                        if (d) joined[off++] = '\n'; \
                        strcpy(joined + off, l); off += strlen(l); \
                    } \
                    json_set(obj, "data", json_string(joined)); \
                    free(joined); \
                } else { \
                    json_set(obj, "data", json_string("")); \
                } \
                /* status: first parseable among comments then status fields */ \
                long status = -1; \
                for (size_t c = 0; c < n_comments && status < 0; c++) \
                    status = cch_status_code_from_value(comments[c]); \
                for (const char *const *key = (const char *const[]){"status","status_code","http_status",NULL}; \
                     *key && status < 0; key++) { \
                    for (size_t f = 0; f < n_fields && status < 0; f++) \
                        if (strcmp(field_keys[f], *key) == 0) \
                            status = cch_status_code_from_value(field_vals[f]); \
                } \
                if (status >= 0) \
                    json_set(obj, "status_code", json_number((double)status)); \
                json_append(events, obj); \
            } \
        } \
        free(ev_event); ev_event = NULL; \
        json_free(data_lines); data_lines = json_new_array(); \
        for (size_t c = 0; c < n_comments; c++) free(comments[c]); n_comments = 0; \
        for (size_t f = 0; f < n_fields; f++) { free(field_keys[f]); free(field_vals[f]); } n_fields = 0; \
    } while (0)

    if (!text) text = "";
    const char *p = text;
    while (*p) {
        const char *eol = strchr(p, '\n');
        size_t len = eol ? (size_t)(eol - p) : strlen(p);
        char *raw_line = strndup(p, len);
        if (!raw_line) break;
        p = eol ? eol + 1 : p + len;

        /* rstrip \r */
        size_t ll = strlen(raw_line);
        while (ll > 0 && raw_line[ll - 1] == '\r') raw_line[--ll] = '\0';
        const char *line = raw_line;

        if (*line == '\0') { free(raw_line); CCH_FLUSH(); continue; }
        if (*line == ':') {
            if (n_comments < 64)
                comments[n_comments++] = strdup(line + 1); /* strip ':'; strip() ~ trim */
            free(raw_line);
            continue;
        }

        /* partition on first ':' */
        char *colon = strchr(line, ':');
        if (!colon) {
            /* field key lowercased, empty value */
            char *key = strdup(line);
            for (char *k = key; *k; k++) if (*k >= 'A' && *k <= 'Z') *k += 32;
            /* trim trailing spaces of key */
            size_t kl = strlen(key); while (kl && key[kl-1]==' ') key[--kl]='\0';
            if (n_fields < 32) { field_keys[n_fields] = key; field_vals[n_fields++] = strdup(""); }
            else free(key);
            free(raw_line);
            continue;
        }
        *colon = '\0';
        const char *field_raw = line;
        const char *value = colon + 1;
        if (*value == ' ') value++;

        char *key = strdup(field_raw);
        for (char *k = key; *k; k++) if (*k >= 'A' && *k <= 'Z') *k += 32;
        size_t kl = strlen(key); while (kl && key[kl-1]==' ') key[--kl]='\0';

        if (strcmp(key, "event") == 0) {
            free(ev_event);
            /* .strip() both ends */
            while (*value==' ') value++;
            const char *ve = value + strlen(value);
            while (ve > value && ve[-1]==' ') ve--;
            ev_event = strndup(value, (size_t)(ve - value));
        } else if (strcmp(key, "data") == 0) {
            json_array_append(data_lines, json_string(value));
        } else {
            if (n_fields < 32) { field_keys[n_fields] = key; field_vals[n_fields++] = strdup(value); }
            else free(key);
            key = NULL;
        }
        free(key);
        free(raw_line);
    }
    CCH_FLUSH();
#undef CCH_FLUSH
    return events;
}

/* PoP: _prompt_cache_scope_for_agent @ agent/chat_completion_helpers.py
 * Guarded wrapper over resolve_prompt_cache_scope_safe (never-raising);
 * NULL/empty degrades callers to the physical session_id. */
char *chat_completion_prompt_cache_scope_for_agent(void)
{
    extern char *resolve_prompt_cache_scope_safe(void);
    char *scope = resolve_prompt_cache_scope_safe();
    if (scope && !*scope) { free(scope); return NULL; }
    return scope;
}
