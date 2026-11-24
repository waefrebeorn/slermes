/*
 * noc_w24_write_5b6032.h - C surface for upstream
 * persistence/sqlite/_state/write.py (gate w24_write_5b6032).
 *
 * The value half of the state.db write path: how a code location is named
 * ("module:qualname"), how a recorded write site renders ("caller -> callback"),
 * which post-commit maintenance is due after the N-th committed write, and the
 * contention constants those decisions are tuned with.  Frames, code objects,
 * locks and logger handlers all stay in the caller: a code object arrives here
 * as the value {"filename", "name", "qualname"}.
 */
#ifndef W24_WRITE_5B6032_H
#define W24_WRITE_5B6032_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The patience for one writer-owner WARNING line, per database per window. */
#define W24_WRITE_5B6032_OWNER_LOG_INTERVAL_S 30.0

/* code: {"co_filename": <str>, "co_name": <str>, "co_qualname": <str>} (any
 * key may be absent) -> "module:qualname", where module is the last
 * '/'-separated component of the filename with a trailing ".py" removed, and
 * qualname falls back to name and then "?" -- exactly the f-string upstream
 * builds. */
json_t *w24_write_5b6032_describe_code(const json_t *code);

/* site: null | [caller_code|null, callback_code|null] -> "caller -> callback",
 * or "?" when neither names a code object.  Anything that is not a 2-element
 * sequence is the unpack error upstream would raise, as {"error": "..."}. */
json_t *w24_write_5b6032_describe_site(const json_t *site);

/* req: {"write_count": N, "checkpoint_every": C, "fts_every": F} -> the
 * maintenance due after the N-th committed write:
 * {"checkpoint": bool, "fts_merge": bool}.  A zero period is Python's
 * ZeroDivisionError, as {"error": "division by zero"}. */
json_t *w24_write_5b6032_maintenance(const json_t *req);

/* The owner-log interval, the wrapper names, and the writer-mutex refusal. */
json_t *w24_write_5b6032_constants(void);

#ifdef __cplusplus
}
#endif
#endif
