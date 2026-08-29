/*
 * models_gaps.h — final REAL_GAP closure batch for hermes_cli/models.py.
 *
 * Every function here carries a single-line PoP annotation on its definition
 * in src/hermes_cli/models_gaps.c so the parity scanner credits it.
 * Conventions follow port_models_net.h / port_models_pure.h:
 *   - JSON via libjson json_t
 *   - network via injectable http_fetch_fn (NULL -> models_live_http_fetch)
 *   - config.yaml via libyaml (config_py_read_raw_config)
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
char *models_get_ollama_request_headers(void);              /* malloc'd JSON obj */
char *models_get_ollama_native_headers(const char *base_url,
                                       const char *api_key);/* malloc'd JSON obj */
void models_evict_related_ollama_cache_entries(const char *key);
void models_remember_ollama_cache(int cache_id, const char *key, const char *value_json);
char *models_ollama_probe_cache_key(const char *root, const char *headers_json);
char *models_probe_ollama_local_models(const char *base_url, double timeout,
                                       const char *headers_json); /* JSON array or NULL */
char *models_fetch_ollama_local_models(const char *base_url, double timeout,
                                       const char *headers_json);
int models_same_ollama_native_root(const char *left, const char *right);
int models_should_use_ollama_native_catalog(const char *provider,
                                            const char *base_url,
                                            const char *headers_json);

/* ── catalog resolution ─────────────────────────────────────────────────── */
char *models_model_ids(int force_refresh);                  /* malloc'd JSON array */
char *models_openrouter_variant_base(const char *model_id); /* malloc'd or NULL */
int models_detect_provider_for_model(const char *model_name,
                                     const char *current_provider,
                                     char **out_provider,
                                     char **out_model);
char *models_merge_with_models_dev(const char *provider, const char *curated_json);
void models_update_provider_cache_entry(const char *provider, const char *models_json);
long models_get_copilot_model_context(const char *model_id, const char *api_key); /* -1 = unknown */
char *models_fetch_github_models(const char *api_key);      /* JSON array or NULL */
char *models_copilot_catalog_ids(const char *catalog_json, const char *api_key);
char *models_lmstudio_model_reasoning_options(const char *model, const char *base_url,
                                              const char *api_key);  /* JSON array */
char *models_cached_fetch_api_models(const char *api_key, const char *base_url,
                                     int cache_only, int force_refresh); /* JSON array or NULL */

/* ── opencode zen free tier ─────────────────────────────────────────────── */
int models_is_opencode_zen_free_model(const char *model_id);
json_t *models_opencode_zen_free_headers(void);               /* malloc'd JSON obj */
char *models_opencode_zen_free_runtime(const char *provider_id,
                                       const char *model_id); /* malloc'd JSON obj or NULL */

/* ── ollama cloud disk cache ────────────────────────────────────────────── */
char *models_load_ollama_cloud_cache(int ignore_ttl);       /* JSON obj or NULL */
void models_save_ollama_cloud_cache(const char *models_json);

/* ── batch-2 additions (pure + catalog resolution) ──────────────────────── */
char *models_xai_finalize_catalog(const char *ids_json);    /* JSON array in/out */
char *models_get_default_model_for_provider(const char *provider);

/* ── reasoning-caps internals (thread-backed warmers + fetch) ───────────── */
void models_warm_reasoning_caps_async(void (*refresh)(void));
json_t *models_hydrate_reasoning_caps_from_disk(const char *url, void (*refresh)(void));
json_t *models_fetch_reasoning_caps_catalog(http_fetch_fn fetch, void *ctx,
                                            const char *url, double timeout);
json_t *models_fetch_openrouter_reasoning_caps(http_fetch_fn fetch, void *ctx,
                                               double timeout, int force);
void models_refresh_openrouter_reasoning_caps(void);
json_t *models_openrouter_caps_cached(void);
json_t *models_fetch_nous_reasoning_caps(http_fetch_fn fetch, void *ctx,
                                         double timeout, int force);
void models_refresh_nous_reasoning_caps(void);
json_t *models_nous_caps_cached(void);

#ifdef __cplusplus
}
#endif

#endif /* MODELS_GAPS_H */
