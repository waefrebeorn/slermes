/*
 * noc_w17_pluginscmd_645b9b.h -- public C API for the pure selection logic of
 * hermes_cli/plugins_cmd_toggle.py (wave17 gate w17_pluginscmd_645b9b).
 */
#ifndef NOC_W17_PLUGINSCMD_645B9B_H
#define NOC_W17_PLUGINSCMD_645B9B_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of _effective_plugin_selection: row indices ticked on open, using the
 * load-time status rule (explicit disable wins, then enabled/active names).
 * entries: array of [name, version, description, source, dir_path, key]; the
 * dir_path slot is null in fixtures (the filesystem default-on probe is not
 * part of this deterministic gate). enabled/disabled/active: JSON string arrays.
 * Returns a fresh JSON array of integer indices (ascending); caller frees. */
json_t *w17_pluginscmd_645b9b_effective_plugin_selection(json_t *entries, json_t *enabled,
                                                         json_t *disabled, json_t *active);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_PLUGINSCMD_645B9B_H */
