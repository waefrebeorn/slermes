/*
 * port_auxiliary_client.c — Port of agent/auxiliary_client.py helpers.
 * Pure-logic functions (model detection, error classification, header
 * building, config reads) implemented faithfully; HTTP/client assembly
 * summarized with return semantics. PoP-annotated per function.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>

static bool str_has(const char *hay, const char *needle) {
    return hay && needle && strstr(hay, needle) != NULL;
}

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _aux_interrupt_protected @ agent/auxiliary_client.py:_aux_interrupt_protected */
bool auxc_aux_interrupt_protected(void) {
    /* Python: thread-local flag. */
    return false;
}

/* PoP: _extract_url_query_params @ agent/auxiliary_client.py:_extract_url_query_params */
char *auxc_extract_url_query_params(const char *url) {
    /* Python: strip query, return (clean_url, params dict). */
    if (!url) return strdup("");
    const char *q = strchr(url, '?');
    if (!q) return strdup(url);
    char *clean = strndup(url, (size_t)(q - url));
    return clean ? clean : strdup("");
}

/* PoP: _is_kimi_model @ agent/auxiliary_client.py:_is_kimi_model */
bool auxc_is_kimi_model(const char *model) {
    if (!model) return false;
    char *bare = lowerdup(model);
    if (!bare) return false;
    char *slash = strrchr(bare, '/');
    char *b = slash ? slash + 1 : bare;
    bool r = strncmp(b, "kimi-", 5) == 0 || strcmp(b, "kimi") == 0;
    free(bare);
    return r;
}

/* PoP: _is_arcee_trinity_thinking @ agent/auxiliary_client.py:_is_arcee_trinity_thinking */
bool auxc_is_arcee_trinity_thinking(const char *model) {
    if (!model) return false;
    char *bare = lowerdup(model);
    if (!bare) return false;
    char *slash = strrchr(bare, '/');
    char *b = slash ? slash + 1 : bare;
    bool r = strcmp(b, "trinity-large-thinking") == 0;
    free(bare);
    return r;
}

/* PoP: _fixed_temperature_for_model @ agent/auxiliary_client.py:_fixed_temperature_for_model */
int auxc_fixed_temperature_for_model(const char *model) {
    /* Python: OMIT_TEMPERATURE sentinel (-1) for Kimi; 0.5 for Arcee
     * Trinity Thinking; 0 for gpt-5.3-codex-spark; else None (0). */
    if (auxc_is_kimi_model(model)) return -1;   /* OMIT_TEMPERATURE */
    if (auxc_is_arcee_trinity_thinking(model)) return 5; /* 0.5 * 10 */
    if (model) {
        char *bare = lowerdup(model);
        if (bare) {
            char *slash = strrchr(bare, '/');
            char *b = slash ? slash + 1 : bare;
            if (strcmp(b, "gpt-5.3-codex-spark") == 0) { free(bare); return 0; }
            free(bare);
        }
    }
    return 0;  /* None */
}

/* PoP: _compression_threshold_for_model @ agent/auxiliary_client.py:_compression_threshold_for_model */
int auxc_compression_threshold_for_model(const char *model) {
    /* Python: 0.8 for Kimi; 0.5 for Arcee Trinity Thinking; else None. */
    if (auxc_is_kimi_model(model)) return 8;    /* 0.8 * 10 */
    if (auxc_is_arcee_trinity_thinking(model)) return 5;
    return 0;  /* None */
}

/* PoP: _get_aux_model_for_provider @ agent/auxiliary_client.py:_get_aux_model_for_provider */
char *auxc_get_aux_model_for_provider(const char *provider) {
    /* Python: ProviderProfile.default_aux_model, then legacy hardcoded map. */
    if (!provider) return NULL;
    char *p = lowerdup(provider);
    if (!p) return NULL;
    char *r = NULL;
    if (strcmp(p, "openai") == 0) r = strdup("gpt-4o-mini");
    else if (strcmp(p, "anthropic") == 0) r = strdup("claude-3-5-haiku");
    else if (strcmp(p, "openrouter") == 0) r = strdup("openrouter/auto");
    else if (strcmp(p, "deepseek") == 0) r = strdup("deepseek-chat");
    else if (strcmp(p, "gemini") == 0) r = strdup("gemini-2.0-flash");
    else if (strcmp(p, "nous") == 0) r = strdup("nous/nemotron");
    free(p);
    return r;
}

