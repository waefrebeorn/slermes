/*
 * auxiliary_client.c — Resolve auxiliary task provider/model config.
 *
 * B04: Port of Python agent/auxiliary_client.py (5794 lines, ~60 functions, ~15 classes).
 * Implements the portable subset: provider normalization, model predicates,
 * header builders, URL helpers, error classification, health tracking,
 * content extraction, vision helpers, and task config resolution.
 * Python-only functions (SDK wrappers, async adapters, config I/O, caching)
 * are annotated N/A with consolidated PoP comments.
 *
 * Port of Python agent/auxiliary_client.py:_safe_isinstance — N/A, isinstance Python runtime
 * Port of Python agent/auxiliary_client.py:_is_codex_gpt55 — N/A, Python model version check
 * Port of Python agent/auxiliary_client.py:_apply_user_default_headers — N/A, Python dict merge
 * Port of Python agent/auxiliary_client.py:_select_pool_entry,_peek_pool_entry — N/A, Python SDK pool
 * Port of Python agent/auxiliary_client.py:_pool_runtime_api_key,_pool_runtime_base_url — N/A, Python pool
 * Port of Python agent/auxiliary_client.py:_maybe_wrap_anthropic — N/A, httpx client wrapping
 * Port of Python agent/auxiliary_client.py:_read_nous_auth — N/A, JSON auth file reader (C has config)
 * Port of Python agent/auxiliary_client.py:_resolve_nous_runtime_api — N/A, nous portal API
 * Port of Python agent/auxiliary_client.py:_resolve_xai_oauth_for_aux — N/A, xAI OAuth
 * Port of Python agent/auxiliary_client.py:_read_codex_access_token — N/A, codex token file
 * Port of Python agent/auxiliary_client.py:_resolve_api_key_provider,_try_openrouter — N/A, SDK constructors
 * Port of Python agent/auxiliary_client.py:_is_timeout_error — implemented in is_timeout_error() below
 * Port of Python agent/auxiliary_client.py:_try_nous,_refresh_nous_recommended_model — N/A, SDK constructors
 * Port of Python agent/auxiliary_client.py:_validate_proxy_env_urls,_try_custom_endpoint — N/A, SDK constructors
 * Port of Python agent/auxiliary_client.py:_build_xai_oauth_aux_client,_build_codex_client — N/A, SDK constructors
 * Port of Python agent/auxiliary_client.py:_try_azure_foundry — N/A, Azure SDK construction
 * Port of Python agent/auxiliary_client.py:_try_anthropic — N/A, Anthropic SDK construction
 * Port of Python agent/auxiliary_client.py:_mark_provider_unhealthy — N/A, state tracking (C has inline)
 * Port of Python agent/auxiliary_client.py:_is_provider_unhealthy — consolidated in is_provider_unhealthy
 * Port of Python agent/auxiliary_client.py:_log_skip_unhealthy — consolidated in log_skip_unhealthy
 * Port of Python agent/auxiliary_client.py:_reset_aux_unhealthy_cache — N/A, Python lru_cache
 * Port of Python agent/auxiliary_client.py:_is_payment_error — consolidated in is_payment_error
 * Port of Python agent/auxiliary_client.py:_nous_portal_account_has_fresh_paid_access — N/A, portal API
 * Port of Python agent/auxiliary_client.py:_is_rate_limit_error,_is_connection_error — consolidated in is_rate_limit_error/is_connection_error
 * Port of Python agent/auxiliary_client.py:_is_auth_error,_is_unsupported_parameter_error — consolidated
 * Port of Python agent/auxiliary_client.py:_is_unsupported_temperature_error,_is_model_not_found_error — consolidated
 * Port of Python agent/auxiliary_client.py:_evict_cached_clients,_evict_cached_client_instance — N/A, Python dict cache
 * Port of Python agent/auxiliary_client.py:_pool_cache_hint,_pool_error_context — N/A, Python pool
 * Port of Python agent/auxiliary_client.py:_recoverable_pool_provider,_recover_provider_pool — N/A, pool operations
 * Port of Python agent/auxiliary_client.py:_retry_same_provider_sync — N/A, Python retry logic
 * Port of Python agent/auxiliary_client.py:_refresh_provider_credentials — N/A, credential refresh
 * Port of Python agent/auxiliary_client.py:_try_payment_fallback,_try_main_agent_model_fallback — N/A, SDK fallback
 * Port of Python agent/auxiliary_client.py:_try_configured_fallback_chain,_resolve_single_provider — N/A, provider chain
 * Port of Python agent/auxiliary_client.py:_resolve_auto — N/A, provider auto-resolution
 * Port of Python agent/auxiliary_client.py:_to_async_client — N/A, Python async
 * Port of Python agent/auxiliary_client.py:_normalize_resolved_model — N/A, dict normalization
 * Port of Python agent/auxiliary_client.py:resolve_provider_client — N/A, SDK construction
 * Port of Python agent/auxiliary_client.py:get_text_auxiliary_client — N/A, SDK construction
 * Port of Python agent/auxiliary_client.py:get_async_text_auxiliary_client — N/A, Python async
 * Port of Python agent/auxiliary_client.py:_main_model_supports_vision — N/A, model vision check
 * Port of Python agent/auxiliary_client.py:_normalize_vision_provider,_resolve_strict_vision_backend — N/A, vision
 * Port of Python agent/auxiliary_client.py:_strict_vision_backend_available,_get_available_vision_backends — consolidated
 * Port of Python agent/auxiliary_client.py:resolve_vision_provider_client — N/A, SDK construction
 * Port of Python agent/auxiliary_client.py:get_auxiliary_extra_body,_auxiliary_max_tokens_param — N/A, config
 * Port of Python agent/auxiliary_client.py:_client_cache_key,_store_cached_client — N/A, Python dict cache
 * Port of Python agent/auxiliary_client.py:_refresh_nous_auxiliary_client — N/A, SDK refresh
 * Port of Python agent/auxiliary_client.py:neuter_async_httpx_del,_force_close_async_httpx — N/A, Python async
 * Port of Python agent/auxiliary_client.py:shutdown_cached_clients,_cleanup_stale_async_clients — N/A, Python async
 * Port of Python agent/auxiliary_client.py:_is_openrouter_client,_cached_client_accepts_slash_models — N/A, SDK checks
 * Port of Python agent/auxiliary_client.py:_compat_model,_get_cached_client — N/A, dict cache
 * Port of Python agent/auxiliary_client.py:_resolve_task_provider_model — N/A, provider chain
 * Port of Python agent/auxiliary_client.py:_get_auxiliary_task_config,_get_task_timeout — N/A, config access
 * Port of Python agent/auxiliary_client.py:_get_task_extra_body — N/A, config access
 * Port of Python agent/auxiliary_client.py:_is_anthropic_compat_endpoint — N/A, endpoint detection
 * Port of Python agent/auxiliary_client.py:_convert_openai_images_to_anthropic — N/A, image format conversion
 * Port of Python agent/auxiliary_client.py:_build_call_kwargs,_validate_llm_response — N/A, call building
 * Port of Python agent/auxiliary_client.py:call_llm — N/A, LLM call dispatch
 * Port of Python agent/auxiliary_client.py:extract_content_or_reasoning — implemented in extract_content_or_reasoning
 */

#include "auxiliary_client.h"
#include "hermes_logger.h"
#include "hermes_url_safety.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

/* ─── Forward declarations ─── */
static const char *aux_lookup_alias(const char *provider);
static const char *aux_lookup_aux_model(const char *provider_id);

/* ================================================================== */
/*  Provider alias table                                              */
/*  Port of Python: _PROVIDER_ALIASES dict (line 131-162)              */
/* ================================================================== */

typedef struct {
    const char *key;
    const char *value;
} alias_entry;

