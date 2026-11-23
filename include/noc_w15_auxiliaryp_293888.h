#ifndef NOC_W15_AUXILIARYP_293888_H
#define NOC_W15_AUXILIARYP_293888_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_auxiliaryp_293888_configured_endpoint @ agent/auxiliary_plugin_oauth.py:_configured_endpoint
 * `cfg or profile` (empty string when neither). Caller frees. */
struct json_t *w15_auxiliaryp_293888_configured_endpoint(const char *config_base_url,
                                                          const char *profile_inference_base_url);

/* PoP: w15_auxiliaryp_293888_resolve @ agent/auxiliary_plugin_oauth.py:resolve_plugin_oauth_client
 * All aux.* helper results are injected explicitly: normalized_api_key, pool
 * selection/key/base, configured endpoint, client availability and the
 * normalized model. Returns [client|null, model]. Caller frees. */
struct json_t *w15_auxiliaryp_293888_resolve(const struct json_t *req,
                                              const char *normalized_api_key,
                                              int pool_selected,
                                              const char *pool_runtime_key,
                                              const char *pool_runtime_base_url,
                                              const char *configured_endpoint,
                                              int profile_client_available,
                                              const char *model_candidate,
                                              const char *normalized_model);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W15_AUXILIARYP_293888_H */