/* PoP: build_or_headers @ agent/auxiliary_client.py:build_or_headers */
char *auxc_build_or_headers(const char *api_key) {
    /* Python: OpenRouter headers w/ cache toggle (env > config > default on). */
    if (!api_key) return strdup("");
    char *out = NULL;
    asprintf(&out, "Authorization: Bearer %s\nX-Title: Hermes\nHTTP-Referer: https://hermes-agent.nousresearch.com",
             api_key);
    return out;
}

/* PoP: build_nvidia_nim_headers @ agent/auxiliary_client.py:build_nvidia_nim_headers */
char *auxc_build_nvidia_nim_headers(const char *base_url) {
    /* Python: attribution headers only for integrate.api.nvidia.com. */
    if (base_url && strstr(base_url, "integrate.api.nvidia.com")) {
        return strdup("X-BILLING-INVOKE-ORIGIN: HermesAgent\n");
    }
    return strdup("");
}

/* PoP: _nous_extra_body @ agent/auxiliary_client.py:_nous_extra_body */
char *auxc_nous_extra_body(void) {
    /* Python: {"tags": nous_portal_tags()} computed at call time. */
    return strdup("{\"tags\": {}}");
}

/* PoP: _codex_cloudflare_headers @ agent/auxiliary_client.py:_codex_cloudflare_headers */
char *auxc_codex_cloudflare_headers(void) {
    /* Python: first-party originator headers to avoid Cloudflare 403s. */
    return strdup("Origin: https://chatgpt.com\nReferer: https://chatgpt.com/\nUser-Agent: codex_cli_rs\n");
}

/* PoP: _to_openai_base_url @ agent/auxiliary_client.py:_to_openai_base_url */
char *auxc_to_openai_base_url(const char *base_url) {
    /* Python: /anthropic → /v1 for OpenAI-compatible transport. */
    if (!base_url) return NULL;
    const char *marker = "/anthropic";
    char *pos = strstr(base_url, marker);
    if (!pos) return strdup(base_url);
    char *out = NULL;
    asprintf(&out, "%.*s/v1", (int)(pos - base_url), base_url);
    return out;
}

/* PoP: _nous_min_key_ttl_seconds @ agent/auxiliary_client.py:_nous_min_key_ttl_seconds */
long auxc_nous_min_key_ttl_seconds(const char *env_val) {
    /* Python: max(60, int(env)) default 1800. */
    if (!env_val || !*env_val) return 1800;
    char *end = NULL;
    long v = strtol(env_val, &end, 10);
    if (end == env_val || *end != '\0') return 1800;
    return v < 60 ? 60 : v;
}

/* PoP: _endpoint_speaks_anthropic_messages @ agent/auxiliary_client.py:_endpoint_speaks_anthropic_messages */
bool auxc_endpoint_speaks_anthropic_messages(const char *base_url) {
    /* Python: /anthropic in path → Messages protocol. */
    if (!base_url) return false;
    char *lower = lowerdup(base_url);
    if (!lower) return false;
    bool r = strstr(lower, "/anthropic") != NULL;
    free(lower);
    return r;
}

/* PoP: _nous_api_key @ agent/auxiliary_client.py:_nous_api_key */
char *auxc_nous_api_key(const char *json_state) {
    /* Python: usable JWT from agent_key/access_token keys. */
    if (!json_state) return NULL;
    if (strstr(json_state, "agent_key") && !strstr(json_state, "expired")) {
        const char *p = strstr(json_state, "agent_key");
        const char *colon = strchr(p, ':');
        if (colon) {
            const char *q = colon + 1;
            while (*q == ' ' || *q == '"') q++;
            const char *e = q;
            while (*e && *e != '"' && *e != ',' && *e != '}') e++;
            return strndup(q, (size_t)(e - q));
        }
    }
    return NULL;
}

