/*
 * auxiliary_client.h — Resolve auxiliary task provider/model config.
 *
 * Port of Python auxiliary_client.py's provider resolution chain,
 * model predicates, header builders, and error classification.
 * Functions marked N/A are Python-specific (SDK wrappers, async, config I/O).
 */

#ifndef HERMES_AUXILIARY_CLIENT_H
#define HERMES_AUXILIARY_CLIENT_H

#include "hermes_core_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Constants
 * ================================================================ */

#define AUX_OPENROUTER_MODEL        "google/gemini-3-flash-preview"
#define AUX_NOUS_MODEL              "google/gemini-3-flash-preview"
#define AUX_NOUS_DEFAULT_BASE_URL   "https://inference-api.nousresearch.com/v1"
#define AUX_ANTHROPIC_DEFAULT_BASE_URL "https://api.anthropic.com"
#define AUX_CODEX_BASE_URL          "https://chatgpt.com/backend-api/codex"
#define AUX_DEFAULT_TIMEOUT         30
#define AUX_OR_REFERER              "https://hermes-agent.nousresearch.com"
#define AUX_OR_TITLE                "Slermes Agent"
#define AUX_OR_CATEGORIES           "productivity,cli-agent"
#define AUX_NVIDIA_ORIGIN           "HermesAgent"

/* Truthy env values */
#define AUX_TRUTHY_ENV_VALUES       "1,true,yes,on"

/* Provider health tracking */
#define AUX_HEALTH_UNHEALTHY_TTL    60
#define AUX_HEALTH_MAX_PROVIDERS    64

/* ================================================================
 *  Provider aliases (Port of Python _PROVIDER_ALIASES)
 * ================================================================ */

/**
 * Normalize a provider name through the alias table.
 * Port of Python _normalize_aux_provider() and _PROVIDER_ALIASES dict.
 *
 * @param provider Provider name (e.g. "google", "x-ai", "claude").
 *                 NULL or empty defaults to "auto".
 * @return Static string — the normalized provider name. Never NULL.
 *         "auto" returned for NULL/empty/unknown.
 */
const char *auxiliary_normalize_provider(const char *provider);

/* ================================================================
 *  Model predicates (Port of Python model-specific checks)
 * ================================================================ */

/**
 * True for any Kimi / Moonshot model that manages temperature server-side.
 * Port of Python _is_kimi_model().
 */
bool is_kimi_model(const char *model);

/**
 * True for Arcee Trinity Large Thinking.
 * Port of Python _is_arcee_trinity_thinking().
 */
bool is_arcee_trinity_thinking(const char *model);

/**
 * Return temperature directive for models with strict contracts.
 * Port of Python _fixed_temperature_for_model().
 *
 * Returns:
 *   Negative value (e.g. -1.0f) → OMIT_TEMPERATURE — caller must strip key.
 *   0.0f or positive → specific value to use.
 *   -0.0f → no override (caller's default).
 */
#define AUX_OMIT_TEMPERATURE (-1.0f)
float fixed_temperature_for_model(const char *model, const char *base_url);

/**
 * Return context-compression threshold override for specific models.
 * Port of Python _compression_threshold_for_model().
 *
 * Returns a value in (0, 1] to override, or 0.0f for no override.
 */
float compression_threshold_for_model(const char *model);

/**
 * Return the cheap auxiliary model name for a provider.
 * Port of Python _get_aux_model_for_provider() and
 * _API_KEY_PROVIDER_AUX_MODELS_FALLBACK dict.
 *
 * @return Static string or empty string if no known aux model.
 */
const char *get_aux_model_for_provider(const char *provider_id);

/**
 * Return the vision model override for a provider, or NULL if none.
 * Port of Python _PROVIDER_VISION_MODELS dict.
 */
const char *auxiliary_get_vision_model_for_provider(const char *provider_id);

/**
 * True if the provider's endpoint does not accept image input.
 * Port of Python _PROVIDERS_WITHOUT_VISION frozenset.
 */
bool auxiliary_provider_without_vision(const char *provider_id);

/* ================================================================
 *  Header builders (Port of Python header-building functions)
 * ================================================================ */

/**
 * Build OpenRouter headers into a buffer.
 * Port of Python build_or_headers().
 *
 * @param out       Output buffer for the full header JSON.
 * @param out_size  Size of out buffer.
 * @param cache_enabled  Whether response cache should be requested.
 * @param cache_ttl      Cache TTL in seconds (0 = no TTL override).
 */
void build_or_headers(char *out, size_t out_size,
                                 bool cache_enabled, int cache_ttl);

/**
 * Return NVIDIA NIM cloud billing headers as JSON, or empty string.
 * Port of Python build_nvidia_nim_headers().
 */
void build_nvidia_nim_headers(char *out, size_t out_size,
                                         const char *base_url);

/**
 * Build Nous Portal extra_body tags JSON.
 * Port of Python _nous_extra_body().
 */
void build_nous_extra_body(char *out, size_t out_size);

/**
 * Build Codex Cloudflare headers into a buffer.
 * Port of Python _codex_cloudflare_headers().
 */
void auxiliary_build_codex_headers(char *out, size_t out_size,
                                    const char *access_token);

/* ================================================================
 *  URL helpers (Port of Python URL manipulation functions)
 * ================================================================ */

/**
 * Normalize an Anthropic-style base URL to OpenAI-compatible format.
 * Port of Python _to_openai_base_url().
 *
 * @param out       Output buffer.
 * @param out_size  Size of out.
 * @param base_url  Input base URL.
 */
