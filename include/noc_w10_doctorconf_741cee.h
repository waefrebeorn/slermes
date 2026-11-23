#ifndef NOC_W10_DOCTORCONF_741CEE_H
#define NOC_W10_DOCTORCONF_741CEE_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w10_doctorconf_741cee_collect_deprecated_config_keys @ hermes_cli/doctor_config.py:collect_deprecated_config_keys
 * Returns a JSON array of [legacy_path, replacement] pairs; empty array for non-dict input. */
struct json_t *w10_doctorconf_741cee_collect_deprecated_config_keys(const struct json_t *raw_config);
/* PoP: w10_doctorconf_741cee_collect_deprecated_env_vars @ hermes_cli/doctor_config.py:collect_deprecated_env_vars
 * Returns a JSON array of [legacy_env, replacement] pairs from the on-disk env map. */
struct json_t *w10_doctorconf_741cee_collect_deprecated_env_vars(const struct json_t *env_map);
/* PoP: w10_doctorconf_741cee_endpoint_url @ hermes_cli/doctor_config.py:_endpoint_url
 * str(api or base_url or url).strip().rstrip("/").lower() */
struct json_t *w10_doctorconf_741cee_endpoint_url(const struct json_t *entry);
/* PoP: w10_doctorconf_741cee_has_provider_env_config @ hermes_cli/doctor_config.py:_has_provider_env_config
 * any(hint in content for hint in doctor._PROVIDER_ENV_HINTS). */
int w10_doctorconf_741cee_has_provider_env_config(const char *content);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_DOCTORCONF_741CEE_H */
