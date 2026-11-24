/*
 * noc_w17_pluginscmd_207eca.h -- public C API for the deterministic dashboard
 * removal decision of hermes_cli/plugins_cmd_remove.py
 * (wave17 gate w17_pluginscmd_207eca).
 */
#ifndef NOC_W17_PLUGINSCMD_207ECA_H
#define NOC_W17_PLUGINSCMD_207ECA_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Faithful port of the error-decision half of dashboard_remove_user_plugin:
 * bundled plugins are refused; an otherwise missing user-installed plugin gets
 * the "was not found under <plugins_dir>" error. `entries` is the
 * _discover_all_plugins() array of [name, version, description, source, path, key].
 * Returns a fresh JSON object; caller frees. */
json_t *w17_pluginscmd_207eca_dashboard_remove_error(const char *name, json_t *entries,
                                                     const char *plugins_dir);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_PLUGINSCMD_207ECA_H */
