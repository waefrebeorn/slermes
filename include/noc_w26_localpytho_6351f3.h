/*
 * noc_w26_localpytho_6351f3.h - C surface for upstream
 * tools/environments/local_pythonpath.py (gate w26_localpytho_6351f3).
 *
 * Hermes-owned PYTHONPATH stripping, decided purely by path provenance: two
 * path spellings are the same file when their normcased parts agree, and an
 * env mapping is rewritten by removing exactly those entries.  Everything the
 * live module reaches for a filesystem (site-packages discovery, the venv
 * check, the repo-root aliases) arrives here as a fixed input list.
 */
#ifndef NOC_W26_LOCALPYTHO_6351F3_H
#define NOC_W26_LOCALPYTHO_6351F3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* os.path.normcase(parts) equality: {"out": true|false}. */
json_t *w26_localpytho_6351f3_same_path(const char *left, const char *right);

/* env, owned paths (site-packages + repo-root aliases), pathsep ->
 * {"env": {...}} with Hermes-owned PYTHONPATH entries removed. */
json_t *w26_localpytho_6351f3_strip_pythonpath(const json_t *env,
                                              const json_t *owned_paths,
                                              const char *sep);

/* Same, then drop the active-venv marker vars. */
json_t *w26_localpytho_6351f3_strip_all(const json_t *env,
                                       const json_t *owned_paths,
                                       const char *sep);

/* {"markers": [...], "is_windows": bool, "pathsep": ";"} */
json_t *w26_localpytho_6351f3_constants(void);

#ifdef __cplusplus
}
#endif
#endif
