#ifndef NOC_W15_PLUGINSET_B71C40_H
#define NOC_W15_PLUGINSET_B71C40_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_pluginsset_b71c40_secret_env_name @ hermes_cli/plugins_settings.py:secret_env_name */
struct json_t *w15_pluginsset_b71c40_secret_env_name(const char *plugin_id, const char *key,
                                                    const struct json_t *spec);
/* PoP: w15_pluginsset_b71c40_field_type @ hermes_cli/plugins_settings.py:_field_type */
struct json_t *w15_pluginsset_b71c40_field_type(const struct json_t *spec);
/* PoP: w15_pluginsset_b71c40_save_plugin_settings @ hermes_cli/plugins_settings.py:save_plugin_settings */
struct json_t *w15_pluginsset_b71c40_save_plugin_settings(const char *plugin_id,
                                                         const struct json_t *schema,
                                                         const struct json_t *values);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_PLUGINSET_B71C40_H */