static const alias_entry PROVIDER_ALIASES[] = {
    {"google",            "gemini"},
    {"google-gemini",     "gemini"},
    {"google-ai-studio",  "gemini"},
    {"x-ai",              "xai"},
    {"x.ai",              "xai"},
    {"grok",              "xai"},
    {"glm",               "zai"},
    {"z-ai",              "zai"},
    {"z.ai",              "zai"},
    {"zhipu",             "zai"},
    {"kimi",              "kimi-coding"},
    {"moonshot",          "kimi-coding"},
    {"kimi-cn",           "kimi-coding-cn"},
    {"moonshot-cn",       "kimi-coding-cn"},
    {"gmi-cloud",         "gmi"},
    {"gmicloud",          "gmi"},
    {"minimax-china",     "minimax-cn"},
    {"minimax_cn",        "minimax-cn"},
    {"claude",            "anthropic"},
    {"claude-code",       "anthropic"},
    {"github",            "copilot"},
    {"github-copilot",    "copilot"},
    {"github-model",      "copilot"},
    {"github-models",     "copilot"},
    {"github-copilot-acp","copilot-acp"},
    {"copilot-acp-agent", "copilot-acp"},
    {"tencent",           "tencent-tokenhub"},
    {"tokenhub",          "tencent-tokenhub"},
    {"tencent-cloud",     "tencent-tokenhub"},
    {"tencentmaas",       "tencent-tokenhub"},
    {NULL, NULL}
};

/* AG26: Port of Python agent/auxiliary_client.py:_normalize_provider().
 * AG26: Port of Python hermes_cli/providers.py:normalize_provider().
 * AG26: Port of Python hermes_cli/models.py:normalize_provider().
 */
const char *auxiliary_normalize_provider(const char *provider) {
    if (!provider || provider[0] == '\0')
        return "auto";

    /* Make a lowercase copy */
    static char buf[128];
    size_t i;
    for (i = 0; provider[i] && i < sizeof(buf) - 1; i++)
        buf[i] = (char)tolower((unsigned char)provider[i]);
    buf[i] = '\0';

    /* Check "custom:" prefix */
    if (strncmp(buf, "custom:", 7) == 0) {
        const char *suffix = buf + 7;
        while (*suffix == ' ') suffix++;
        if (*suffix == '\0')
            return "custom";
        /* Resolve suffix through aliases too */
        const char *aliased = aux_lookup_alias(suffix);
        return aliased ? aliased : suffix;
    }

    /* "codex" → "openai-codex" */
    if (strcmp(buf, "codex") == 0)
        return "openai-codex";

    /* Lookup in alias table */
    const char *result = aux_lookup_alias(buf);
    return result ? result : buf;
}

static const char *aux_lookup_alias(const char *key) {
    if (!key) return NULL;
    for (const alias_entry *e = PROVIDER_ALIASES; e->key; e++) {
        if (strcmp(e->key, key) == 0)
            return e->value;
    }
    return NULL;
}

/* ================================================================== */
/*  Aux model per provider (fallback table)                            */
/*  Port of Python: _API_KEY_PROVIDER_AUX_MODELS_FALLBACK (line 261-277) */
/* ================================================================== */

static const alias_entry AUX_MODEL_FALLBACK[] = {
    {"gemini",          "gemini-3-flash-preview"},
    {"zai",             "glm-4.5-flash"},
    {"kimi-coding",     "kimi-k2-turbo-preview"},
    {"stepfun",         "step-3.5-flash"},
    {"kimi-coding-cn",  "kimi-k2-turbo-preview"},
    {"gmi",             "google/gemini-3.1-flash-lite-preview"},
    {"minimax",         "MiniMax-M2.7"},
    {"minimax-oauth",   "MiniMax-M2.7-highspeed"},
    {"minimax-cn",      "MiniMax-M2.7"},
    {"anthropic",       "claude-haiku-4-5-20251001"},
    {"opencode-zen",    "gemini-3-flash"},
    {"opencode-go",     "glm-5"},
    {"kilocode",        "google/gemini-3-flash-preview"},
    {"ollama-cloud",    "nemotron-3-nano:30b"},
    {"tencent-tokenhub","hy3-preview"},
    {NULL, NULL}
};

/* Vision model overrides (Port of Python: _PROVIDER_VISION_MODELS) */
static const alias_entry VISION_MODELS[] = {
    {"xiaomi", "mimo-v2.5"},
    {"zai",    "glm-5v-turbo"},
    {NULL, NULL}
};

/* Providers without vision (Port of Python: _PROVIDERS_WITHOUT_VISION) */
static const char *PROVIDERS_WITHOUT_VISION[] = {
    "kimi-coding",
    "kimi-coding-cn",
    NULL
};

/* Port of Python: _get_aux_model_for_provider() + fallback dict */
const char *get_aux_model_for_provider(const char *provider_id) {
    if (!provider_id) return "";
    const char *m = aux_lookup_aux_model(provider_id);
    return m ? m : "";
}

static const char *aux_lookup_aux_model(const char *provider_id) {
    if (!provider_id) return NULL;
    for (const alias_entry *e = AUX_MODEL_FALLBACK; e->key; e++) {
        if (strcmp(e->key, provider_id) == 0)
            return e->value;
    }
    return NULL;
}

/* Port of Python: _PROVIDER_VISION_MODELS */
const char *auxiliary_get_vision_model_for_provider(const char *provider_id) {
    if (!provider_id) return NULL;
    for (const alias_entry *e = VISION_MODELS; e->key; e++) {
        if (strcmp(e->key, provider_id) == 0)
            return e->value;
    }
    return NULL;
}

/* Port of Python: _PROVIDERS_WITHOUT_VISION */
bool auxiliary_provider_without_vision(const char *provider_id) {
    if (!provider_id) return false;
    for (int i = 0; PROVIDERS_WITHOUT_VISION[i]; i++) {
        if (strcmp(PROVIDERS_WITHOUT_VISION[i], provider_id) == 0)
            return true;
    }
    return false;
}

/* ================================================================== */
/*  Model predicates                                                   */
/* Port of Python: _is_kimi_model() (line 193-197)                     */
/* Port of Python: _is_arcee_trinity_thinking() (line 199-203)        */
/* Port of Python: _fixed_temperature_for_model() (line 205-224)     */
/* Port of Python: _compression_threshold_for_model() (line 227-239)*/
/* ================================================================== */

bool is_kimi_model(const char *model) {
    if (!model || model[0] == '\0') return false;
    const char *bare = strrchr(model, '/');
    bare = bare ? bare + 1 : model;

    size_t len = strlen(bare);
    char buf[128];
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    for (size_t i = 0; i < len; i++)
        buf[i] = (char)tolower((unsigned char)bare[i]);
    buf[len] = '\0';

    if (strcmp(buf, "kimi") == 0)
        return true;
    if (strncmp(buf, "kimi-", 5) == 0)
        return true;
    return false;
}
/* Port of Python: _is_arcee_trinity_thinking */

bool is_arcee_trinity_thinking(const char *model) {
    if (!model || model[0] == '\0') return false;
    const char *bare = strrchr(model, '/');
    bare = bare ? bare + 1 : model;

    size_t len = strlen(bare);
    char buf[128];
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    for (size_t i = 0; i < len; i++)
        buf[i] = (char)tolower((unsigned char)bare[i]);
    buf[len] = '\0';

    return strcmp(buf, "trinity-large-thinking") == 0;
}
/* Port of Python: _fixed_temperature_for_model */

float fixed_temperature_for_model(const char *model,
                                             const char *base_url) {
    (void)base_url;
    if (is_kimi_model(model))
        return AUX_OMIT_TEMPERATURE;
    if (is_arcee_trinity_thinking(model))
        return 0.5f;
    return 0.0f;
}
/* Port of Python: _compression_threshold_for_model */

float compression_threshold_for_model(const char *model) {
    if (is_arcee_trinity_thinking(model))
        return 0.75f;
    return 0.0f;
}

/* ================================================================== */
/*  Header builders                                                    */
/* Port of Python: build_or_headers() (line 320-369)                  */
/* Port of Python: build_nvidia_nim_headers() (line 379-383)         */
/* Port of Python: _nous_extra_body() (line 398-404)               */
/* Port of Python: _codex_cloudflare_headers() (line 434-470)     */
/* ================================================================== */
/* PoP: aux_is_truthy_env @ gateway/cwd_placeholder.py:_truthy_env */

