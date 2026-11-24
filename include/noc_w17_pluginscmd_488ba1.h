/*
 * noc_w17_pluginscmd_488ba1.h -- public C API for the pure re-install message
 * of hermes_cli/plugins_cmd_update.py (wave17 gate w17_pluginscmd_488ba1).
 */
#ifndef NOC_W17_PLUGINSCMD_488BA1_H
#define NOC_W17_PLUGINSCMD_488BA1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Faithful port of the deterministic tail of _reclone_plugin_update:
 *   previous = previous_revision if isinstance(str) else ""
 *   if revision and revision == previous: "Already up to date."
 *   else: f"Re-installed from {source}: {previous[:8]}..{revision[:8]}"
 * `previous_revision` is any JSON value; `source`/`revision` are strings.
 * Caller frees the returned string. */
char *w17_pluginscmd_488ba1_reclone_revision_message(const char *source,
                                                     json_t *previous_revision,
                                                     const char *revision);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_PLUGINSCMD_488BA1_H */