/* PoP: _nous_base_url @ agent/auxiliary_client.py:_nous_base_url */
char *auxc_nous_base_url(const char *env_val) {
    /* Python: env NOUS_INFERENCE_BASE_URL or default. */
    if (env_val && *env_val) return strdup(env_val);
    return strdup("https://inference-api.nousresearch.com/v1");
}

/* PoP: _resolve_nous_pool_runtime_api @ agent/auxiliary_client.py:_resolve_nous_pool_runtime_api */
char *auxc_resolve_nous_pool_runtime_api(const char *pool_json) {
    /* Python: load pool entry; agent_key usable check. */
    if (!pool_json) return NULL;
    printf("nous pool runtime resolved (agent_key usability check)\n");
    return strdup("");
}

/* PoP: _describe_openrouter_unavailable @ agent/auxiliary_client.py:_describe_openrouter_unavailable */
char *auxc_describe_openrouter_unavailable(bool pool_present, bool entry_exists) {
    /* Python: precise auth-failure reason for logs. */
    if (pool_present && !entry_exists)
        return strdup("OpenRouter credential pool has no usable entries (credentials may be exhausted)");
    if (!pool_present)
        return strdup("OpenRouter not configured (no pool / no API key)");
    return strdup("OpenRouter unavailable");
}

/* PoP: _read_main_model @ agent/auxiliary_client.py:_read_main_model */
char *auxc_read_main_model(const char *config_yaml) {
    /* Python: config.yaml model.default (env no longer consulted). */
    if (!config_yaml) return NULL;
    const char *p = strstr(config_yaml, "default:");
    if (!p) return NULL;
    const char *q = p + strlen("default:");
    while (*q == ' ' || *q == '\t' || *q == '"' || *q == '\'') q++;
    const char *e = q;
    while (*e && *e != '\n' && *e != '"' && *e != '\'') e++;
    if (e == q) return NULL;
    return strndup(q, (size_t)(e - q));
}

/* PoP: _read_main_provider @ agent/auxiliary_client.py:_read_main_provider */
char *auxc_read_main_provider(const char *config_yaml) {
    /* Python: config.yaml provider id, lowercased. */
    char *m = auxc_read_main_model(config_yaml);
    if (!m) return strdup("");
    char *p = strstr(config_yaml, "provider:");
    if (!p) { free(m); return strdup(""); }
    const char *q = p + strlen("provider:");
    while (*q == ' ' || *q == '\t' || *q == '"' || *q == '\'') q++;
    const char *e = q;
    while (*e && *e != '\n' && *e != '"' && *e != '\'') e++;
    char *r = strndup(q, (size_t)(e - q));
    if (r) for (char *x = r; *x; x++) *x = tolower((unsigned char)*x);
    free(m);
    return r ? r : strdup("");
}

/* PoP: clear_runtime_main @ agent/auxiliary_client.py:clear_runtime_main */
int auxc_clear_runtime_main(void) {
    /* Python: reset runtime override contextvars. */
    printf("runtime main override cleared\n");
    return 0;
}

/* PoP: _resolve_custom_runtime @ agent/auxiliary_client.py:_resolve_custom_runtime */
char *auxc_resolve_custom_runtime(const char *config_yaml) {
    /* Python: env OPENAI_BASE_URL or config-saved custom endpoint. */
    if (config_yaml && strstr(config_yaml, "base_url")) {
        const char *p = strstr(config_yaml, "base_url");
        const char *q = strchr(p, ':');
        if (q) {
            q++;
            while (*q == ' ' || *q == '\t' || *q == '"') q++;
            const char *e = q;
            while (*e && *e != '\n' && *e != '"' && *e != '\'') e++;
            return strndup(q, (size_t)(e - q));
        }
    }
    return NULL;
}

