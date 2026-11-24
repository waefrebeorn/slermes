/*
 * noc_w25_sessionman_1c48ef.h - C surface for upstream
 * gateway/session_managed_worker_log.py (gate w25_sessionman_1c48ef).
 *
 * The per-profile sink for a managed worker's stderr/stdout.  The pipe drain
 * thread, the per-file lock, ``os.open``/``os.write`` and the redactor
 * (``agent.redact.redact_sensitive_text``) are seams.  What is ported is what
 * the values decide: where the sink lives, how the read loop frames lines (a
 * line longer than the read cap is dropped to its last fragment and the next
 * line resumes only after a newline), how a line is truncated, and the
 * rotation decision that keeps one rotated copy under the byte cap.
 */
#ifndef NOC_W25_SESSIONMAN_1C48EF_H
#define NOC_W25_SESSIONMAN_1C48EF_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* profile_id -> "<home>\\logs\\managed-worker.log" when the authority's home is
 * an absolute Windows path (drive + root, the pathlib rule this Python uses),
 * otherwise null (test doubles have no home path, so they get no sink). */
json_t *w25_sessionman_1c48ef_worker_log_path(const json_t *profile_id);

/* plan: {
 *   chunks:    ["<readline() result>", ...]   # "" ends the stream
 *   pid:       <int>,
 *   fail_from: <int> | null                   # _append raises from this call on
 * } -> {"appended": ["[pid N] ...", ...], "warned": bool}  (redaction is the
 * seam: the piece under test is framing, truncation and the drain-never-stops
 * rule).  A non-string chunk maps to the empty read that ends the stream. */
json_t *w25_sessionman_1c48ef_frame_lines(const json_t *plan);

/* plan: {size: <existing bytes>, nbytes: <bytes to append>} ->
 * {"rotate": bool, "write": <bytes actually written>} for the 0600 append. */
json_t *w25_sessionman_1c48ef_append_plan(const json_t *plan);

/* Sink literals: name, caps and the 0600 mode. */
json_t *w25_sessionman_1c48ef_constants(void);

#ifdef __cplusplus
}
#endif
#endif
