#ifndef NOC_W7_WORKERBOOTS_FDB251_H
#define NOC_W7_WORKERBOOTS_FDB251_H
#include "hermes_json.h"
#include <stdbool.h>

/* C home for upstream Hermes cron/worker_bootstrap.py (gate w7_workerboots_fdb251).
 * WORKER_MARKER = "_HERMES_CRON_WORKER_BOOT" is mirrored as a literal below. */

/* PoP: w7_workerboots_worker_bootstrap_ex @ cron/worker_bootstrap.py:worker_bootstrap
 * Explicit env seam (Windows CRT setenv("",1) unsets; Python keeps "" as falsy):
 *   if not os.environ.get(WORKER_MARKER): return
 *   activate_dependencies(Path(project_root_str()))
 * Returns ["noop"] when the marker is absent/empty, else ["activate", str(Path(root))]
 * (Windows pathlib form: separators normalised, "" -> "."). */
json_t *w7_workerboots_worker_bootstrap_ex(bool has_marker, const char *marker_value,
                                           const char *project_root);

#endif /* NOC_W7_WORKERBOOTS_FDB251_H */
