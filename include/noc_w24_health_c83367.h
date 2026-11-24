/*
 * noc_w24_health_c83367.h - C surface for upstream
 * persistence/sqlite/_state/health.py (gate w24_health_c83367).
 *
 * The profile-level "is this state.db damaged" latch, as values: a list of
 * operations against the process-wide table (mark / state / reason / reset /
 * reset-all) and the operator text the first latch logs.  The path key is
 * opaque here -- ``_key`` (expanduser + resolve) is the caller's seam, exactly
 * as the store holder already owns the real path -- so the same string is the
 * same entry.  No file, no sqlite handle, no logger is touched.
 */
#ifndef W24_HEALTH_C83367_H
#define W24_HEALTH_C83367_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The two served states. */
#define W24_HEALTH_C83367_STORAGE_OK "ok"
#define W24_HEALTH_C83367_STORAGE_CORRUPT "corrupt"

/* ops: [{"op":"mark","path":P,"reason":R} | {"op":"state","path":P} |
 *        {"op":"reason","path":P} | {"op":"reset","path":P} |
 *        {"op":"reset_all"}, ...] -> one result per op (null for the void
 * ops, the state/reason for the reads, "UNKNOWN_OP" for anything else).
 * The table starts empty: a case is the whole history of its paths. */
json_t *w24_health_c83367_latch(const json_t *ops);

/* req: {"path": P, "reason": R} -> the operator sentence logged when P is
 * first latched (str() of the reason, P named twice). */
json_t *w24_health_c83367_log_message(const json_t *req);

/* The served states, plus the module's export table. */
json_t *w24_health_c83367_constants(void);

#ifdef __cplusplus
}
#endif
#endif