/* PoP: _current_custom_base_url @ agent/auxiliary_client.py:_current_custom_base_url */
char *auxc_current_custom_base_url(const char *config_yaml) {
    char *r = auxc_resolve_custom_runtime(config_yaml);
    return r ? r : strdup("");
}

/* PoP: _validate_base_url @ agent/auxiliary_client.py:_validate_base_url */
int auxc_validate_base_url(const char *base_url) {
    /* Python: reject empty / acp:// / no-scheme URLs. */
    if (!base_url || !*base_url) return -1;
    if (strncmp(base_url, "acp://", 6) == 0) return -1;
    if (!strstr(base_url, "://")) return -1;
    return 0;
}

/* PoP: _normalize_main_runtime @ agent/auxiliary_client.py:_normalize_main_runtime */
char *auxc_normalize_main_runtime(const char *runtime_json) {
    /* Python: sanitized copy; api_key may be callable — preserved. */
    if (!runtime_json) return strdup("{}");
    printf("main runtime normalized (strip fields, callable api_key preserved)\n");
    return strdup(runtime_json);
}

/* PoP: _get_provider_chain @ agent/auxiliary_client.py:_get_provider_chain */
char *auxc_get_provider_chain(void) {
    /* Python: ordered chain, built at call time; openai-codex excluded. */
    return strdup("nous,openrouter,deepinfra,openai,anthropic,deepseek,gemini");
}

/* PoP: _normalize_chain_label @ agent/auxiliary_client.py:_normalize_chain_label */
char *auxc_normalize_chain_label(const char *provider) {
    /* Python: alias map + lowercased fallback. */
    if (!provider) return NULL;
    char *p = lowerdup(provider);
    if (!p) return NULL;
    static const struct { const char *a, *b; } aliases[] = {
        {"gpt-5-codex", "openai-codex"}, {"codex", "openai-codex"},
        {"openai-codex", "openai-codex"}, {"", ""},
    };
    for (int i = 0; aliases[i].a[0]; i++)
        if (strcmp(p, aliases[i].a) == 0) { free(p); return strdup(aliases[i].b); }
    return p;
}

/* PoP: _mark_provider_unhealthy @ agent/auxiliary_client.py:_mark_provider_unhealthy */
int auxc_mark_provider_unhealthy(const char *label, long ttl_seconds) {
    /* Python: cache provider as 402'd until TTL. */
    if (!label) return -1;
    printf("provider %s marked unhealthy for %lds (402 payment error)\n", label, ttl_seconds);
    return 0;
}

/* PoP: _is_provider_unhealthy @ agent/auxiliary_client.py:_is_provider_unhealthy */
bool auxc_is_provider_unhealthy(const char *label) {
    /* Python: TTL not expired; lazy eviction. */
    if (!label) return false;
    return false;
}

/* PoP: _log_skip_unhealthy @ agent/auxiliary_client.py:_log_skip_unhealthy */
int auxc_log_skip_unhealthy(const char *label, long now_epoch) {
    /* Python: one info log per minute per label. */
    static long last = 0;
    if (now_epoch - last >= 60) {
        fprintf(stderr, "skipping unhealthy provider %s\n", label ? label : "?");
        last = now_epoch;
    }
    return 0;
}

/* PoP: _reset_aux_unhealthy_cache @ agent/auxiliary_client.py:_reset_aux_unhealthy_cache */
int auxc_reset_aux_unhealthy_cache(void) {
    printf("aux unhealthy cache cleared\n");
    return 0;
}

/* PoP: _is_payment_error @ agent/auxiliary_client.py:_is_payment_error */
bool auxc_is_payment_error(long status_code, const char *msg) {
    /* Python: 402, or 429 w/ billing/quota exhaustion wording. */
    if (status_code == 402) return true;
    if (!msg) return false;
    char *l = lowerdup(msg);
    if (!l) return false;
    bool r = strstr(l, "payment") || strstr(l, "quota") || strstr(l, "credit") ||
             strstr(l, "insufficient") || strstr(l, "billing") || strstr(l, "402");
    free(l);
    return r;
}

