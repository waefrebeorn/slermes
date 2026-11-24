/*
 * noc_w18_pluginscmd_1fa477.h -- public C API for the deterministic
 * list-filter tail of hermes_cli/plugins_cmd_listing.py
 * (wave18 gate w18_pluginscmd_1fa477).
 */
#ifndef NOC_W18_PLUGINSCMD_1FA477_H
#define NOC_W18_PLUGINSCMD_1FA477_H

#include <stddef.h>
#include <stdbool.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_pluginscmd_1fa477_filter_entries @ hermes_cli/plugins_cmd_listing.py:_filter_plugin_entries
 * Faithful port of the --no-bundled/--user source filter and the --enabled
 * status filter used by `hermes plugins list`.
 *   entries    : JSON array of [name, version, description, source, dir, key]
 *   no_bundled : args.no_bundled
 *   user       : args.user
 *   enabled    : args.enabled (when true, keep only status == "enabled")
 *   status_map : JSON object name -> status string (the facade's _plugin_status)
 * Returns a new JSON array; caller frees it. */
json_t *w18_pluginscmd_1fa477_filter_entries(json_t *entries, bool no_bundled, bool user,
                                             bool enabled, json_t *status_map);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_PLUGINSCMD_1FA477_H */
