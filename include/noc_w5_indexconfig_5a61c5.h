#ifndef NOC_W5_INDEXCONFIG_5A61C5_H
#define NOC_W5_INDEXCONFIG_5A61C5_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w5_indexconfig_5a61c5_is_forwarded @ pm/index_config.py:is_forwarded */
int w5_indexconfig_5a61c5_is_forwarded(const char *key);

/* PoP: w5_indexconfig_5a61c5_is_index_redirect @ pm/index_config.py:is_index_redirect */
int w5_indexconfig_5a61c5_is_index_redirect(const char *key);

/* PoP: w5_indexconfig_5a61c5_ini_unsafe @ pm/index_config.py:_ini_unsafe
 * Returns a malloc'd decoded string. Caller frees. */
char *w5_indexconfig_5a61c5_ini_unsafe(const char *raw);

/* PoP: w5_indexconfig_5a61c5_npmrc_env_replace @ pm/index_config.py:_npmrc_env_replace
 * Returns a malloc'd string, or NULL for Python None (unset non-optional var). */
char *w5_indexconfig_5a61c5_npmrc_env_replace(const char *text, const struct json_t *env);

/* PoP: w5_indexconfig_5a61c5_npm_registry @ pm/index_config.py:npm_registry
 * Explicit seams: env_registry is the stripped npm_config_registry value,
 * npmrc_registry is registry= from the user npmrc. Returns malloc'd string. */
char *w5_indexconfig_5a61c5_npm_registry_ex(const char *env_registry, const char *npmrc_registry);

/* PoP: w5_indexconfig_5a61c5_npm_registry_url @ pm/index_config.py:npm_registry_url
 * Same seams as npm_registry_ex. Returns malloc'd string. */
char *w5_indexconfig_5a61c5_npm_registry_url_ex(const char *url, const char *env_registry,
                                                 const char *npmrc_registry);

/* PoP: w5_indexconfig_5a61c5_bridged_index_settings @ pm/index_config.py:bridged_index_settings
 * Explicit seam: pip_conf_index_url stands in for pip_conf_index_url(ambient).
 * Returns a NEW json object (caller frees). */
struct json_t *w5_indexconfig_5a61c5_bridged_index_settings_ex(const struct json_t *ambient,
                                                               const char *pip_conf_index_url);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W5_INDEXCONFIG_5A61C5_H */
