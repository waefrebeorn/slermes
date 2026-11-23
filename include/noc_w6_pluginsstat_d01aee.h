#ifndef NOC_W6_PLUGINSSTAT_D01AEE_H
#define NOC_W6_PLUGINSSTAT_D01AEE_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w6_pluginsstat_enabled_from_config @ pm/plugins_state.py:_enabled_from_config
 * Returns the order-preserving, deduplicated plugins.enabled list after
 * applying plugins.disabled (exact name and bare leaf), as a JSON array.
 * Caller frees. */
struct json_t *w6_pluginsstat_enabled_from_config(const struct json_t *config);

/* PoP: w6_pluginsstat_plugin_spellings_sorted @ pm/plugins_state.py:plugin_spellings
 * `dir_name` is the plugin directory name/path, `key` an optional selection
 * key, `manifest_name` the injected manifest `name:` value (NULL = none).
 * Returns the sorted set of spellings as a JSON array.  Caller frees. */
struct json_t *w6_pluginsstat_plugin_spellings_sorted(const char *dir_name,
                                                      const char *key,
                                                      const char *manifest_name);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W6_PLUGINSSTAT_D01AEE_H */
