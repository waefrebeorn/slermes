/*
 * hive.h — C11 luddite hive: linked fixed blocks + skipfield + freelist.
 *
 * A cache-friendly stable-pointer collection with O(1) insert/erase and
 * iteration that jumps dead slots. Unlike a vector, element addresses are
 * STABLE across growth; unlike a linked list, iteration is cache-friendly
 * (block-contiguous) and erase is O(1) with no unlink.
 *
 *   struct block {
 *       void **slots;      // fixed-capacity slot array
 *       uint8_t *skip;     // skipfield: distance to next LIVE slot (0 = dead)
 *       size_t  live, cap; // live count / slot capacity
 *       struct block *next;
 *   };
 *
 * Erase  -> skipfield mark + freelist push (O(1), stable neighbours).
 * Insert -> freelist reuse first, else new block (amortized O(1)).
 * Iterate-> follow skipfield deltas, skip dead slots (O(live), cache-local).
 *
 * Pure C11. No templates, no macros for the user, opaque struct.
 */

#ifndef HERMES_HIVE_H
#define HERMES_HIVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque hive handle. */
typedef struct hive hive_t;

/* A stable element handle: block index + slot index packed into a size_t.
 * Iterators yield these; consumers pass them back for get/erase. */
typedef struct hive_handle {
    size_t block;   /* block ordinal */
    size_t slot;    /* slot within block */
} hive_handle_t;

/* Default block capacity when hive_new() is given 0. */
#define HIVE_DEFAULT_BLOCK_CAP 64

/* ── Lifecycle ─────────────────────────────────────────────────────────── */

/* Create a hive. block_cap: slots per block (0 = HIVE_DEFAULT_BLOCK_CAP).
 * Returns NULL on allocation failure. */
hive_t *hive_new(size_t block_cap);

/* Free the hive and all blocks. Does NOT free stored pointers. */
void hive_free(hive_t *h);

/* Remove all elements (keeps allocated blocks for reuse). */
void hive_clear(hive_t *h);

/* ── Accessors ─────────────────────────────────────────────────────────── */

/* Number of live elements. */
size_t hive_count(const hive_t *h);

/* Number of allocated blocks. */
size_t hive_block_count(const hive_t *h);

/* True when the hive is empty. */
bool hive_empty(const hive_t *h);

/* ── Insert ────────────────────────────────────────────────────────────── */

/* Append *ptr; returns its stable handle, or {0,0} with ok=false on OOM.
 * Reuses a freelist slot when one exists, else grows a new block. */
hive_handle_t hive_insert(hive_t *h, void *ptr, bool *ok);

/* ── Erase ─────────────────────────────────────────────────────────────── */

/* Remove the element at handle. The slot is pushed onto the freelist and
 * marked dead in the skipfield; neighbouring elements keep their handles.
 * Returns true when the handle was live. */
bool hive_erase(hive_t *h, hive_handle_t hnd);

/* ── Lookup ────────────────────────────────────────────────────────────── */

/* Return the pointer at handle, or NULL when dead/out of range. */
void *hive_get(const hive_t *h, hive_handle_t hnd);

/* True when handle currently holds a live element. */
bool hive_contains(const hive_t *h, hive_handle_t hnd);

/* ── Iteration ─────────────────────────────────────────────────────────── */

/* Iteration cursor: fill with HIVE_ITER_INIT, then advance. */
typedef struct hive_iter {
    size_t block;      /* current block ordinal */
    size_t slot;       /* current slot (next slot to visit on advance) */
    size_t remaining;  /* live elements still to visit */
} hive_iter_t;

#define HIVE_ITER_INIT { 0, 0, 0 }

/* Rewind iter to the first live element (call after any mutation). */
void hive_iter_begin(const hive_t *h, hive_iter_t *it);

/* Advance to the next live element. Returns false at end.
 * On success *out_handle and *out_ptr (if non-NULL) are filled. */
bool hive_iter_next(const hive_t *h, hive_iter_t *it,
                    hive_handle_t *out_handle, void **out_ptr);

/* ── Stats (diagnostics) ───────────────────────────────────────────────── */

/* Total slot capacity across all blocks (live + dead + free). */
size_t hive_capacity(const hive_t *h);

/* Total dead (erased) slots currently on the freelist. */
size_t hive_freelist_count(const hive_t *h);

/* Largest live-run in the skipfield (cache-locality metric). */
size_t hive_max_contiguous(const hive_t *h);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_HIVE_H */