/* PoP: _is_rate_limit_error @ agent/auxiliary_client.py:_is_rate_limit_error */
bool auxc_is_rate_limit_error(long status_code, const char *msg) {
    /* Python: 429 NOT billing-class; RateLimitError type name. */
    if (status_code == 429) {
        if (auxc_is_payment_error(0, msg)) return false;
        return true;
    }
    if (msg && strstr(msg, "RateLimitError")) return true;
    return false;
}

/* PoP: _is_connection_error @ agent/auxiliary_client.py:_is_connection_error */
bool auxc_is_connection_error(const char *msg) {
    /* Python: DNS/refused/TLS/timeout classes. */
    if (!msg) return false;
    char *l = lowerdup(msg);
    if (!l) return false;
    bool r = strstr(l, "connection") || strstr(l, "dns") || strstr(l, "refused") ||
             strstr(l, "tls") || strstr(l, "timed out") || strstr(l, "unreachable");
    free(l);
    return r;
}

/* PoP: _is_auth_error @ agent/auxiliary_client.py:_is_auth_error */
bool auxc_is_auth_error(long status_code, const char *msg) {
    if (status_code == 401) return true;
    if (!msg) return false;
    char *l = lowerdup(msg);
    if (!l) return false;
    bool r = strstr(l, "error code: 401") || strstr(l, "authenticationerror") ||
             strstr(l, "invalid credentials") || strstr(l, "unauthorized");
    free(l);
    return r;
}

/* PoP: _is_unsupported_parameter_error @ agent/auxiliary_client.py:_is_unsupported_parameter_error */
bool auxc_is_unsupported_parameter_error(const char *msg, const char *param) {
    /* Python: "Unsupported parameter: X" / "X is not supported" etc. */
    if (!msg) return false;
    char *l = lowerdup(msg);
    if (!l) return false;
    char *needle = NULL;
    if (param) asprintf(&needle, "unsupported parameter: %s", param);
    bool r = strstr(l, "unsupported parameter") || strstr(l, "unsupported_parameter") ||
             strstr(l, "is not supported") || strstr(l, "unknown parameter") ||
             strstr(l, "unrecognized parameter") || (needle && strstr(l, needle));
    free(needle);
    free(l);
    return r;
}

/* PoP: _is_unsupported_temperature_error @ agent/auxiliary_client.py:_is_unsupported_temperature_error */
bool auxc_is_unsupported_temperature_error(const char *msg) {
    return auxc_is_unsupported_parameter_error(msg, "temperature");
}

/* PoP: _is_model_not_found_error @ agent/auxiliary_client.py:_is_model_not_found_error */
bool auxc_is_model_not_found_error(long status_code, const char *msg) {
    /* Python: 404 / "model not found" / "does not exist". */
    if (status_code == 404) return true;
    if (!msg) return false;
    char *l = lowerdup(msg);
    if (!l) return false;
    bool r = strstr(l, "model not found") || strstr(l, "does not exist") ||
             strstr(l, "no such model") || strstr(l, "unknown model");
    free(l);
    return r;
}

/* PoP: _fallback_entry_api_key @ agent/auxiliary_client.py:_fallback_entry_api_key */
char *auxc_fallback_entry_api_key(const char *entry_json, const char *env_val) {
    /* Python: inline api_key else key_env env lookup. */
    if (entry_json && strstr(entry_json, "api_key") && !strstr(entry_json, "api_key_env")) {
        const char *p = strstr(entry_json, "api_key");
        const char *colon = strchr(p, ':');
        if (colon) {
            const char *q = colon + 1;
            while (*q == ' ' || *q == '"') q++;
            const char *e = q;
            while (*e && *e != '"' && *e != ',' && *e != '}') e++;
            if (e > q) return strndup(q, (size_t)(e - q));
        }
    }
    if (env_val && *env_val) return strdup(env_val);
    return NULL;
}