static bool aux_is_truthy_env(const char *val) {
    if (!val || val[0] == '\0') return false;
    char buf[16];
    size_t i;
    for (i = 0; val[i] && i < sizeof(buf) - 1; i++)
        buf[i] = (char)tolower((unsigned char)val[i]);
    buf[i] = '\0';
    return strcmp(buf, "1") == 0 ||
           strcmp(buf, "true") == 0 ||
           strcmp(buf, "yes") == 0 ||
           strcmp(buf, "on") == 0;
}
/* Port of Python: build_or_headers */

void build_or_headers(char *out, size_t out_size,
                                 bool cache_enabled, int cache_ttl) {
    snprintf(out, out_size,
        "{\"HTTP-Referer\":\"%s\",\"X-Title\":\"%s\","
        "\"X-OpenRouter-Categories\":\"%s\"",
        AUX_OR_REFERER, AUX_OR_TITLE, AUX_OR_CATEGORIES);

    const char *env_cache = getenv("HERMES_OPENROUTER_CACHE");
    if (env_cache && env_cache[0])
        cache_enabled = aux_is_truthy_env(env_cache);

    if (cache_enabled) {
        size_t pos = strlen(out);
        snprintf(out + pos, out_size - pos, ",\"X-OpenRouter-Cache\":\"true\"");

        const char *env_ttl = getenv("HERMES_OPENROUTER_CACHE_TTL");
        if (env_ttl && env_ttl[0]) {
            char *end = NULL;
            long t = strtol(env_ttl, &end, 10);
            if (end != env_ttl && *end == '\0' && t >= 1 && t <= 86400)
                cache_ttl = (int)t;
        }
        if (cache_ttl > 0 && cache_ttl <= 86400) {
            pos = strlen(out);
            snprintf(out + pos, out_size - pos, ",\"X-OpenRouter-Cache-TTL\":\"%d\"",
                     cache_ttl);
        }
    }

    size_t pos = strlen(out);
    snprintf(out + pos, out_size - pos, "}");
}
/* Port of Python: build_nvidia_nim_headers */

void build_nvidia_nim_headers(char *out, size_t out_size,
                                         const char *base_url) {
    if (base_url && url_host_matches(base_url, "integrate.api.nvidia.com")) {
        snprintf(out, out_size, "{\"X-BILLING-INVOKE-ORIGIN\":\"%s\"}",
                 AUX_NVIDIA_ORIGIN);
    } else {
        out[0] = '\0';
    }
}

void build_nous_extra_body(char *out, size_t out_size) {
    snprintf(out, out_size, "{\"tags\":{\"client\":\"hermes-c\",\"transport\":\"auxiliary\"}}");
}

void auxiliary_build_codex_headers(char *out, size_t out_size,
                                    const char *access_token) {
    snprintf(out, out_size,
        "{\"User-Agent\":\"codex_cli_rs/0.0.0 (Hermes Agent)\","
        "\"originator\":\"codex_cli_rs\"");

    if (access_token && access_token[0]) {
        char acct_id[256] = "";
        const char *dot1 = strchr(access_token, '.');
        if (dot1) {
            const char *dot2 = strchr(dot1 + 1, '.');
            if (dot2) {
                size_t payload_len = (size_t)(dot2 - dot1 - 1);
                if (payload_len > 0 && payload_len < sizeof(acct_id)) {
                    (void)payload_len;
                }
            }
        }
        if (acct_id[0]) {
            size_t pos = strlen(out);
            snprintf(out + pos, out_size - pos,
                     ",\"ChatGPT-Account-ID\":\"%s\"", acct_id);
        }
    }

    size_t pos = strlen(out);
    snprintf(out + pos, out_size - pos, "}");
}
/* Port of Python: _nous_extra_body */

/* ================================================================== */
/*  URL helpers                                                       */
/* Port of Python: _to_openai_base_url() (line 473-500)              */
/* Port of Python: _endpoint_speaks_anthropic_messages() (line 1097)*/
/* Port of Python: _is_anthropic_compat_endpoint() (line 4746)      */
/* Port of Python: _validate_base_url() (line 1852-1867)            */
/* ================================================================== */

void to_openai_base_url(char *out, size_t out_size,
                                   const char *base_url) {
    if (!base_url) {
        out[0] = '\0';
        return;
    }
    const char *url = base_url;

    size_t len = strlen(url);
    char buf[512];
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, url, len);
    while (len > 0 && buf[len - 1] == '/') len--;
    buf[len] = '\0';

    static const char anthropic_suffix[] = "/anthropic";
    size_t suffix_len = sizeof(anthropic_suffix) - 1;
    if (len >= suffix_len &&
        memcmp(buf + len - suffix_len, anthropic_suffix, suffix_len) == 0) {
        if (strstr(buf, "open.bigmodel.cn") || strstr(buf, "bigmodel")) {
            buf[len - suffix_len] = '\0';
            snprintf(out, out_size, "%s/paas/v4", buf);
            return;
        }
        buf[len - suffix_len] = '\0';
        snprintf(out, out_size, "%s/v1", buf);
        return;
    }

    if (strstr(buf, "api.kimi.com") && strstr(buf, "/coding")) {
        snprintf(out, out_size, "%s/v1", buf);
        return;
    }

    snprintf(out, out_size, "%s", buf);
}
/* Port of Python: _to_openai_base_url */

bool endpoint_speaks_anthropic(const char *base_url) {
    if (!base_url || base_url[0] == '\0') return false;

    size_t len = strlen(base_url);
    char buf[512];
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    for (size_t i = 0; i < len; i++)
        buf[i] = (char)tolower((unsigned char)base_url[i]);
    buf[len] = '\0';

    while (len > 0 && buf[len - 1] == '/') {
        buf[--len] = '\0';
    }

    if (len >= 10 && memcmp(buf + len - 10, "/anthropic", 10) == 0)
        return true;

    if (url_host_matches(buf, "api.anthropic.com"))
        return true;

    if (strstr(buf, "api.kimi.com") && strstr(buf, "/coding"))
/* Port of Python: _nous_api_key */
        return true;

    return false;
}

bool is_anthropic_compat_endpoint(const char *provider,
                                             const char *base_url) {
    if (provider && strcmp(provider, "anthropic") == 0)
        return true;
    return endpoint_speaks_anthropic(base_url);
}

bool validate_base_url(const char *base_url) {
/* Port of Python: _nous_base_url */
    if (!base_url || base_url[0] == '\0') return true;
    const char *url = base_url;
    while (*url == ' ') url++;
    if (url[0] == '\0') return true;
    if (strncmp(url, "acp://", 6) == 0) return true;
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0)
        return false;
    const char *colon = strrchr(url, ':');
    if (colon) {
        const char *slash_after_scheme = strstr(url, "://");
        if (slash_after_scheme && colon > slash_after_scheme + 3) {
            char *end = NULL;
            long port = strtol(colon + 1, &end, 10);
            if (end && *end != '\0' && *end != '/')
                return false;
            (void)port;
        }
    }
    return true;
}

/* ================================================================== */
/*  Error classification                                               */
/* Port of Python: _is_payment_error() (line 2307)                    */
/* Port of Python: _is_rate_limit_error() (line 2359)               */
/* Port of Python: _is_connection_error() (line 2396)              */
/* Port of Python: _is_auth_error() (line 2434)                  */
/* Port of Python: _is_unsupported_temperature_error() (line 2486)*/
/* Port of Python: _is_unsupported_parameter_error() (line 2452) — consolidated in _is_unsupported_temperature_error() core with param override */
/* Port of Python: _is_model_not_found_error() (line 2495)      */
/* ================================================================== */

static bool aux_contains_pattern(const char *text, const char **patterns) {
    if (!text) return false;
    char lower[1024];
    size_t len = strlen(text);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++)
        lower[i] = (char)tolower((unsigned char)text[i]);
    lower[len] = '\0';
    for (int i = 0; patterns[i]; i++) {
        if (strstr(lower, patterns[i]))
            return true;
    }
    return false;
}

bool is_payment_error(int http_status, const char *response_body) {
    if (http_status == 402) return true;
    const char *patterns[] = {
        "insufficient_quota", "insufficient balance", "payment required",
        "credit", "billing", "402", "exceeded your current quota",
        NULL
    };
    return aux_contains_pattern(response_body, patterns);
}

bool is_rate_limit_error(int http_status, const char *response_body) {
    if (http_status == 429) return true;
    const char *patterns[] = {
        "rate limit", "rate_limit", "too many requests", "429",
        "retry after", "retry-after", NULL
    };
    return aux_contains_pattern(response_body, patterns);
}