void to_openai_base_url(char *out, size_t out_size,
                                   const char *base_url);

/**
 * True if the endpoint speaks Anthropic Messages protocol.
 * Port of Python _endpoint_speaks_anthropic_messages().
 */
bool endpoint_speaks_anthropic(const char *base_url);

/**
 * True if the endpoint is an Anthropic-compatible endpoint
 * (provider is "anthropic" or URL suggests it).
 * Port of Python _is_anthropic_compat_endpoint().
 */
bool is_anthropic_compat_endpoint(const char *provider,
                                             const char *base_url);

/**
 * Validate a base URL — returns true if it looks valid.
 * Port of Python _validate_base_url().
 */
bool validate_base_url(const char *base_url);

/* ================================================================
 *  Error classification (Port of Python error predicates)
 * ================================================================ */

/**
 * True if the HTTP status is a payment/402 error.
 * Port of Python _is_payment_error().
 */
bool is_payment_error(int http_status, const char *response_body);

/**
 * True if the response suggests a rate limit error (HTTP 429 etc.).
 * Port of Python _is_rate_limit_error().
 */
bool is_rate_limit_error(int http_status, const char *response_body);

/**
 * True if the response suggests a connection error.
 * Port of Python _is_connection_error(const char *error_message).
 */
bool is_connection_error(const char *error_message);

/**
 * True if the response suggests an auth error (HTTP 401/403).
 * Port of Python _is_auth_error().
 */
bool is_auth_error(int http_status, const char *response_body);

/**
 * True if the response suggests model not found (HTTP 404 with model msg).
 * Port of Python _is_model_not_found_error().
 */
bool is_model_not_found_error(int http_status,
                                         const char *response_body);

/**
 * True if the response suggests temperature is unsupported.
 * Port of Python _is_unsupported_temperature_error().
 */
bool is_unsupported_temperature_error(const char *response_body);

/* ================================================================
 *  Provider health tracking (Port of Python health functions)
 * ================================================================ */

/**
 * Mark a provider as unhealthy for a TTL duration.
 * Port of Python _mark_provider_unhealthy().
 */
void mark_provider_unhealthy(const char *provider, int ttl_seconds);

/**
 * True if the provider is currently marked unhealthy.
 * Port of Python _is_provider_unhealthy().
 */
bool is_provider_unhealthy(const char *provider);

/**
 * Reset all provider health state.
 * Port of Python _reset_aux_unhealthy_cache().
 */
void auxiliary_reset_unhealthy_cache(void);

/* ================================================================
 *  Content extraction (Port of Python extract_content_or_reasoning)
 * ================================================================ */

/**
 * Extract content or reasoning text from an LLM response.
 * Port of Python extract_content_or_reasoning().
 */
const char *extract_content_or_reasoning(const llm_response_t *resp);

/* ================================================================
 *  Provider chain labels (Port of Python chain helpers)
 * ================================================================ */

/**
 * Return a human-readable label for a provider in the resolution chain.
 * Port of Python _normalize_chain_label().
 */
const char *normalize_chain_label(const char *provider);

/**
 * Return a description of why OpenRouter is unavailable.
 * Port of Python _describe_openrouter_unavailable().
 */
const char *describe_openrouter_unavailable(void);

/* ================================================================
 *  Vision helpers (Port of Python vision functions)
 * ================================================================ */

/**
 * True if the given provider+model combination supports vision.
 * Port of Python _main_model_supports_vision().
 */
bool main_model_supports_vision(const char *provider,
                                           const char *model);

/**
 * Normalize a vision provider name.
 * Port of Python _normalize_vision_provider().
 */
const char *normalize_vision_provider(const char *provider);

/**
 * True if a strict vision backend is available for the provider.
 * Port of Python _strict_vision_backend_available().
 */
bool strict_vision_backend_available(const char *provider);

/**
 * Return a space-separated list of available vision backends.
 * Port of Python get_available_vision_backends().
 */
const char *get_available_vision_backends(void);

/**
 * Normalize a resolved model name for a provider.
 * Port of Python _normalize_resolved_model().
 */
const char *auxiliary_normalize_resolved_model(const char *model_name,
                                                 const char *provider);

/**
 * Return "auxiliary_max_tokens" param JSON for a value.
 * Port of Python auxiliary_max_tokens_param().
 */
void auxiliary_max_tokens_param(char *out, size_t out_size, int value);

/* ================================================================
 *  Task config resolution (existing + expanded)
 * ================================================================ */

/**
 * Return a static description string for the task name.
 */
const char *auxiliary_task_label(const char *task_name);

/**
 * Resolve an auxiliary task config into an llm_config_t.
 * Port of Python auxiliary_client.py's provider resolution chain.
 *
 * @param cfg         Global hermes config.
 * @param task_cfg    Specific auxiliary task config.
 * @param out         [out] Populated llm_config_t.
 * @return true on success.
 */
bool auxiliary_resolve_llm_config(const hermes_config_t *cfg,
                                   const auxiliary_task_config_t *task_cfg,
                                   llm_config_t *out);

/**
 * Convenience: resolve auxiliary task by name string.
 * Looks up cfg->auxiliary.<name> and calls auxiliary_resolve_llm_config.
 *
 * @param cfg       Global hermes config.
 * @param task_name Task name, e.g. "compression", "vision".
 * @param out       [out] Populated llm_config_t.
 * @return true on success.
 */
bool resolve_task(const hermes_config_t *cfg,
                             const char *task_name,
                             llm_config_t *out);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_AUXILIARY_CLIENT_H */
