/*
 * port_models_helpers.h — public API for the pure, network-free model
 * helper functions ported from hermes_cli/models.py.
 *
 * These helpers contain no big static catalogs, no live /v1/models fetch,
 * and no file-walking — only string/value coercion, small constant prefix
 * matching, and JSON-struct extraction. They are the *single* home for
 * vendor-prefix stripping, fast-mode detection, credential fingerprinting and
 * the provider-models cache path. Other catalog modules (model_catalog.c)
 * must call these instead of redefining them.
 *
 * Minimal includes: only <stddef.h>. Every parameter is a plain char-pointer
 * or int, so no other types leak into this header.
 */

#ifndef SLERMES_PORT_MODELS_HELPERS_H
#define SLERMES_PORT_MODELS_HELPERS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Model name coercion ───────────────────────────────────────────── */

/* Strip a leading "vendor/" prefix; returns a malloc'd lowercased string.
 * Caller frees. (PoP: hermes_cli/models.py:_strip_vendor_prefix) */
char *strip_vendor_prefix(const char *model_id);

/* Strip a trailing ":cloud" / "-cloud" suffix. Returns malloc'd string.
 * (PoP: hermes_cli/models.py:_strip_ollama_cloud_suffix) */
char *strip_ollama_cloud_suffix(const char *model_id);

/* Pull modelName out of a recommended-model entry JSON. Returns malloc'd
 * string or NULL. Caller frees. (PoP: .../_extract_model_name) */
char *extract_model_name(const char *entry_json);

/* Convert a per-token price string to "$/Mtok" (2 dp) or "free"/"?".
 * Returns malloc'd string. Caller frees. (PoP: .../_format_price_per_mtok) */
char *format_price_per_mtok(const char *per_token_str);

/* ── Fast-mode capability (single source of truth) ────────────────── */

/* True if model_id is an OpenAI fast-mode model (gpt-/o1/o3/o4, not codex).
 * (PoP: hermes_cli/models.py:_is_openai_fast_model) */
int is_openai_fast_model(const char *model_id);

/* True if model_id is an Anthropic fast-mode model (claude- opus-4-6/-4.6).
 * (PoP: hermes_cli/models.py:_is_anthropic_fast_model) */
int is_anthropic_fast_model(const char *model_id);

/* True if model_id supports fast mode (anthropic OR openai). This is the
 * canonical model_supports_fast_mode — catalog modules MUST delegate to it
 * rather than redefining it. (PoP: hermes_cli/models.py:model_supports_fast_mode) */
int model_supports_fast_mode(const char *model_id);

/* Returns malloc'd JSON {"speed":"fast"} / {"service_tier":"priority"} or
 * NULL when unsupported. Caller frees. (PoP: .../resolve_fast_mode_overrides) */
char *fast_mode_overrides_json(const char *model_id);

/* ── Pricing / tier helpers ────────────────────────────────────────── */

/* True if pricing JSON shows prompt==0 AND completion==0 for model_id. */
int is_model_free(const char *model_id, const char *pricing_json);

/* True if account_info JSON indicates a free (unpaid) tier. */
int nous_free_tier_check(const char *account_info_json);

/* Returns malloc'd JSON {"selectable":[...],"unavailable":[...]}.
 * Caller frees. (PoP: .../partition_nous_models_by_tier) */
char *partition_nous_models_by_tier_json(const char *model_ids_json,
                                         const char *pricing_json, int free_tier);

/* True if a pricing JSON has prompt==0 AND completion==0 (free model). */
int openrouter_model_is_free(const char *pricing_json);

/* ── Base-url / endpoint helpers ───────────────────────────────────── */

/* True if normalized base URL path ends with /anthropic or /anthropic/v1. */
int base_url_looks_like_anthropic_messages(const char *base_url);

/* Append /v1/models or /models to a base URL. Returns malloc'd string.
 * Caller frees. (PoP: .../_anthropic_models_url) */
char *anthropic_models_url(const char *base_url);

/* True if base URL points at GitHub Models inference. (PoP: .../_is_github_models_base_url) */
int is_github_models_base_url(const char *base_url);

/* Strip /api/v1, /api, /v1 suffixes from an LM Studio base URL. Returns
 * malloc'd string or NULL. Caller frees. (PoP: .../_lmstudio_server_root) */
char *lmstudio_server_root(const char *base_url);

/* ── OpenCode / Azure / Copilot api-mode helpers ──────────────────── */

/* Normalize an OpenCode model id (drop "provider/" prefix when applicable).
 * Returns malloc'd string. Caller frees. (PoP: .../normalize_opencode_model_id) */
char *normalize_opencode_model_id(const char *provider_id, const char *model_id);

/* Return malloc'd api_mode ("anthropic_messages"/"codex_responses"/
 * "chat_completions"). Caller frees. (PoP: .../opencode_model_api_mode) */
char *opencode_model_api_mode(const char *provider_id, const char *model_id);

/* Return malloc'd "codex_responses" or NULL. Caller frees.
 * (PoP: .../azure_foundry_model_api_mode) */
char *azure_foundry_model_api_mode(const char *model_name);

/* True if a gpt-5+ model should use the Copilot Responses API.
 * (PoP: .../_should_use_copilot_responses_api) */
int should_use_copilot_responses_api(const char *model_id);

/* Return malloc'd JSON of Copilot fallback headers. Caller frees.
 * (PoP: .../copilot_default_headers) */
char *copilot_default_headers_json(void);

/* ── Fingerprint / cache-path helpers (single source of truth) ────── */

/* SHA-256 fingerprint of env vars + credential-file mtimes. Returns
 * malloc'd hex string. Caller frees. (PoP: .../_credential_fingerprint) */
char *credential_fingerprint(const char *provider);

/* Disk path to provider_models_cache.json. Returns malloc'd string.
 * Caller frees. (PoP: .../_provider_models_cache_path) */
char *provider_models_cache_path(void);

/* Disk path to ollama_cloud_models_cache.json. Returns malloc'd string.
 * Caller frees. (PoP: .../_ollama_cloud_cache_path) */
char *ollama_cloud_cache_path(void);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_PORT_MODELS_HELPERS_H */
