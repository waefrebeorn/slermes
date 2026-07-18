/*
 * port_models_pure.h — second cohesive port module for hermes_cli/models.py.
 *
 * Covers the remaining pure/config/disk-cache helpers and the network wrappers
 * (LM Studio, GitHub Copilot, custom /v1 probes, Ollama Cloud disk cache,
 * pricing) that were not already ported in port_models_net.c.
 *
 * All network access goes through the injectable `http_fetch_fn` transport
 * (declared in port_models_net.h) so the logic is testable offline. Credential
 * and config resolution is likewise injected (NULL → sensible default), so the
 * module stays pure and dependency-light per AGENTS.md.
 */

#ifndef PORT_MODELS_PURE_H
#define PORT_MODELS_PURE_H

#include <stddef.h>
#include <stddef.h>
#include <stdbool.h>

/* Full json_t definition (so json_t* is a complete type everywhere in this
 * module's prototypes, avoiding incomplete-vs-complete pointer warnings). */
#include "libjson/json.h"

/* Re-use the injectable HTTP transport from port_models_net.h. */
#include "port_models_net.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Credential / config resolvers (injectable; NULL → default) ─────────── */

/* Injectable resolver returning (api_key, base_url) for Nous pricing. */
typedef void (*nous_creds_fn)(char **out_key, char **out_base, void *ctx);

/* Injectable resolver returning the active model config dict as JSON. */
typedef char *(*model_config_fn)(void *ctx);

/* Injectable resolver returning the Copilot catalog API key (or ""). */
typedef char *(*copilot_apikey_fn)(void *ctx);

/* ── OpenRouter / Novita / Nous pricing ────────────────────────────────── */

/* Faithful port of models.py:_resolve_openrouter_api_key. Returns a malloc'd
 * key from OPENROUTER_API_KEY (or ""); caller frees. */
char *models_resolve_openrouter_api_key(void);

/* Faithful port of models.py:_resolve_nous_pricing_credentials. Fills *out_key
 * and *out_base (caller frees). Uses the injected resolver when provided. */
void models_resolve_nous_pricing_credentials(nous_creds_fn resolve, void *ctx,
                                             char **out_key, char **out_base);

/* Faithful port of models.py:fetch_models_with_pricing. Fetches /v1/models and
 * returns a malloc'd JSON object {model_id:{prompt,completion,...}}. Caller
 * frees. On failure returns a malloc'd "{}". */
char *models_fetch_models_with_pricing(http_fetch_fn fetch, void *ctx,
                                       const char *api_key, const char *base_url,
                                       int force_refresh);

/* Faithful port of models.py:_fetch_novita_pricing. Returns a malloc'd JSON
 * object {model_id:{prompt,completion}} (Novita units converted). Caller frees.
 * Returns "{}" when no api_key. */
char *models_fetch_novita_pricing(http_fetch_fn fetch, void *ctx, int force_refresh);

/* Faithful port of models.py:get_pricing_for_provider. Returns a malloc'd JSON
 * object for openrouter/nous/novita, or "{}" otherwise. Caller frees. */
char *models_get_pricing_for_provider(http_fetch_fn fetch, void *ctx,
                                      nous_creds_fn resolve_nous, void *nous_ctx,
                                      const char *provider, int force_refresh);

/* ── LM Studio native API helpers ──────────────────────────────────────── */

/* Faithful port of models.py:_lmstudio_server_root. */
char *models_lmstudio_server_root(const char *base_url);

/* Faithful port of models.py:_lmstudio_request_headers. Returns malloc'd JSON
 * headers object string; caller frees. */
char *models_lmstudio_request_headers(const char *api_key);

/* Faithful port of models.py:_lmstudio_fetch_raw_models (injectable HTTP).
 * Returns a malloc'd JSON array of raw model dicts, or NULL. Caller frees. */
json_t *models_lmstudio_fetch_raw_models(http_fetch_fn fetch, void *ctx,
                                         const char *api_key, const char *base_url);

/* Faithful port of models.py:probe_lmstudio_models. Returns a malloc'd
 * NULL-terminated array of chat-capable model keys (caller frees each + array),
 * or NULL on unreachable/malformed. */
char **models_probe_lmstudio_models(http_fetch_fn fetch, void *ctx,
                                    const char *api_key, const char *base_url);

/* Faithful port of models.py:lmstudio_model_reasoning_options. Returns a
 * malloc'd NULL-terminated array of option strings (caller frees). */
char **models_lmstudio_reasoning_options(http_fetch_fn fetch, void *ctx,
                                         const char *model, const char *base_url,
                                         const char *api_key);

/* ── GitHub Copilot catalog helpers ────────────────────────────────────── */

/* Faithful port of models.py:_fetch_github_models. Returns a malloc'd
 * NULL-terminated array of model ids (caller frees each + array), or NULL. */
char **models_fetch_github_models(http_fetch_fn fetch, void *ctx,
                                  const char *api_key);

/* Faithful port of models.py:_copilot_catalog_ids. Returns a malloc'd
 * NULL-terminated array of catalog ids (caller frees each + array). */
