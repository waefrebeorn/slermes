#ifndef NOC_W21_AUTHDECLAR_1A88FD_H
#define NOC_W21_AUTHDECLAR_1A88FD_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/auth_declared_providers.py (gate w21_authdeclar_1a88fd).
 *
 * Credential/base-URL resolution for API-key providers, including a declared
 * `providers.<id>` block that PROVIDER_REGISTRY has never heard of. Every live
 * IO edge is a seam:
 *   load_config()                 -> the fixture's `config` object (already
 *                                    ${VAR}-expanded by the loader upstream);
 *   get_env_value_prefer_dotenv() -> the fixture's `env` map;
 *   key_cmd minting               -> the fixture's `key_cmd_values` map;
 *   the owning-entry credential   -> the fixture's `owning` object;
 *   is_known_auth_provider()      -> the fixture's `known_provider` flag.
 * The ordering, the ${VAR}/${env:VAR} expansion, the strip/rstrip shaping, the
 * enabled/registry checks and the usable-secret test are all ported.
 */

/* PoP: w21_authdeclar_1a88fd_default_api_key_base_url @ hermes_cli/auth_declared_providers.py:default_api_key_base_url
 * `env_url.rstrip("/") if env_url else default` (the api_key is unused by the
 * live helper and is kept in the signature for parity). */
json_t *w21_authdeclar_1a88fd_default_api_key_base_url(const char *api_key,
                                                      const char *default_url,
                                                      const char *env_url);

/* PoP: w21_authdeclar_1a88fd_origin @ utils/urls.py:base_url_origin
 * `(scheme, hostname, effective_port)`; ("", "", 0) when there is no host or
 * the port is not a usable number. Answers {"scheme","host","port"}. */
json_t *w21_authdeclar_1a88fd_origin(const char *base_url);

/* PoP: w21_authdeclar_1a88fd_relay_probe_api_key @ hermes_cli/auth_declared_providers.py:relay_probe_api_key
 * `api_key` when it may ride *relay*'s catalog probe, else "". A registry
 * provider's key always rides; a declared provider's key only when both URLs
 * share one origin (host compared case-insensitively, literal otherwise). */
json_t *w21_authdeclar_1a88fd_relay_probe_api_key(const char *provider_id,
                                                 const char *relay,
                                                 const char *api_key,
                                                 const char *cred_base_url,
                                                 int known_provider);

/* PoP: w21_authdeclar_1a88fd_resolve_declared @ hermes_cli/auth_declared_providers.py:resolve_declared_api_key_provider
 * Credentials for an enabled `providers.<id>` block.
 *   config          -- {"providers": {...}} as load_config() returns it
 *   env             -- {"<VAR>": "<value>"} for key_env / api_key_env
 *   key_cmd_values  -- {"<key_cmd>": "<minted token>"}
 * Answers {"status":"ok","value":{"provider","api_key","base_url","source"}} or
 * {"status":"error","type":"AuthError","message":...,"provider":...,"code":"invalid_provider"}. */
json_t *w21_authdeclar_1a88fd_resolve_declared(const char *provider_id,
                                              const json_t *config,
                                              const json_t *env,
                                              const json_t *key_cmd_values);

/* PoP: w21_authdeclar_1a88fd_custom_catalog @ hermes_cli/auth_declared_providers.py:custom_catalog_probe_api_key
 * Key for a bare `custom` catalog probe of base_url, else "" (keyless).
 *   case = {
 *     "model_cfg": {...},                       # model.* config
 *     "base_url": "...",                        # the probed URL
 *     "env": {"<VAR>": "<value>"},              # key_env + CUSTOM_API_KEY
 *     "owning": {"declared": bool, "key": ...}, # _owning_entry_credential()
 *     "minted": bool,                           # mint a key_cmd token source?
 *     "key_cmd_values": {"<src>": "<token>"}    # materialize_probe_api_key()
 *   }
 * Answers the key string ("" when keyless). */
json_t *w21_authdeclar_1a88fd_custom_catalog(const json_t *case_);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_AUTHDECLAR_1A88FD_H */
