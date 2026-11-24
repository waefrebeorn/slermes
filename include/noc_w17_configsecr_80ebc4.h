#ifndef NOC_W17_CONFIGSECR_80EBC4_H
#define NOC_W17_CONFIGSECR_80EBC4_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of hermes_cli/config_secret_refs.py (gate w17_configsecr_80ebc4).
 * Deterministic .env variable naming for credential references; SHA-256 and
 * URL canonicalization are implemented locally so the names are identical. */
json_t *w17_configsecr_80ebc4_env_slug(const char *text);
json_t *w17_configsecr_80ebc4_model_api_key_env(void);
json_t *w17_configsecr_80ebc4_aux_api_key_env(const char *task);
json_t *w17_configsecr_80ebc4_digest(const char *text);
json_t *w17_configsecr_80ebc4_provider_name_key_env(const char *name);
json_t *w17_configsecr_80ebc4_is_env_reference(const char *value);
json_t *w17_configsecr_80ebc4_canonical_endpoint(const char *base_url);
json_t *w17_configsecr_80ebc4_endpoint_identity(const char *base_url);
json_t *w17_configsecr_80ebc4_endpoint_api_key_env(const char *base_url);
json_t *w17_configsecr_80ebc4_dashboard_endpoint_key_env(const json_t *entry,
                                                         const char *endpoint_id);
json_t *w17_configsecr_80ebc4_block_base_url(const json_t *block);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_CONFIGSECR_80EBC4_H */