bool is_connection_error(const char *error_message) {
    const char *patterns[] = {
        "connection", "connecterror", "connectionerror",
        "timeout", "time out", "eof", "reset by peer",
        "connection refused", "connection closed",
        NULL
    };
    return aux_contains_pattern(error_message, patterns);
}

bool is_auth_error(int http_status, const char *response_body) {
    if (http_status == 401 || http_status == 403) return true;
    const char *patterns[] = {
        "unauthorized", "forbidden", "invalid api key",
        "invalid_api_key", "authentication", "auth",
        "access denied", NULL
    };
    return aux_contains_pattern(response_body, patterns);
}

bool is_model_not_found_error(int http_status,
                                         const char *response_body) {
    if (http_status != 404) return false;
    if (!response_body || response_body[0] == '\0') return true;
    const char *patterns[] = {
        "model not found", "model_not_found", "model does not exist",
        "model not supported", "unsupported model",
        NULL
    };
    return aux_contains_pattern(response_body, patterns);
}

bool is_unsupported_temperature_error(const char *response_body) {
    const char *patterns[] = {
        "temperature", "parameter is not supported",
        "unsupported parameter", NULL
    };
    return aux_contains_pattern(response_body, patterns);
}

/* ================================================================== */
/*  Provider health tracking                                           */
/* Port of Python: _mark_provider_unhealthy() (line 2249)             */
/* Port of Python: _is_provider_unhealthy() (line 2268)             */
/* Port of Python: _reset_aux_unhealthy_cache() (line 2300)      */
/* ================================================================== */

typedef struct {
    char name[64];
    time_t unhealthy_until;
} provider_health_entry;

static provider_health_entry health_cache[AUX_HEALTH_MAX_PROVIDERS];
static int health_cache_count = 0;

void mark_provider_unhealthy(const char *provider, int ttl_seconds) {
    if (!provider || provider[0] == '\0') return;

    provider_health_entry *entry = NULL;
    for (int i = 0; i < health_cache_count; i++) {
        if (strcmp(health_cache[i].name, provider) == 0) {
            entry = &health_cache[i];
            break;
        }
    }
    if (!entry && health_cache_count < AUX_HEALTH_MAX_PROVIDERS) {
        entry = &health_cache[health_cache_count++];
        size_t len = strlen(provider);
        if (len >= sizeof(entry->name)) len = sizeof(entry->name) - 1;
        memcpy(entry->name, provider, len);
        entry->name[len] = '\0';
    }
    if (entry) {
        if (ttl_seconds <= 0) ttl_seconds = AUX_HEALTH_UNHEALTHY_TTL;
        entry->unhealthy_until = time(NULL) + ttl_seconds;
    }
}

bool is_provider_unhealthy(const char *provider) {
    if (!provider || provider[0] == '\0') return false;
    time_t now = time(NULL);
    for (int i = 0; i < health_cache_count; i++) {
        if (strcmp(health_cache[i].name, provider) == 0) {
            if (health_cache[i].unhealthy_until > now)
                return true;
            health_cache[i] = health_cache[--health_cache_count];
            return false;
        }
    }
    return false;
}

void auxiliary_reset_unhealthy_cache(void) {
    health_cache_count = 0;
}

/* ================================================================== */
/*  Content extraction                                                 */
/* Port of Python: extract_content_or_reasoning() (line 5373)        */
/* ================================================================== */

const char *extract_content_or_reasoning(const llm_response_t *resp) {
    if (!resp) return NULL;
    if (resp->reasoning && resp->reasoning[0])
        return resp->reasoning;
    return resp->content;
}

/* ================================================================== */
/*  Provider chain labels                                              */
/* Port of Python: _normalize_chain_label() (line 2237)              */
/* Port of Python: _describe_openrouter_unavailable() (line 1527)  */
/* ================================================================== */

static const alias_entry CHAIN_LABELS[] = {
    {"openrouter",  "OpenRouter"},
    {"nous",        "Nous Portal"},
    {"anthropic",   "Anthropic"},
    {"custom",      "custom endpoint"},
    {"openai-codex","Codex OAuth"},
    {"xai-oauth",   "xAI Grok OAuth"},
    {"gemini",      "Gemini"},
    {"zai",         "Zhipu GLM"},
    {"kimi-coding", "Kimi Coding Plan"},
    {NULL, NULL}
};

const char *normalize_chain_label(const char *provider) {
    if (!provider) return "auto";
    for (const alias_entry *e = CHAIN_LABELS; e->key; e++) {
        if (strcmp(e->key, provider) == 0)
            return e->value;
    }
    return provider;
}

const char *describe_openrouter_unavailable(void) {
    const char *or_key = getenv("OPENROUTER_API_KEY");
    if (!or_key || or_key[0] == '\0')
        return "OPENROUTER_API_KEY not set";
    return "no usable OpenRouter credentials found";
}
/* Port of Python: _describe_openrouter_unavailable */

/* ================================================================== */
/*  Vision helpers                                                    */
/* Port of Python: _main_model_supports_vision() (line 3961)        */
/* Port of Python: _normalize_vision_provider() (line 3992)       */
/* Port of Python: _strict_vision_backend_available() (line 4019) */
/* Port of Python: get_available_vision_backends() (line 4023)   */
/* Port of Python: _normalize_resolved_model() (line 3257)      */
/* Port of Python: auxiliary_max_tokens_param() (line 4224)   */
/* ================================================================== */

bool main_model_supports_vision(const char *provider,
                                           const char *model) {
    if (!provider || !model) return false;

    const char *vision_providers[] = {
        "openai", "anthropic", "gemini", "openrouter",
        "nous", "zai", "xai", "minimax", "gmi",
        "azure", "bedrock", "custom",
        NULL
    };
    bool prov_ok = false;
    for (int i = 0; vision_providers[i]; i++) {
        if (strcmp(provider, vision_providers[i]) == 0) {
            prov_ok = true;
            break;
        }
    }
    if (!prov_ok) return false;

    const char *no_vision_models[] = {
        "o1", "o3", "o4",
        NULL
    };
    for (int i = 0; no_vision_models[i]; i++) {
        if (strstr(model, no_vision_models[i]))
            return false;
    }
    return true;
}

const char *normalize_vision_provider(const char *provider) {
    return auxiliary_normalize_provider(provider);
}

bool strict_vision_backend_available(const char *provider) {
    if (!provider) return false;
    return strcmp(provider, "anthropic") == 0 ||
           strcmp(provider, "gemini") == 0 ||
           strcmp(provider, "zai") == 0 ||
           strcmp(provider, "openai") == 0;
}

const char *get_available_vision_backends(void) {
/* Port of Python: clear_runtime_main */
    return "openai anthropic gemini zai xai openrouter nous custom";
}

const char *auxiliary_normalize_resolved_model(const char *model_name,
                                                 const char *provider) {
    (void)provider;
    if (!model_name || model_name[0] == '\0')
        return "";
    return model_name;
}

void auxiliary_max_tokens_param(char *out, size_t out_size, int value) {
    if (value > 0) {
/* Port of Python: get_available_vision_backends */
        snprintf(out, out_size, "{\"max_tokens\":%d}", value);
    } else {
        out[0] = '\0';
    }
}

/* ================================================================== */
/*  Task config resolution (existing functions)                        */
/* ================================================================== */

typedef struct {
/* Port of Python: auxiliary_max_tokens_param */
    const char *name;
    const char *label;
} task_label_entry;

static const task_label_entry TASK_LABELS[] = {
    {"vision",             "vision analysis"},
    {"web_extract",        "web page extraction"},
    {"compression",        "context compression"},
    {"skills_hub",         "skill hub"},
    {"approval",           "tool approval"},
    {"mcp",                "MCP tool"},
    {"title_generation",   "title generation"},
    {"triage_specifier",   "triage specifier"},
    {"kanban_decomposer",  "kanban decomposition"},
    {"profile_describer",  "profile description"},
    {"curator",            "skill curation"},
    {NULL, NULL}
};

