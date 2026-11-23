#ifndef NOC_W15_CONFIGPROV_34DD3F_H
#define NOC_W15_CONFIGPROV_34DD3F_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C home for upstream Hermes hermes_cli/config_providers.py (gate w15_configprov_34dd3f).
 * All results are freshly allocated json_t values freed by the caller. */

/* PoP: w15_configprov_34dd3f_canonical_api_mode @ hermes_cli/config_providers.py:_canonical_api_mode */
struct json_t *w15_configprov_34dd3f_canonical_api_mode(const char *api_mode);

/* PoP: w15_configprov_34dd3f_coerce_provider_id @ hermes_cli/config_providers.py:coerce_provider_id */
struct json_t *w15_configprov_34dd3f_coerce_provider_id(const struct json_t *value);

/* PoP: w15_configprov_34dd3f_stringify_provider_map @ hermes_cli/config_providers.py:stringify_provider_map */
struct json_t *w15_configprov_34dd3f_stringify_provider_map(const struct json_t *providers);

/* PoP: w15_configprov_34dd3f_find_provider_entry @ hermes_cli/config_providers.py:find_provider_entry
 * Python (stored_key, entry) is emitted as a 2-element JSON array. */
struct json_t *w15_configprov_34dd3f_find_provider_entry(const struct json_t *providers,
                                                        const struct json_t *key);

/* PoP: w15_configprov_34dd3f_normalize_provider_models @ hermes_cli/config_providers.py:_normalize_provider_models
 * Python (models_dict, discovered_flag) is emitted as a 2-element JSON array. */
struct json_t *w15_configprov_34dd3f_normalize_provider_models(const struct json_t *models);

/* PoP: w15_configprov_34dd3f_coerce_ssl_verify @ hermes_cli/config_providers.py:_coerce_ssl_verify */
struct json_t *w15_configprov_34dd3f_coerce_ssl_verify(const struct json_t *value);

/* PoP: w15_configprov_34dd3f_normalize_extra_headers @ hermes_cli/config_providers.py:normalize_extra_headers */
struct json_t *w15_configprov_34dd3f_normalize_extra_headers(const struct json_t *extra_headers);

/* PoP: w15_configprov_34dd3f_is_provider_enabled @ hermes_cli/config_providers.py:is_provider_enabled */
struct json_t *w15_configprov_34dd3f_is_provider_enabled(const struct json_t *provider_cfg);

/* PoP: w15_configprov_34dd3f_route_model_cfg @ hermes_cli/config_providers.py:_route_model_cfg */
struct json_t *w15_configprov_34dd3f_route_model_cfg(const struct json_t *entry,
                                                     const char *model);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_CONFIGPROV_34DD3F_H */
