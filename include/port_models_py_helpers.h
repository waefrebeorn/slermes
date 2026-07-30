/*
 * port_models_py_helpers.h — Faithful C11 ports of the module-level pure
 * helpers from Python hermes_cli/models.py that are still REAL_GAPs in the
 * parity battleground.
 *
 * Every function carries its exact PoP comment so the scanner credits it.
 * Network-dependent helpers (_urlopen_model_catalog_request,
 * _fetch_deepinfra_catalog, ollama_model_supports_thinking) follow the
 * injectable http_fetch_fn transport convention used by port_models_net.c so
 * their parse/filter logic stays testable offline.
 */

#ifndef PORT_MODELS_PY_HELPERS_H
#define PORT_MODELS_PY_HELPERS_H

#include <stddef.h>
#include <stdbool.h>

typedef struct json_t json_t;

/* Injectable HTTP transport (mirrors port_models_net.h). Returns 0 on success
 * (fills *out_body with a malloc'd response body, caller frees) or non-zero. */
typedef int (*http_fetch_fn)(const char *url,
                             const char *headers_json,
                             char **out_body,
                             size_t *out_len,
                             void *ctx);

/* A default live transport backed by libhttp (used when callers pass NULL). */
int models_live_http_fetch(const char *url, const char *headers_json,
                           char **out_body, size_t *out_len, void *ctx);

/* ============================================================
 * hermes_cli/models.py — pure helpers
 * ============================================================ */

/* compute_sale_discount(prompt, completion, original) -> (pct, was_prompt, was_completion)
 * or NULL (no sale). Returns malloc'd strings; *out_pct set, *out_wp/*out_wc malloc'd. */
bool models_compute_sale_discount(const char *prompt, const char *completion,
                                  const json_t *original,
                                  int *out_pct, char **out_was_prompt, char **out_was_completion);

/* normalize_opencode_base_url(provider_id, api_mode, base_url) -> normalized str (caller frees). */
char *models_normalize_opencode_base_url(const char *provider_id, const char *api_mode, const char *base_url);

/* _deepinfra_catalog_url() -> (cache_key, full_url) via out params (caller frees). */
void models_deepinfra_catalog_url(char **out_cache_key, char **out_full_url);

/* deepinfra_base_url(section_json) -> normalized str (caller frees). */
char *models_deepinfra_base_url(const json_t *section);

/* _fireworks_pricing_from_models_dev() -> json object {id:{prompt,completion,...}} (caller frees). */
json_t *models_fireworks_pricing_from_models_dev(int force_refresh);

/* In-memory catalog transforms (no network). Pass a parsed DeepInfra catalog
 * data array (json_t array) and a tag; returns filtered list of {id,metadata}
 * or NULL. */
json_t *models_deepinfra_models_by_tag(const json_t *catalog_data, const char *tag);
/* Returns malloc'd array of model-id strings (caller frees each + array) and count. */
char **models_deepinfra_model_ids(const json_t *catalog_data, const char *tag, int *out_count);
/* chat-only id list convenience; returns malloc'd id array + count. */
char **models_fetch_deepinfra_models(const json_t *catalog_data, int *out_count);
/* pricing transform from a models-by-tag list -> {id:{prompt,completion,input_cache_read}}. */
json_t *models_fetch_deepinfra_pricing(const json_t *tagged_models);

/* Network: fetch raw DeepInfra catalog (injectable transport). Returns parsed
 * json_t array (caller frees) or NULL. Uses module-level caches. */
json_t *models_fetch_deepinfra_catalog(http_fetch_fn fetch, void *ctx, int force_refresh);

/* _urlopen_model_catalog_request(req_url, headers_json, timeout) -> body (caller frees) or NULL. */
char *models_urlopen_model_catalog_request(http_fetch_fn fetch, void *ctx,
                                           const char *url, const char *headers_json, double timeout);

/* ollama_model_supports_thinking(model, base_url, api_key, timeout) -> 1 true, 0 false, -1 probe failed. */
int models_ollama_model_supports_thinking(http_fetch_fn fetch, void *ctx,
                                          const char *model, const char *base_url,
                                          const char *api_key, double timeout);

#endif /* PORT_MODELS_PY_HELPERS_H */