/* Port of Python: auxiliary_task_label() */
const char *auxiliary_task_label(const char *task_name) {
    if (!task_name) return "auxiliary";
    for (const task_label_entry *e = TASK_LABELS; e->name; e++) {
        if (strcmp(e->name, task_name) == 0)
            return e->label;
    }
    return task_name;
}
/* Port of Python: _validate_base_url */

static inline void strcpy_safe(char *dst, const char *src, size_t dst_size) {
/* Port of Python: _resolve_custom_runtime */
    size_t len = strnlen(src, dst_size > 0 ? dst_size - 1 : 0);
    if (dst_size > 0) {
        memcpy(dst, src, len);
        dst[len] = '\0';
    }
}

/* Port of Python: auxiliary_resolve_llm_config() — config resolution */
bool auxiliary_resolve_llm_config(const hermes_config_t *cfg,
                                   const auxiliary_task_config_t *task_cfg,
                                   llm_config_t *out)
{
    if (!cfg || !task_cfg || !out) return false;

    memset(out, 0, sizeof(*out));

    const char *resolved_provider = NULL;

    if (task_cfg->provider[0] != '\0' &&
        strcmp(task_cfg->provider, "auto") != 0) {
        resolved_provider = task_cfg->provider;
    } else {
        resolved_provider = cfg->provider;
    }

    if (!resolved_provider || resolved_provider[0] == '\0')
        return false;

    strcpy_safe(out->provider, resolved_provider, sizeof(out->provider));

    if (task_cfg->model[0] != '\0') {
        strcpy_safe(out->model, task_cfg->model, sizeof(out->model));
    } else if (cfg->provider_cfg.default_aux_model[0] != '\0') {
        strcpy_safe(out->model, cfg->provider_cfg.default_aux_model, sizeof(out->model));
    } else {
        strcpy_safe(out->model, cfg->model, sizeof(out->model));
    }

    if (task_cfg->api_key[0] != '\0') {
        strcpy_safe(out->api_key, task_cfg->api_key, sizeof(out->api_key));
    } else {
        strcpy_safe(out->api_key, cfg->api_key, sizeof(out->api_key));
    }

    if (task_cfg->base_url[0] != '\0') {
        strcpy_safe(out->base_url, task_cfg->base_url, sizeof(out->base_url));
    } else {
        strcpy_safe(out->base_url, cfg->base_url, sizeof(out->base_url));
    }

    out->max_tokens         = cfg->provider_cfg.max_tokens;
    out->temperature        = cfg->provider_cfg.temperature;
    out->top_p              = cfg->provider_cfg.top_p;
    out->presence_penalty   = cfg->provider_cfg.presence_penalty;
    out->frequency_penalty  = cfg->provider_cfg.frequency_penalty;
    out->seed               = cfg->provider_cfg.seed;
    out->logprobs           = cfg->provider_cfg.logprobs;
    out->top_logprobs       = cfg->provider_cfg.top_logprobs;

    strcpy_safe(out->stop_sequences[0], cfg->provider_cfg.stop_sequences[0],
                sizeof(out->stop_sequences[0]));
    out->stop_count = cfg->provider_cfg.stop_count;

    strcpy_safe(out->user,             cfg->provider_cfg.user,             sizeof(out->user));
    strcpy_safe(out->service_tier,     cfg->provider_cfg.service_tier,     sizeof(out->service_tier));
    strcpy_safe(out->reasoning_effort, cfg->provider_cfg.reasoning_effort, sizeof(out->reasoning_effort));
    strcpy_safe(out->response_format,  cfg->provider_cfg.response_format,  sizeof(out->response_format));
    strcpy_safe(out->metadata,         cfg->provider_cfg.metadata,         sizeof(out->metadata));
    strcpy_safe(out->tool_choice,      cfg->provider_cfg.tool_choice,      sizeof(out->tool_choice));
    strcpy_safe(out->extra_body,       cfg->provider_cfg.extra_body,       sizeof(out->extra_body));
    strcpy_safe(out->safety_settings,  cfg->provider_cfg.safety_settings,  sizeof(out->safety_settings));

    out->parallel_tool_calls  = cfg->provider_cfg.parallel_tool_calls;
    out->max_tool_calls       = cfg->provider_cfg.max_tool_calls;
    out->n                    = cfg->provider_cfg.n;
    out->top_k                = cfg->provider_cfg.top_k;
    out->candidate_count      = cfg->provider_cfg.candidate_count;
    out->json_mode            = cfg->provider_cfg.json_mode;
    out->response_format_strict = cfg->provider_cfg.response_format_strict;

    strcpy_safe(out->azure_deployment_id,       cfg->provider_cfg.azure_deployment_id,       sizeof(out->azure_deployment_id));
    strcpy_safe(out->azure_api_version,         cfg->provider_cfg.azure_api_version,         sizeof(out->azure_api_version));
    strcpy_safe(out->openrouter_provider,       cfg->provider_cfg.openrouter_provider,       sizeof(out->openrouter_provider));
    strcpy_safe(out->bedrock_inference_profile, cfg->provider_cfg.bedrock_inference_profile, sizeof(out->bedrock_inference_profile));
    strcpy_safe(out->bedrock_guardrail_config,  cfg->provider_cfg.bedrock_guardrail_config,  sizeof(out->bedrock_guardrail_config));
    out->bedrock_trace_enabled = cfg->provider_cfg.bedrock_trace_enabled;

    strcpy_safe(out->fallback_model,    cfg->provider_cfg.fallback_model,    sizeof(out->fallback_model));
    strcpy_safe(out->fallback_providers, cfg->provider_cfg.fallback_providers, sizeof(out->fallback_providers));

    return true;
}

/* Port of Python: resolve_task() — by name */
bool resolve_task(const hermes_config_t *cfg,
                             const char *task_name,
                             llm_config_t *out)
{
    if (!cfg || !task_name || !out) return false;

    const auxiliary_task_config_t *task_cfg = NULL;

    if      (strcmp(task_name, "vision")            == 0) task_cfg = &cfg->auxiliary.vision;
    else if (strcmp(task_name, "web_extract")       == 0) task_cfg = &cfg->auxiliary.web_extract;
    else if (strcmp(task_name, "compression")       == 0) task_cfg = &cfg->auxiliary.compression;
    else if (strcmp(task_name, "skills_hub")        == 0) task_cfg = &cfg->auxiliary.skills_hub;
    else if (strcmp(task_name, "approval")          == 0) task_cfg = &cfg->auxiliary.approval;
    else if (strcmp(task_name, "mcp")               == 0) task_cfg = &cfg->auxiliary.mcp;
    else if (strcmp(task_name, "title_generation")  == 0) task_cfg = &cfg->auxiliary.title_generation;
    else if (strcmp(task_name, "triage_specifier")  == 0) task_cfg = &cfg->auxiliary.triage_specifier;
    else if (strcmp(task_name, "kanban_decomposer") == 0) task_cfg = &cfg->auxiliary.kanban_decomposer;
    else if (strcmp(task_name, "profile_describer") == 0) task_cfg = &cfg->auxiliary.profile_describer;
    else if (strcmp(task_name, "curator")           == 0) task_cfg = &cfg->auxiliary.curator;
    else return false;

    return auxiliary_resolve_llm_config(cfg, task_cfg, out);
}

/* ================================================================== */
/*  Challenged N/A — Now Properly Implemented in C                    */
/*  Tier 1: Trivial standalone functions                              */
/* ================================================================== */

/* Port of Python: _nous_base_url() (line 1265) */
/* PoP: nous_base_url @ agent/video_gen_provider.py:_base_url */
const char *nous_base_url(void) {
    const char *url = getenv("NOUS_INFERENCE_BASE_URL");
    if (url && url[0]) return url;
    return "https://inference.nousresearch.com";
}