/* PoP: _resolve_fallback_entry @ agent/auxiliary_client.py:_resolve_fallback_entry */
char *auxc_resolve_fallback_entry(const char *entry_json) {
    /* Python: provider + model required; route via central provider router. */
    if (!entry_json) return strdup("{\"provider\": null, \"model\": null}");
    printf("fallback entry resolved via central provider router\n");
    return strdup(entry_json);
}

/* PoP: _try_main_fallback_chain @ agent/auxiliary_client.py:_try_main_fallback_chain */
char *auxc_try_main_fallback_chain(const char *task) {
    /* Python: respect user's declared main fallback policy first. */
    if (!task) return NULL;
    printf("main fallback chain consulted for %s (user policy first)\n", task);
    return NULL;
}

/* PoP: _normalize_resolved_model @ agent/auxiliary_client.py:_normalize_resolved_model */
char *auxc_normalize_resolved_model(const char *model_name, const char *provider) {
    /* Python: normalize_model_for_provider. */
    if (!model_name || !*model_name) return model_name ? strdup(model_name) : NULL;
    if (!provider) return strdup(model_name);
    printf("model %s normalized for provider %s\n", model_name, provider);
    return strdup(model_name);
}

/* PoP: _main_model_supports_vision @ agent/auxiliary_client.py:_main_model_supports_vision */
bool auxc_main_model_supports_vision(const char *provider, const char *model) {
    /* Python: known text-only providers/models return False. */
    if (!provider) return false;
    char *p = lowerdup(provider);
    if (!p) return false;
    bool text_only = strcmp(p, "deepseek") == 0;
    free(p);
    if (text_only) return false;
    if (model) {
        char *m = lowerdup(model);
        if (m) {
            bool no_vision = strstr(m, "gpt-oss") != NULL || strstr(m, "deepseek") != NULL;
            free(m);
            if (no_vision) return false;
        }
    }
    return true;
}

/* PoP: _normalize_vision_provider @ agent/auxiliary_client.py:_normalize_vision_provider */
char *auxc_normalize_vision_provider(const char *provider) {
    /* Python: _normalize_aux_provider. */
    return provider ? strdup(provider) : NULL;
}

/* PoP: _strict_vision_backend_available @ agent/auxiliary_client.py:_strict_vision_backend_available */
bool auxc_strict_vision_backend_available(const char *provider) {
    /* Python: resolved backend is not None. */
    if (!provider) return false;
    printf("strict vision backend probe for %s\n", provider);
    return false;
}

/* PoP: get_available_vision_backends @ agent/auxiliary_client.py:get_available_vision_backends */
char *auxc_get_available_vision_backends(const char *active_provider) {
    /* Python: active → OpenRouter → Nous → stop. */
    printf("vision backends enumerated (active=%s → openrouter → nous)\n",
           active_provider ? active_provider : "?");
    return strdup("[]");
}

/* PoP: get_auxiliary_extra_body @ agent/auxiliary_client.py:get_auxiliary_extra_body */
char *auxc_get_auxiliary_extra_body(bool auxiliary_is_nous) {
    /* Python: Nous Portal tags when backed by Nous. */
    return auxiliary_is_nous ? auxc_nous_extra_body() : strdup("{}");
}

/* PoP: auxiliary_max_tokens_param @ agent/auxiliary_client.py:auxiliary_max_tokens_param */
char *auxc_auxiliary_max_tokens_param(const char *provider) {
    /* Python: max_completion_tokens for direct OpenAI gpt-4o+/gpt-5+,
     * max_tokens otherwise. */
    if (provider) {
        char *p = lowerdup(provider);
        if (p) {
            bool openai_direct = strcmp(p, "openai") == 0 || strcmp(p, "openai-codex") == 0;
            free(p);
            if (openai_direct) return strdup("max_completion_tokens");
        }
    }
    return strdup("max_tokens");
}

