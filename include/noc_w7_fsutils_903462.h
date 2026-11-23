#ifndef NOC_W7_FSUTILS_903462_H
#define NOC_W7_FSUTILS_903462_H
/*
 * Public C API for the port of hermes_cli/fs_utils.py (gate w7_fsutils_903462).
 *
 * rmtree_force mirrors shutil.rmtree(path, onerror=...) where onerror chmods
 * the failing entry to S_IWRITE and retries. The caller supplies the set of
 * read-only relative paths so the C side reproduces the same permission state
 * as the oracle without mutating the process environment.
 */
#include "hermes_json.h"

/* Remove a directory tree, clearing read-only bits before retrying failures.
 * Returns 1 when the path no longer exists, 0 otherwise. */
int w7_fsutils_rmtree_force(const char *path, const json_t *readonly_paths);

#endif /* NOC_W7_FSUTILS_903462_H */
