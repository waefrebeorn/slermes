/*
 * port_models_net.h — public API for the network-backed + pure model-catalog
 * helpers ported from hermes_cli/models.py.
 *
 * The fetch functions take an injectable HTTP transport so their parse/filter
 * logic is faithfully testable offline (per hermes_agent AGENTS.md: exercise
 * the real path, never stub).
 */

#ifndef PORT_MODELS_NET_H
#define PORT_MODELS_NET_H

#include <stddef.h>
#include <stdbool.h>

/* Forward declaration so the header need not drag in libjson. */
typedef struct json_t json_t;

/* Resolve the shared Hermes home dir (HERMES_HOME / SLERMES_HOME / HOME).
 * Shared by port_hermes_cli_models.c and port_models_pure.c. */
void hermes_home_dir(char *out, size_t sz);

#ifdef __cplusplus
extern "C" {
#endif

/* Injectable HTTP transport. Returns 0 on success (fills *out_body with a
 * malloc'd response body, caller frees) or a non-zero error code. */
typedef int (*http_fetch_fn)(const char *url,
                             const char *headers_json,
                             char **out_body,
                             size_t *out_len,
                             void *ctx);

/* ── Pure parse helpers ────────────────────────────────────────────────── */

/* Faithful port of models.py:_copilot_catalog_item_is_text_model. */
int models_copilot_item_is_text_model(const json_t *item);

/* Faithful port of models.py:_is_github_models_base_url. */
int models_is_github_models_base_url(const char *base_url);

/* ── Network fetchers (injectable HTTP) ─────────────────────────────────── */

/* Faithful port of models.py:fetch_github_model_catalog. Returns a malloc'd
 * JSON array of matched text-model items (caller frees) or NULL. */
char *models_fetch_github_model_catalog(http_fetch_fn fetch, void *ctx,
                                        const char *api_key);

/* Faithful port of models.py:_fetch_anthropic_models. Returns a malloc'd JSON
 * array of model-id strings (opus>sonnet>haiku, alphabetical within tier) or
 * NULL. base_url may be NULL. */
char *models_fetch_anthropic_models(http_fetch_fn fetch, void *ctx,
                                    const char *base_url, const char *api_key);

/* Faithful port of models.py:fetch_lmstudio_models. server_root may be NULL
 * (defaults to http://localhost:1234). Returns malloc'd JSON array of ids. */
char *models_fetch_lmstudio_models(http_fetch_fn fetch, void *ctx,
                                   const char *server_root);

/* Faithful port of models.py:fetch_ollama_cloud_models. server_root may be
 * NULL (defaults to https://api.ollama.com). Returns malloc'd JSON array. */
char *models_fetch_ollama_cloud_models(http_fetch_fn fetch, void *ctx,
                                       const char *server_root);

/* ── Pure model-tier / alias helpers ────────────────────────────────────── */

/* Faithful port of models.py:_provider_keys. */
void models_provider_keys(const char *provider,
                          char *out_key, size_t key_sz,
                          char *out_norm, size_t norm_sz);

/* Faithful port of models.py:_model_in_provider_catalog. providers is a
 * NULL-terminated array of normalized provider slugs. */
int models_model_in_provider_catalog(const char *name_lower,
                                     char *const *providers);

/* Faithful port of models.py:_xai_promote_top. ids is NULL-terminated; out is
 * caller-sized to cap entries. */
void models_xai_promote_top(char *const *ids, char **out, size_t cap);

/* Faithful port of models.py:_xai_merge_curated_extras. ids is a NULL-terminated
 * array (caller-sized with room to grow). */
void models_xai_merge_curated_extras(char **ids, size_t cap);

/* Faithful port of models.py:_xai_curated_models. Fills *out (caller frees
 * each + array) with a NULL-terminated array; *out_n is the count. */
void models_xai_curated_models(char ***out, size_t *out_n);

/* Faithful port of models.py:_codex_curated_models. Fills *out (caller frees
 * each + array) with a NULL-terminated array; *out_n is the count. */
void models_codex_curated_models(char ***out, size_t *out_n);

/* Faithful port of models.py:get_nous_recommended_aux_model. Returns a
 * malloc'd model id or NULL. */
char *models_nous_recommended_aux_model(const char *recommended_json);

/* ── Nous recommended-models disk cache (pure file IO) ─────────────────── */

/* Faithful port of models.py:_nous_recommended_disk_path. Returns a malloc'd
 * path to the persisted recommended-models cache JSON. Caller frees. */
char *nous_recommended_disk_path(void);

/* Faithful port of models.py:_read_nous_recommended_disk. Reads the disk cache
 * and returns the last-known-good data payload for `base` as a malloc'd JSON
 * string (already JSON-encoded), or NULL. Caller frees. */
char *read_nous_recommended_disk(const char *base);

/* Faithful port of models.py:_write_nous_recommended_disk. Persists `data`
 * (JSON-encoded string) as the last-known-good payload for `base`, merged
 * into any existing per-base map, written atomically. Non-fatal on failure. */
void models_write_nous_recommended_disk(const char *base, const char *data_json);

/* Faithful port of models.py:get_curated_nous_model_ids. Fills *out (caller
 * frees each + array) with the Nous curated list, preferring the live catalog
 * manifest (via injectable fetch) and falling back to the static
 * _PROVIDER_MODELS["nous"] snapshot. */
void models_get_curated_nous_model_ids(http_fetch_fn fetch, void *ctx,
                                        char ***out, size_t *out_n);

/* Faithful port of models.py:fetch_nous_recommended_models. Hits the Portal
 * recommended-models endpoint (injectable HTTP), returns the parsed JSON dict
 * string, or falls back to the disk cache on failure. Caller frees. */
char *models_fetch_nous_recommended_models(http_fetch_fn fetch, void *ctx,
                                            const char *portal_base_url,
                                            int force_refresh);

#ifdef __cplusplus
}
#endif

#endif /* PORT_MODELS_NET_H */