char **models_copilot_catalog_ids(http_fetch_fn fetch, void *ctx,
                                  json_t *catalog, const char *api_key);

/* Faithful port of models.py:_github_reasoning_efforts_for_model_id. Returns a
 * malloc'd NULL-terminated array of effort strings (caller frees each + array). */
char **models_github_reasoning_efforts_for_id(const char *model_id);

/* Faithful port of models.py:copilot_model_api_mode. `catalog` may be NULL;
 * returns one of "codex_responses"/"anthropic_messages"/"chat_completions". */
char *models_copilot_model_api_mode(http_fetch_fn fetch, void *ctx,
                                    const char *model_id, json_t *catalog,
                                    const char *api_key);

/* Faithful port of models.py:github_model_reasoning_efforts. Returns a
 * malloc'd NULL-terminated array of effort strings (caller frees each + array). */
char **models_github_model_reasoning_efforts(http_fetch_fn fetch, void *ctx,
                                             const char *model_id, json_t *catalog,
                                             const char *api_key);

/* Faithful port of models.py:get_copilot_model_context. Returns malloc'd max
 * prompt tokens (as string) or NULL. Caller frees. (Network via fetch.) */
char *models_get_copilot_model_context(http_fetch_fn fetch, void *ctx,
                                       const char *model_id, const char *api_key);

/* ── Custom /v1 probe helpers ──────────────────────────────────────────── */

/* Faithful port of models.py:probe_api_models. Returns a malloc'd JSON object
 * {models,probed_url,resolved_base_url,suggested_base_url,used_fallback}.
 * models is a JSON array or null. Caller frees. */
char *models_probe_api_models(http_fetch_fn fetch, void *ctx,
                              const char *api_key, const char *base_url,
                              const char *api_mode);

/* Faithful port of models.py:fetch_api_models. Returns a malloc'd
 * NULL-terminated array of model ids (caller frees each + array), or NULL. */
char **models_fetch_api_models(http_fetch_fn fetch, void *ctx,
                               const char *api_key, const char *base_url,
                               const char *api_mode);

/* ── Config / base-url resolvers (injectable) ──────────────────────────── */

/* Faithful port of models.py:_get_custom_base_url. Returns malloc'd base URL
 * from HERMES_CUSTOM_BASE_URL / config, or "" — caller frees. */
char *models_get_custom_base_url(void);

/* Faithful port of models.py:_get_model_config_dict. Returns malloc'd JSON
 * config dict (caller frees) via injected getter, else "{}". */
char *models_get_model_config_dict(model_config_fn get, void *ctx);

/* Faithful port of models.py:_resolve_copilot_catalog_api_key. Returns malloc'd
 * key (caller frees) via injected getter, else OPENAI/COPILOT env. */
char *models_resolve_copilot_catalog_api_key(copilot_apikey_fn get, void *ctx);

/* Faithful port of models.py:_resolve_nous_portal_url. Returns malloc'd portal
 * base URL (caller frees). */
char *models_resolve_nous_portal_url(void);

/* ── Ollama Cloud disk cache ───────────────────────────────────────────── */

/* Faithful port of models.py:_ollama_cloud_cache_path. Returns malloc'd path;
 * caller frees. */
char *models_ollama_cloud_cache_path(void);

/* Faithful port of models.py:_load_ollama_cloud_cache. Returns malloc'd JSON
 * cache object (caller frees) or NULL. ignore_ttl honors stale fallback. */
char *models_load_ollama_cloud_cache(int ignore_ttl);

/* Faithful port of models.py:_save_ollama_cloud_cache. Persists JSON cache
 * object (already serialized) atomically; non-fatal on failure. */
void models_save_ollama_cloud_cache(const char *json);

/* ── Nous free-tier / portal recommendation unions ─────────────────────── */

/* Faithful port of models.py:check_nous_free_tier. Returns 1 if the account is
 * free-tier (best-effort), else 0. Uses injected pricing resolver. */
int models_check_nous_free_tier(http_fetch_fn fetch, void *ctx,
                                nous_creds_fn resolve_nous, void *nous_ctx);

/* Faithful port of models.py:_merge_with_models_dev. Returns a malloc'd
 * NULL-terminated merged array (caller frees each + array) from live + dev
 * registry lists. */
char **models_merge_with_models_dev(char *const *live, char *const *mdev);

/* Faithful port of models.py:union_with_portal_free_recommendations. Returns a
 * malloc'd NULL-terminated array merging `base` with the portal recommended
 * free-tier list. */
char **models_union_portal_free_recommendations(http_fetch_fn fetch, void *ctx,
                                                 const char *portal_base_url,
                                                 char *const *base);

/* Faithful port of models.py:union_with_portal_paid_recommendations. Returns a
 * malloc'd NULL-terminated array merging `base` with the portal recommended
 * paid-tier list. */
char **models_union_portal_paid_recommendations(http_fetch_fn fetch, void *ctx,
                                                 const char *portal_base_url,
                                                 char *const *base);

#ifdef __cplusplus
}
#endif

#endif /* PORT_MODELS_PURE_H */
