#ifndef NOC_W14_SETUPMIGRA_BD703D_H
#define NOC_W14_SETUPMIGRA_BD703D_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_setupmigra_bd703d_model_summary @ hermes_cli/setup_migration.py:_model_summary
 * Credential-gated model section summary; returns a JSON string or null. */
struct json_t *w14_setupmigra_bd703d_model_summary(const struct json_t *in);

/* PoP: w14_setupmigra_bd703d_cfg_summary @ hermes_cli/setup_migration.py:_cfg_summary
 * "prefix" + cfg_get(config, section, key, default), Python str() coercion. */
struct json_t *w14_setupmigra_bd703d_cfg_summary(const struct json_t *in);

/* PoP: w14_setupmigra_bd703d_get_section_summary @ hermes_cli/setup_migration.py:_get_section_config_summary
 * Dispatch a setup section key to its configured summary (model/terminal/agent). */
struct json_t *w14_setupmigra_bd703d_get_section_summary(const struct json_t *in);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_SETUPMIGRA_BD703D_H */
