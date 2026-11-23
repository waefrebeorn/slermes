#ifndef NOC_W10_PLUGINSSTA_A8131D_H
#define NOC_W10_PLUGINSSTA_A8131D_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w10_pluginssta_a8131d_relative_segments @ hermes_cli/plugins_state.py:_plugin_relative_segments
 * Returns {"ok":true,"segments":[...]} or {"ok":false,"error":"ValueError"}.
 * Caller frees. */
json_t *w10_pluginssta_a8131d_relative_segments(const char *key);

/* PoP: w10_pluginssta_a8131d_nested_value @ hermes_cli/plugins_state.py:_nested_plugin_value
 * Walk `segments` (a JSON array of strings) through nested objects in `root`;
 * return `default` on the first miss. Caller frees. */
json_t *w10_pluginssta_a8131d_nested_value(const json_t *root, const json_t *segments, const json_t *def);

/* PoP: w10_pluginssta_a8131d_nested_mapping @ hermes_cli/plugins_state.py:_nested_plugin_mapping
 * Wrap `value` in nested single-key objects, outermost segment first. Caller frees. */
json_t *w10_pluginssta_a8131d_nested_mapping(const json_t *segments, const json_t *value);

/* PoP: w10_pluginssta_a8131d_state_key_valid @ hermes_cli/plugins_state.py:PluginState._validate_key
 * Returns true when key matches ^[A-Za-z0-9][A-Za-z0-9_.:-]{0,127}$ and has no "..". */
json_t *w10_pluginssta_a8131d_state_key_valid(const char *key);

/* PoP: w10_pluginssta_a8131d_data_namespace @ hermes_cli/plugins_state.py:_plugin_data_namespace
 * `portable_ns` is an injected seam for _portable_data_namespace(candidate).
 * Returns one Windows-safe directory component. Caller frees. */
json_t *w10_pluginssta_a8131d_data_namespace(const char *plugin_id,
                                             const char *data_namespace,
                                             const char *portable_ns);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_PLUGINSSTA_A8131D_H */