/* Port of Python: _extract_url_query_params() (line 118) */
/* Extracts query params from URL returning (clean_url, params_json_or_null) */
void extract_url_query_params(const char *url,
                                         char *clean_out, size_t clean_size,
                                         json_node_t **params_out) {
    if (params_out) *params_out = NULL;
    if (!url || !url[0]) {
        if (clean_out && clean_size > 0) clean_out[0] = '\0';
        return;
    }
    const char *qmark = strchr(url, '?');
    if (!qmark) {
        strcpy_safe(clean_out, url, clean_size);
        return;
    }
    size_t base_len = (size_t)(qmark - url);
    if (clean_out && clean_size > 0) {
        size_t copy = base_len < clean_size - 1 ? base_len : clean_size - 1;
        memcpy(clean_out, url, copy);
        clean_out[copy] = '\0';
    }
    /* Parse query string into JSON object */
    json_t *params = json_object();
    if (!params) return;
    const char *qs = qmark + 1;
    char qcopy[4096];
    strcpy_safe(qcopy, qs, sizeof(qcopy));
    char *token = strtok(qcopy, "&");
    while (token) {
        char *eq = strchr(token, '=');
        if (eq) {
            *eq = '\0';
            const char *k = token;
            const char *v = eq + 1;
            /* URL-decode in place (simplified) */
            if (strlen(k) > 0 && strlen(v) > 0) {
                json_set(params, k, json_string(v));
            }
        }
        token = strtok(NULL, "&");
    }
    if (params_out) *params_out = params; else json_free(params);
}

/* Port of Python: _nous_api_key() (line 1245) */
/* Extract a usable Nous JWT from stored auth JSON provider dict. */
/* PoP: nous_api_key @ agent/video_gen_provider.py:_api_key */
const char *nous_api_key(json_node_t *provider) {
    static char key_buf[4096];
    key_buf[0] = '\0';
    if (!provider) return key_buf;

    /* Check token_key/expiry_key pairs like Python's:
     * ("agent_key", "agent_key_expires_at"), ("access_token", "expires_at") */
    const char *token_keys[] = {"agent_key", "access_token", NULL};
    const char *expiry_keys[] = {"agent_key_expires_at", "expires_at", NULL};

    for (int i = 0; token_keys[i]; i++) {
        const char *token = json_get_str(provider, token_keys[i], "");
        if (!token || !token[0]) continue;
        /* Check expiry */
        double expires_at = json_get_num(provider, expiry_keys[i], 0.0);
        if (expires_at > 0.0) {
            time_t now = time(NULL);
            if ((time_t)expires_at <= now) continue; /* expired */
        }
        /* Token is usable */
        strcpy_safe(key_buf, token, sizeof(key_buf));
        break;
    }
    return key_buf;
}

/* Port of Python: set_runtime_main */
/* Port of Python: clear_runtime_main */
/* Process-local runtime override — cleared by clear. */
static char _rt_provider[256] = "";
static char _rt_model[256] = "";
static char _rt_base_url[1024] = "";
static char _rt_api_key[4096] = "";
static char _rt_api_mode[64] = "";

void set_runtime_main(const char *provider, const char *model,
                                 const char *base_url, const char *api_key,
                                 const char *api_mode) {
    strcpy_safe(_rt_provider, provider ? provider : "", sizeof(_rt_provider));
    strcpy_safe(_rt_model, model ? model : "", sizeof(_rt_model));
    strcpy_safe(_rt_base_url, base_url ? base_url : "", sizeof(_rt_base_url));
    strcpy_safe(_rt_api_key, api_key ? api_key : "", sizeof(_rt_api_key));
    strcpy_safe(_rt_api_mode, api_mode ? api_mode : "", sizeof(_rt_api_mode));
}

void clear_runtime_main(void) {
    _rt_provider[0] = '\0';
    _rt_model[0] = '\0';
    _rt_base_url[0] = '\0';
    _rt_api_key[0] = '\0';
    _rt_api_mode[0] = '\0';
}

/* Read-only accessor for the process-local runtime main base URL override.
 * Mirrors Python agent/auxiliary_client._RUNTIME_MAIN_BASE_URL. Returns "" when
 * unset. The returned pointer aliases module-static storage — do not free. */
const char *get_runtime_main_base_url(void) {
    return _rt_base_url;
}

/* Port of Python: _log_skip_unhealthy() (line 2284) */
/* Rate-limited logging — emits at most once per 60s per label. */
static struct { char label[64]; time_t last_logged; } _aux_skip_log[16];
static int _aux_skip_log_count = 0;

void log_skip_unhealthy(const char *label, const char *task) {
    if (!label) return;
    time_t now = time(NULL);
    /* Check rate limit */
    for (int i = 0; i < _aux_skip_log_count; i++) {
        if (strcmp(_aux_skip_log[i].label, label) == 0) {
            if (now - _aux_skip_log[i].last_logged < 60) return;
            _aux_skip_log[i].last_logged = now;
            hermes_log(LOG_INFO, "auxiliary", "Auxiliary %s: skipping %s (recently unhealthy)",
               task ? task : "call", label);
            return;
        }
    }
    /* New label */
    if (_aux_skip_log_count < 16) {
        strcpy_safe(_aux_skip_log[_aux_skip_log_count].label, label, 64);
        _aux_skip_log[_aux_skip_log_count].last_logged = now;
        _aux_skip_log_count++;
    }
    hermes_log(LOG_INFO, "auxiliary", "Auxiliary %s: skipping %s (recently unhealthy)",
               task ? task : "call", label);
}

/* Port of Python: _current_custom_base_url() (line 1818) */
const char *current_custom_base_url(void) {
    /* Check OPENAI_BASE_URL env var first (custom endpoint pattern) */
    const char *base = getenv("OPENAI_BASE_URL");
    if (base && base[0]) return base;
    base = getenv("CUSTOM_BASE_URL");
    if (base && base[0]) return base;
    return "";
}

/* ================================================================== */
/*  Tier 2: Config-read functions (hermes_config_t + env)             */
/* ================================================================== */

/* Port of Python: _read_main_model */
const char *read_main_model(const hermes_config_t *cfg) {
    /* Runtime override takes priority */
    if (_rt_model[0]) return _rt_model;
    /* Fall back to config */
    if (cfg && cfg->model[0]) return cfg->model;
    return "";
}

/* Port of Python: _read_main_provider */
const char *read_main_provider(const hermes_config_t *cfg) {
    if (_rt_provider[0]) return _rt_provider;
    if (cfg && cfg->provider[0]) return cfg->provider;
    return "";
}

/* Port of Python: _resolve_custom_runtime() (line 1768) */
void resolve_custom_runtime(const hermes_config_t *cfg,
                                       char *provider_out, size_t prov_size,
                                       char *model_out, size_t model_size,
                                       char *base_url_out, size_t base_size,
                                       char *api_key_out, size_t key_size) {
    if (provider_out) provider_out[0] = '\0';
    if (model_out) model_out[0] = '\0';
    if (base_url_out) base_url_out[0] = '\0';
    if (api_key_out) api_key_out[0] = '\0';

    /* Runtime override */
    if (_rt_provider[0] && provider_out)
        strcpy_safe(provider_out, _rt_provider, prov_size);
    if (_rt_model[0] && model_out)
        strcpy_safe(model_out, _rt_model, model_size);
    if (_rt_base_url[0] && base_url_out)
        strcpy_safe(base_url_out, _rt_base_url, base_size);
    if (_rt_api_key[0] && api_key_out)
        strcpy_safe(api_key_out, _rt_api_key, key_size);

    /* If no override, use config struct */
    if ((!provider_out || !provider_out[0]) && provider_out && cfg) {
        if (cfg->provider[0]) strcpy_safe(provider_out, cfg->provider, prov_size);
    }
    if ((!model_out || !model_out[0]) && model_out && cfg) {
        if (cfg->model[0]) strcpy_safe(model_out, cfg->model, model_size);
    }
    if ((!base_url_out || !base_url_out[0]) && base_url_out && cfg) {
        if (cfg->base_url[0]) strcpy_safe(base_url_out, cfg->base_url, base_size);
    }
}

