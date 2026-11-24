#ifndef NOC_W18_CONFIGRUNT_356CCE_H
#define NOC_W18_CONFIGRUNT_356CCE_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_configrunt_356cce_covered @ hermes_cli/config_runtime_keys.py:_covered */
int w18_configrunt_356cce_covered(const char *key, const struct json_t *paths);

/* PoP: w18_configrunt_356cce_is_runtime_read_key @ hermes_cli/config_runtime_keys.py:is_runtime_read_key
 * preset / selection / plugin are injected seams for the late-imported helpers
 * is_preset_field_path / selection_config_keys / plugin_declared_config_keys. */
int w18_configrunt_356cce_is_runtime_read_key(const struct json_t *parts, int preset,
                                              const struct json_t *selection,
                                              const struct json_t *plugin);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W18_CONFIGRUNT_356CCE_H */
