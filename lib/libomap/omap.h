/*
 * omap.h — C11 insertion-ordered string-keyed map (Python `dict` semantics).
 *
 * Slermes needs a real associative container in many ports: Python code is
 * full of `dict[str, X]` state that must preserve INSERTION ORDER (Python 3.7+
 * guarantees it, and ports observe that order when they iterate). `libhive`
 * gives stable slots but no keys; libjson gives keys but is a value tree, not
 * a container for arbitrary C pointers. This library is the missing piece.
 *
 * Design (compact-dict, the same shape CPython uses):
 *   - entries[]  : insertion-ordered array of {key, value, hash}. Erased
 *                  entries become tombstones (key == NULL) so the order of
 *                  the surviving entries never changes.
 *   - index[]    : open-addressed power-of-two table of int64 entry ordinals
 *                  (OMAP_EMPTY / OMAP_DELETED sentinels), giving O(1) lookup.
 *   - compaction : when tombstones exceed half the entry array, live entries
 *                  are slid down and the index rebuilt, so a long-lived map
 *                  under churn does not grow without bound.
 *
 * Set usage: insert with a NULL value; `omap_contains` is the membership test.
 *
 * Ownership: keys are copied into the map and freed by it. Values are opaque
 * to the map; supply a `value_free` callback to have erase/clear/free release
 * them, or NULL to keep them caller-owned.
 *
 * Thread-safety: none (caller locks) — matching the Python originals, which
 * guard their dicts with an explicit RLock rather than relying on the
 * container.
 *
 * Pure C11. Opaque struct. No dependencies beyond the C library.
 */

#ifndef HERMES_OMAP_H
#define HERMES_OMAP_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle. */
typedef struct omap omap_t;

/* Optional destructor invoked on a value when it leaves the map. */
typedef void (*omap_value_free_fn)(void *value);

/* ── Lifecycle ─────────────────────────────────────────────────────────── */

/* Create a map. `value_free` may be NULL (values stay caller-owned).
 * Returns NULL on allocation failure. */
omap_t *omap_new(omap_value_free_fn value_free);

/* Free the map, its keys, and (when a destructor was given) its values. */
void omap_free(omap_t *m);

/* Remove every entry (keys freed, values passed to the destructor). */
void omap_clear(omap_t *m);

/* ── Accessors ─────────────────────────────────────────────────────────── */

/* Number of live entries. */
size_t omap_size(const omap_t *m);

/* True when the map holds no live entries. */
bool omap_empty(const omap_t *m);

/* ── Insert / replace ──────────────────────────────────────────────────── */

/* Insert `key` -> `value`, or replace an existing key's value.
 * Replacing KEEPS the key's original insertion position (Python semantics)
 * and passes the displaced value to the destructor. Returns false on OOM. */
bool omap_set(omap_t *m, const char *key, void *value);

/* Insert only when absent (Python's `dict.setdefault`). `*out_value` receives
 * the value now stored under the key (existing one when present, otherwise
 * the value just inserted). Returns false only on OOM. */
bool omap_setdefault(omap_t *m, const char *key, void *value, void **out_value);

/* ── Lookup ────────────────────────────────────────────────────────────── */

/* Value for `key`, or NULL when absent. NULL is also a legal stored value —
 * use omap_contains to distinguish. */
void *omap_get(const omap_t *m, const char *key);

/* True when `key` is present. */
bool omap_contains(const omap_t *m, const char *key);

/* ── Erase ─────────────────────────────────────────────────────────────── */

/* Remove `key`, returning its value WITHOUT invoking the destructor
 * (Python's `dict.pop`). Returns NULL when absent. */
void *omap_pop(omap_t *m, const char *key);

/* Remove `key` and pass its value to the destructor (Python's `del d[k]` /
 * `set.discard`). Returns true when the key was present. */
bool omap_erase(omap_t *m, const char *key);

/* ── Ordered iteration ─────────────────────────────────────────────────── */

/* Live entries in insertion order. `index` runs 0..omap_size()-1; returns
 * false past the end. Either out pointer may be NULL.
 *
 *   const char *k; void *v;
 *   for (size_t i = 0; omap_at(m, i, &k, &v); i++) { ... }
 *
 * Erasing during iteration is NOT permitted (positions shift on compaction);
 * snapshot the keys first, exactly as the Python originals do with
 * `list(d.items())`. */
bool omap_at(const omap_t *m, size_t index, const char **out_key, void **out_value);

/* Snapshot of the live keys in insertion order (Python's `list(d)`).
 * Returns a malloc'd array of `omap_size()` pointers into the map's own key
 * storage (valid until the next mutation); free the array itself, not the
 * strings. `*out_count` receives the length. NULL when empty or on OOM. */
const char **omap_keys(const omap_t *m, size_t *out_count);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_OMAP_H */