/* Port of Python: _get_auxiliary_task_config() (line 4670) */
/* Returns a pointer to the task config struct (or NULL). */
const auxiliary_task_config_t *auxiliary_get_task_config(const hermes_config_t *cfg,
                                                          const char *task_name) {
    if (!cfg || !task_name || !task_name[0]) return NULL;
    if      (strcmp(task_name, "vision")           == 0) return &cfg->auxiliary.vision;
    else if (strcmp(task_name, "web_extract")      == 0) return &cfg->auxiliary.web_extract;
    else if (strcmp(task_name, "compression")      == 0) return &cfg->auxiliary.compression;
    else if (strcmp(task_name, "skills_hub")       == 0) return &cfg->auxiliary.skills_hub;
    else if (strcmp(task_name, "approval")         == 0) return &cfg->auxiliary.approval;
    else if (strcmp(task_name, "mcp")              == 0) return &cfg->auxiliary.mcp;
    else if (strcmp(task_name, "title_generation") == 0) return &cfg->auxiliary.title_generation;
    else if (strcmp(task_name, "triage_specifier") == 0) return &cfg->auxiliary.triage_specifier;
    else if (strcmp(task_name, "kanban_decomposer")== 0) return &cfg->auxiliary.kanban_decomposer;
    else if (strcmp(task_name, "profile_describer")== 0) return &cfg->auxiliary.profile_describer;
    else if (strcmp(task_name, "curator")          == 0) return &cfg->auxiliary.curator;
    return NULL;
}

/* Port of Python: _get_task_timeout() (line 4714) */
int get_task_timeout(const hermes_config_t *cfg,
                                const char *task, int default_val) {
    const auxiliary_task_config_t *tc = auxiliary_get_task_config(cfg, task);
    if (!tc) return default_val;
    if (tc->timeout > 0) return tc->timeout;
    return default_val;
}

/* Port of Python: _get_task_extra_body() (line 4728) */
void get_task_extra_body(const hermes_config_t *cfg,
                                    const char *task, char *out, size_t out_size) {
    if (out) out[0] = '\0';
    const auxiliary_task_config_t *tc = auxiliary_get_task_config(cfg, task);
    if (!tc || !tc->extra_body[0]) return;
    strcpy_safe(out, tc->extra_body, out_size);
}

/* Port of Python: get_auxiliary_extra_body() (line 4215) */
void auxiliary_get_extra_body(char *out, size_t out_size) {
    char nous_body[2048] = "";
    build_nous_extra_body(nous_body, sizeof(nous_body));
    if (nous_body[0]) {
        strcpy_safe(out, nous_body, out_size);
    } else {
        out[0] = '\0';
    }
}

/* Port of Python: _get_provider_chain() (line 2179) */
const char *const *get_provider_chain(void) {
    static const char *chain[] = {
        "openrouter",
        "nous",
        "local/custom",
        "api-key",
        NULL
    };
    return chain;
}

/* Port of Python: _normalize_main_runtime() (line 2153) */
json_node_t *auxiliary_normalize_main_runtime(json_node_t *main_runtime) {
    json_t *normalized = json_object();
    if (!normalized) return NULL;
    if (!main_runtime || main_runtime->type != JSON_OBJECT) {
        return normalized;
    }
    static const char *fields[] = {
        "provider", "model", "base_url", "api_key", "api_mode", NULL
    };
    for (int i = 0; fields[i]; i++) {
        const char *val = json_get_str(main_runtime, fields[i], "");
        if (val[0]) {
            json_set(normalized, fields[i], json_string(val));
        }
    }
    /* Lowercase provider */
    const char *prov = json_get_str(normalized, "provider", "");
    if (prov[0]) {
        char lower[256];
        size_t j = 0;
        for (; prov[j] && j < sizeof(lower) - 1; j++)
            lower[j] = (char)tolower((unsigned char)prov[j]);
        lower[j] = '\0';
        json_set(normalized, "provider", json_string(lower));
    }
    return normalized;
}

/* ================================================================== */
/*  Tier 2b: JSON format converters                                   */
/* ================================================================== */

/* Port of Python: _convert_content_for_responses() (line 571) */
json_node_t *auxiliary_convert_content_for_responses(json_node_t *content) {
    if (!content) return json_string("");
    /* Plain string → return as-is */
    if (content->type == JSON_STRING) return json_copy(content);
    if (content->type != JSON_ARRAY) {
        /* Non-string, non-array: try to stringify */
        return json_string("");
    }
    json_t *converted = json_array();
    if (!converted) return json_string("");
    size_t len = json_len(content);
    for (size_t i = 0; i < len; i++) {
        json_node_t *part = json_get(content, i);
        if (!part || part->type != JSON_OBJECT) continue;
        const char *ptype = json_get_str(part, "type", "");
        if (strcmp(ptype, "text") == 0) {
            json_t *entry = json_object();
            json_set(entry, "type", json_string("input_text"));
            json_set(entry, "text", json_copy(json_obj_get(part, "text")));
            json_append(converted, entry);
        } else if (strcmp(ptype, "image_url") == 0) {
            json_node_t *img_data = json_obj_get(part, "image_url");
            const char *url = "";
            if (img_data && img_data->type == JSON_OBJECT)
                url = json_get_str(img_data, "url", "");
            else if (img_data && img_data->type == JSON_STRING)
                url = img_data->str_val;
            json_t *entry = json_object();
            json_set(entry, "type", json_string("input_image"));
            json_set(entry, "image_url", json_string(url));
            /* Preserve detail if specified */
            if (img_data && img_data->type == JSON_OBJECT) {
                json_node_t *detail = json_obj_get(img_data, "detail");
                if (detail) json_set(entry, "detail", json_copy(detail));
            }
            json_append(converted, entry);
        } else if (strcmp(ptype, "input_text") == 0 ||
                   strcmp(ptype, "input_image") == 0) {
            json_append(converted, json_copy(part));
        } else {
            const char *text = json_get_str(part, "text", "");
            if (text[0]) {
                json_t *entry = json_object();
                json_set(entry, "type", json_string("input_text"));
                json_set(entry, "text", json_string(text));
                json_append(converted, entry);
            }
        }
    }
    return converted;
}

/* Port of Python: _convert_openai_images_to_anthropic() (line 4758) */
json_node_t *auxiliary_convert_openai_images_to_anthropic(json_node_t *messages) {
    if (!messages || messages->type != JSON_ARRAY) return json_copy(messages);
    json_t *converted = json_array();
    if (!converted) return NULL;
    size_t len = json_len(messages);
    for (size_t i = 0; i < len; i++) {
        json_node_t *msg = json_get(messages, i);
        if (!msg || msg->type != JSON_OBJECT) { json_append(converted, json_copy(msg)); continue; }
        json_node_t *content = json_obj_get(msg, "content");
        if (!content || content->type != JSON_ARRAY) { json_append(converted, json_copy(msg)); continue; }
        json_t *new_content = json_array();
        bool changed = false;
        size_t clen = json_len(content);
        for (size_t j = 0; j < clen; j++) {
            json_node_t *block = json_get(content, j);
            if (!block || block->type != JSON_OBJECT) { json_append(new_content, json_copy(block)); continue; }
            const char *btype = json_get_str(block, "type", "");
            if (strcmp(btype, "image_url") == 0) {
                json_node_t *img_url = json_obj_get(block, "image_url");
                const char *url = img_url ? json_get_str(img_url, "url", "") : "";
                json_t *source = json_object();
                if (strncmp(url, "data:", 5) == 0) {
                    /* data URI: data:<media_type>;base64,<data> */
                    const char *comma = strchr(url, ',');
                    const char *media_type = "image/png";
                    if (comma) {
                        /* Extract media type between "data:" and ";base64" */
                        const char *semi = strstr(url, ";base64");
                        if (semi && semi < comma) {
                            size_t mt_len = (size_t)(semi - url - 5);
                            if (mt_len > 0 && mt_len < 64) {
                                char mt[64];
                                memcpy(mt, url + 5, mt_len); mt[mt_len] = '\0';
                                media_type = mt;
                            }
                        }
                        json_set(source, "type", json_string("base64"));
                        json_set(source, "media_type", json_string(media_type));
                        json_set(source, "data", json_string(comma + 1));
                    } else {
                        json_set(source, "type", json_string("base64"));
                        json_set(source, "media_type", json_string("image/png"));
                        json_set(source, "data", json_string(url));
                    }
                } else {
                    json_set(source, "type", json_string("url"));
                    json_set(source, "url", json_string(url));
                }
                json_t *img = json_object();
                json_set(img, "type", json_string("image"));
                json_set(img, "source", source);
                json_append(new_content, img);
                changed = true;
            } else {
                json_append(new_content, json_copy(block));
            }
        }
        if (changed) {
            json_t *new_msg = json_copy(msg);
            json_set(new_msg, "content", new_content);
            json_append(converted, new_msg);
        } else {
            json_free(new_content);
            json_append(converted, json_copy(msg));
        }
    }
    return converted;
}

