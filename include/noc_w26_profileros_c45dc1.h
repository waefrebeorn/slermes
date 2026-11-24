/*
 * noc_w26_profileros_c45dc1.h - C surface for upstream
 * tui_gateway/profile_roster_cache.py (gate w26_profileros_c45dc1).
 *
 * The roster memo core: how a profile's session-store files reduce to a
 * signature, the ``profile.yaml`` signature (with inode, because the atomic
 * writers rename a temp file into place), and the bounded memo that reuses a
 * computed field set while its signature holds.  Every path/stat seam is a
 * fixed input here: the caller supplies each file's (name, mtime_ns, size,
 * inode) and the current clock in nanoseconds.
 */
#ifndef NOC_W26_PROFILEROS_C45DC1_H
#define NOC_W26_PROFILEROS_C45DC1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* names in _STORE_FILES */
#define W26_PROFILEROS_C45DC1_STORE_FILES 2

/* req: {"files": [{"name", "mtime_ns", "size", "exists", "inode"}...],
 *       "now_ns": n} -> [[name, mtime_ns, size]...] or null.
 * Mirrors store_signature(): an empty -wal sidecar is not a move, and any
 * racy mtime disables the memo for this poll. */
json_t *w26_profileros_c45dc1_store_signature(const json_t *req);

/* req: {"files": [...]} -> [name, mtime_ns, size, inode] or null. */
json_t *w26_profileros_c45dc1_yaml_signature(const json_t *req);

/* req: {"cache": [{"key": [...], "signature": [...]|null, "fields": {...}}...],
 *       "key": [...], "signature": [...]|null, "fields": {...},
 *       "copier": "dict"|"deep", "max_entries": n}
 * -> {"fields": {...}, "cache": [...]}  the memo's answer and its new state. */
json_t *w26_profileros_c45dc1_cached(const json_t *req);

/* The memo bounds, as an object. */
json_t *w26_profileros_c45dc1_constants(void);

#ifdef __cplusplus
}
#endif
#endif
