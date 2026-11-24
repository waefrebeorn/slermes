/*
 * noc_w23_holder_f15834.h - C surface for upstream
 * persistence/sqlite/_holder.py (gate w23_holder_f15834).
 *
 * The pure decision core of the state.db StoreHolder: the write-contention
 * budget constants, the quarantine advisory and the async token-accounting
 * coalescer.  Every I/O seam (the writer connection, the read pool, the WAL
 * generation, the accounting thread) stays in the caller; this file owns only
 * what the values decide.
 */
#ifndef NOC_W23_HOLDER_F15834_H
#define NOC_W23_HOLDER_F15834_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Number of coalescable delta fields in each family. */
#define W23_HOLDER_F15834_SUM_FIELDS 6
#define W23_HOLDER_F15834_COST_FIELDS 2
#define W23_HOLDER_F15834_ROUTE_FIELDS 10

/* batch: [{"session_id": <any>, "kwargs": {...}}, ...] ->
 * [{"session_id": <any>, "kwargs": {...}}, ...] with adjacent incremental
 * deltas of one route merged (absolute=True never merges). */
json_t *w23_holder_f15834_coalesce(const json_t *batch);

/* state: {db_corrupt, reason, db_replaced, wal_generation_lost} ->
 * the quarantine advisory string, or null when the handle is clean. */
json_t *w23_holder_f15834_quarantine_reason(const json_t *state);

/* The write/read/accounting budget constants, as an object. */
json_t *w23_holder_f15834_constants(void);

#ifdef __cplusplus
}
#endif
#endif