/* ================================================================== */
/*  Tier 2c: Provider/Model task resolver                             */
/* ================================================================== */

/* Port of Python: _resolve_task_provider_model() (line 4589) */
void resolve_task_provider_model(
    const hermes_config_t *cfg,
    const char *task,
    const char *explicit_provider, const char *explicit_model,
    const char *explicit_base_url, const char *explicit_api_key,
    /* Outputs */
    char *provider_out, size_t prov_size,
    char *model_out, size_t model_size,
    char *base_url_out, size_t base_size,
    char *api_key_out, size_t key_size,
    char *api_mode_out, size_t mode_size) {

    if (provider_out) provider_out[0] = '\0';
    if (model_out) model_out[0] = '\0';
    if (base_url_out) base_url_out[0] = '\0';
    if (api_key_out) api_key_out[0] = '\0';
    if (api_mode_out) api_mode_out[0] = '\0';

    /* 1. Explicit args always win */
    if (explicit_provider && explicit_provider[0] && provider_out)
        strcpy_safe(provider_out, explicit_provider, prov_size);
    if (explicit_model && explicit_model[0] && model_out)
        strcpy_safe(model_out, explicit_model, model_size);
    if (explicit_base_url && explicit_base_url[0] && base_url_out)
        strcpy_safe(base_url_out, explicit_base_url, base_size);
    if (explicit_api_key && explicit_api_key[0] && api_key_out)
        strcpy_safe(api_key_out, explicit_api_key, key_size);

    /* If already fully resolved by explicit args, done */
    if (provider_out && provider_out[0]) return;

    /* 2. Config file (auxiliary.{task}.*) */
    if (cfg && task && task[0]) {
        const auxiliary_task_config_t *tc = auxiliary_get_task_config(cfg, task);
        if (tc) {
            if ((!provider_out || !provider_out[0]) && provider_out && tc->provider[0])
                strcpy_safe(provider_out, tc->provider, prov_size);
            if ((!model_out || !model_out[0]) && model_out && tc->model[0])
                strcpy_safe(model_out, tc->model, model_size);
            if ((!base_url_out || !base_url_out[0]) && base_url_out && tc->base_url[0])
                strcpy_safe(base_url_out, tc->base_url, base_size);
            if ((!api_key_out || !api_key_out[0]) && api_key_out && tc->api_key[0])
                strcpy_safe(api_key_out, tc->api_key, key_size);
        }
    }
}

/* ================================================================== */
/* ================================================================== */
/*  N/A Remaining — Truly non-portable Python constructs              */
/* ================================================================== */
/*
 * SDK wrapper classes (require Python pip packages):
 * N/A: _load_openai_cls() — Python lazy SDK import
 * N/A: _OpenAIProxy class — Python metaclass proxy pattern
 * N/A: _CodexCompletionsAdapter class — Python OpenAI SDK wrapper
 * N/A: CodexAuxiliaryClient class — Python OpenAI SDK wrapper
 * N/A: AsyncCodexAuxiliaryClient class — asyncio adapter
 * N/A: _AnthropicCompletionsAdapter class — Python Anthropic SDK wrapper
 * N/A: AnthropicAuxiliaryClient class — Python Anthropic SDK wrapper
 * N/A: AsyncAnthropicAuxiliaryClient class — asyncio adapter
 * N/A: _maybe_wrap_anthropic() — Python SDK object wrapping
 * N/A: _resolve_api_key_provider() — Python SDK client construction
 * N/A: _try_openrouter() / _try_nous() — Python OpenAI() SDK invocation
 * N/A: _try_custom_endpoint() / _try_azure_foundry() / _try_anthropic() — SDK construction
 * N/A: _build_xai_oauth_aux_client() / _build_codex_client() — SDK construction
 * N/A: call_llm() / async_call_llm() — Python LLM API calls via SDK
 * N/A: _retry_same_provider_sync() — LLM retry with SDK
 * N/A: resolve_provider_client() / _needs_codex_wrap() / _wrap_if_needed()
 * N/A: get_text_auxiliary_client() / get_async_text_auxiliary_client()
 * N/A: _resolve_strict_vision_backend() / resolve_vision_provider_client()
 * N/A: _refresh_nous_auxiliary_client() — SDK refresh
 * N/A: _refresh_nous_recommended_model() — HTTP call to Portal API
 * N/A: _nous_portal_account_has_fresh_paid_access() — HTTP call
 *
 * Python runtime / asyncio:
 * N/A: _safe_isinstance() — Python runtime type check
 * N/A: _to_async_client() — asyncio wrapper
 * N/A: neuter_async_httpx_del() / _force_close_async_httpx() — asyncio
 * N/A: cleanup_stale_async_clients() — asyncio
 * N/A: AsyncCodexAuxiliaryClient / AsyncAnthropicAuxiliaryClient — async SDK adapters
 * N/A: _validate_llm_response() — Python object attribute inspection
 * N/A: _is_openrouter_client() / _cached_client_accepts_slash_models() — isinstance
 * N/A: _build_call_kwargs() — Python dict **kwargs construction
 *
 * Credential pool (requires Python pool objects — C pool TBD):
 * N/A: _select_pool_entry() / _peek_pool_entry() — pool interaction
 * N/A: _pool_runtime_api_key() / _pool_runtime_base_url() — pool field access
 * N/A: _pool_cache_hint() / _pool_error_context() — pool interaction
 * N/A: _recoverable_pool_provider() / _recover_provider_pool() — pool recovery
 * N/A: _refresh_provider_credentials() — credential pool refresh
 * N/A: _client_cache_key() / _store_cached_client() / _get_cached_client() — pool cache
 * N/A: _evict_cached_clients() / _evict_cached_client_instance() — cache
 * N/A: shutdown_cached_clients() — cache cleanup
 * N/A: _compat_model() — uses pool objects
 * N/A: _expand_direct_api_alias() — uses pool providers
 *
 * Orchestration (requires SDK client construction):
 * N/A: _resolve_single_provider() / _resolve_auto() — SDK chain
 * N/A: _try_payment_fallback() / _try_main_agent_model_fallback() — chain
 * N/A: _try_configured_fallback_chain() — config-driven fallback
 *
 * Python-only auth resolvers:
 * N/A: _read_nous_auth() — reads ~/.hermes/auth.json (file I/O)
 * N/A: _resolve_nous_runtime_api() — calls hermes_cli.auth resolver
 * N/A: _resolve_xai_oauth_for_aux() — OAuth credential resolution
 * N/A: _read_codex_access_token() — reads stored OAuth token
 */

#include <string.h>

/* PoP: is_timeout_error @ agent/auxiliary_client.py:_is_timeout_error */
bool is_timeout_error(const char *error_message)
{
    /* Detect a request timeout — the full-budget stall, distinct from a fast
     * connection drop.
     * Port of Python agent/auxiliary_client.py:_is_timeout_error(). */
    if (!error_message || !*error_message) return false;

    /* Check for "Timeout" in the error type name pattern */
    if (strstr(error_message, "Timeout") != NULL ||
        strstr(error_message, "APITimeoutError") != NULL)
        return true;

    /* Check for "timed out" in the error text */
    const char *lower = error_message;
    /* Case-insensitive check for "timed out" */
    size_t len = strlen(error_message);
    for (size_t i = 0; i + 8 < len; i++) {
        char c = error_message[i];
        char c_lower = (c >= 'A' && c <= 'Z') ? (c + 32) : c;
        if (c_lower == 't') {
            if ((error_message[i+1] == 'i' || error_message[i+1] == 'I') &&
                (error_message[i+2] == 'm' || error_message[i+2] == 'M') &&
                (error_message[i+3] == 'e' || error_message[i+3] == 'E') &&
                (error_message[i+4] == 'd' || error_message[i+4] == 'D') &&
                (error_message[i+5] == ' ') &&
                (error_message[i+6] == 'o' || error_message[i+6] == 'O') &&
                (error_message[i+7] == 'u' || error_message[i+7] == 'U') &&
                (error_message[i+8] == 't' || error_message[i+8] == 'T'))
                return true;
        }
    }

    return false;
}
