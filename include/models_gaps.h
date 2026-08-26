/*
 * models_gaps.h — final REAL_GAP closure batch for hermes_cli/models.py.
 *
 * Every function here carries a single-line PoP annotation on its definition
 * in src/hermes_cli/models_gaps.c so the parity scanner credits it.
 * Conventions follow port_models_net.h / port_models_pure.h:
 *   - JSON via libjson json_t
 *   - network via injectable http_fetch_fn (NULL -> models_live_http_fetch)
 *   - config.yaml via libyaml (yaml_get_* dotted paths)
 */
#ifndef MODELS_GAPS_H
#define MODELS_GAPS_H

#include <stddef.h>
#include <stdbool.h>
#include "libjson/json.h"
#include "port_models_net.h"   /* http_fetch_fn */

#ifdef __cplusplus
extern "C" {
#endif

/* ── TLS / custom-provider ssl-context ──────────────────────────────────── */
typedef struct {
    bool verify_off;        /* ssl_verify is False => CERT_NONE */
    char ca_cert[1024];     /* ssl_ca_cert path, "" when unset  */
} models_tls_ctx_t;

char *models_config_path(void);   /* re-export of config_path() */
json_t *models_load_config(void); /* parsed config.yaml tree or NULL; caller frees */
json_t *models_get_provider_config_dict(const char *provider);
json_t *models_get_model_config_dict_simple(void);
char *models_get_custom_base_url(void);
models_tls_ctx_t *models_custom_provider_ssl_context(const char *base_url);

/* ── portal / keys ──────────────────────────────────────────────────────── */
char *models_resolve_nous_portal_url(void);
char *models_resolve_openrouter_api_key(void);
char *models_resolve_copilot_catalog_api_key(void);
void models_resolve_anthropic_pool_catalog_credentials(char **out_token,
                                                       char **out_endpoint);

/* ── reasoning-caps warmers (thread-backed) ─────────────────────────────── */
void models_warm_openrouter_reasoning_caps_async(void);
void models_warm_nous_reasoning_caps_async(void);

/* ── pricing cache + dispatch ───────────────────────────────────────────── */
json_t *models_cached_catalog(const char *cache_key);
json_t *models_cache_catalog(const char *cache_key, json_t *result); /* takes ownership of result */
char *models_fetch_novita_pricing(int force_refresh);   /* malloc'd JSON object */
char *models_get_pricing_for_provider(const char *provider, int force_refresh);

/* ── provider listing ───────────────────────────────────────────────────── */
char *models_configured_custom_provider_ids_json(void); /* malloc'd JSON array */
char *models_list_available_providers_json(void);       /* malloc'd JSON array */

/* ── ollama native API ──────────────────────────────────────────────────── */
char *models_root_for_ollama_native_api(const char *base_url);
char *models_normalize_openai_base_url(const char *base_url);
char *models_get_ollama_base_url(void);
char *models_get_ollama_request_headers_json(void);
char *models_get_ollama_native_headers_json(const char *base_url, const char *api_key);
int models_same_ollama_native_root(const char *left, const char *right);
char *models_ollama_probe_cache_key(const char *root, const char *headers_json);
char **models_probe_ollama_local_models(const char *base_url, double timeout,
                                        const char *headers_json); /* NULL=unreachable */
char **models_fetch_ollama_local_models(const char *base_url, double timeout,
                                        const char *headers_json);
int models_should_use_ollama_native_catalog(const char *provider, const char *base_url,
                                            const char *headers_json);

/* ── misc pure helpers ──────────────────────────────────────────────────── */
int models_base_url_looks_like_anthropic_messages(const char *base_url);
char *models_openrouter_variant_base(const char *model_id);
char **models_detect_provider_for_model(const char *model_name,
                                        const char *current_provider); /* NULL or [provider,model] */
int models_is_openai_fast_model2(const char *model_id);
char *models_resolve_fast_mode_overrides(const char *model_id); /* malloc'd JSON or NULL */
char **models_merge_with_models_dev_live(char *const *curated); /* curated merged w/ models.dev */
void models_update_provider_cache_entry(const char *provider, char *const *models);
char *models_copilot_default_headers_json(int is_agent_turn);
char *models_get_copilot_model_context_str(const char *model_id, const char *api_key);
char **models_copilot_catalog_ids_fetch(http_fetch_fn fetch, void *ctx,
                                        json_t *catalog, const char *api_key);
char **models_github_reasoning_efforts_for_model_id(const char *model_id);
char *models_opencode_provider_family2(const char *provider_id);
int models_is_opencode_zen_free_model(const char *model_id);
char *models_opencode_zen_free_headers_json(void);
char *models_opencode_zen_free_runtime_json(const char *provider_id, const char *model_id);
char *models_model_ids_json(int force_refresh);      /* malloc'd JSON array */
char **models_cached_fetch_api_models(const char *api_key, const char *base_url,
                                      double timeout, const char *api_mode,
                                      const char *headers_json, int force_refresh,
                                      int cache_only);
char *models_load_ollama_cloud_cache(int ignore_ttl);
void models_save_ollama_cloud_cache_json(const char *models_json_array);

#ifdef __cplusplus
}
#endif
#endif /* MODELS_GAPS_H */