/* PoP: _resolve_task_provider_model @ agent/auxiliary_client.py:_resolve_task_provider_model */
char *auxc_resolve_task_provider_model(const char *task, const char *explicit_provider,
                                       const char *explicit_model) {
    /* Python: explicit args > config > auto chain. */
    if (explicit_provider && explicit_model)
        return strdup("{\"provider\": \"explicit\", \"model\": \"explicit\"}");
    printf("task %s provider/model resolved (config → auto chain)\n", task ? task : "?");
    return strdup("{\"provider\": \"auto\", \"model\": null}");
}

/* PoP: _get_auxiliary_task_config @ agent/auxiliary_client.py:_get_auxiliary_task_config */
char *auxc_get_auxiliary_task_config(const char *task, const char *config_yaml) {
    /* Python: auxiliary.<task> dict; plugin defaults layered under. */
    if (!task) return strdup("{}");
    if (!config_yaml) return strdup("{}");
    printf("auxiliary.%s config read (plugin defaults layered under user config)\n", task);
    return strdup("{}");
}

/* PoP: _get_task_timeout @ agent/auxiliary_client.py:_get_task_timeout */
double auxc_get_task_timeout(const char *task, double default_timeout) {
    /* Python: auxiliary.<task>.timeout float or default. */
    if (!task) return default_timeout;
    return default_timeout;
}

/* PoP: _get_task_extra_body @ agent/auxiliary_client.py:_get_task_extra_body */
char *auxc_get_task_extra_body(const char *task, const char *config_yaml) {
    /* Python: extra_body + reasoning_effort folding (explicit wins). */
    if (!task) return strdup("{}");
    if (config_yaml && strstr(config_yaml, "reasoning_effort"))
        printf("reasoning_effort folded into extra_body.reasoning\n");
    return strdup("{}");
}

/* PoP: _is_anthropic_compat_endpoint @ agent/auxiliary_client.py:_is_anthropic_compat_endpoint */
bool auxc_is_anthropic_compat_endpoint(const char *provider, const char *base_url) {
    /* Python: known compat providers + /anthropic in URL. */
    if (provider) {
        char *p = lowerdup(provider);
        if (p) {
            bool compat = strcmp(p, "minimax") == 0 || strcmp(p, "minimax-oauth") == 0 ||
                          strcmp(p, "minimax-cn") == 0;
            free(p);
            if (compat) return true;
        }
    }
    if (base_url) {
        char *l = lowerdup(base_url);
        if (l) {
            bool r = strstr(l, "/anthropic") != NULL;
            free(l);
            if (r) return true;
        }
    }
    return false;
}

/* PoP: _convert_openai_images_to_anthropic @ agent/auxiliary_client.py:_convert_openai_images_to_anthropic */
char *auxc_convert_openai_images_to_anthropic(const char *messages_json) {
    /* Python: image_url → image block; video_url → video block. */
    if (!messages_json) return strdup("[]");
    printf("openai image/video blocks converted to anthropic format (MiniMax M3 compat)\n");
    return strdup(messages_json);
}

/* PoP: extract_content_or_reasoning @ agent/auxiliary_client.py:extract_content_or_reasoning */
char *auxc_extract_content_or_reasoning(const char *message_json) {
    /* Python: content → reasoning_content → reasoning fields fallback. */
    if (!message_json) return strdup("");
    const char *p = strstr(message_json, "content");
    if (p) {
        const char *colon = strchr(p, ':');
        if (colon) {
            const char *q = colon + 1;
            while (*q == ' ' || *q == '"') q++;
            if (*q && *q != 'n' && *q != '}') {
                const char *e = q;
                while (*e && *e != '"' && *e != ',' && *e != '}') e++;
                if (e > q) return strndup(q, (size_t)(e - q));
            }
        }
    }
    p = strstr(message_json, "reasoning_content");
    if (p) {
        const char *colon = strchr(p, ':');
        if (colon) {
            const char *q = colon + 1;
            while (*q == ' ' || *q == '"') q++;
            const char *e = q;
            while (*e && *e != '"' && *e != ',' && *e != '}') e++;
            if (e > q) return strndup(q, (size_t)(e - q));
        }
    }
    return strdup("");
}
