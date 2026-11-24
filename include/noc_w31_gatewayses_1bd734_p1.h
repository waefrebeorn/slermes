/*
 * noc_w31_gatewayses_1bd734_p1.h - C surface for the identity/listing half of
 * upstream hermes_cli/gateway_session_store.py (gate w31_gatewayses_1bd734_p1).
 *
 * What a durable id labels itself as, how a LIMIT/OFFSET window and a transcript
 * view are normalised, whether the store still answers, which family a missing
 * attribute falls into, and what an owner-restart wait froze.  Every seam (the
 * gateway RPC, the read-only SQLite reader) stays in the caller.
 */
#ifndef NOC_W31_GATEWAYSES_1BD734_P1_H
#define NOC_W31_GATEWAYSES_1BD734_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* session_label(session_id): the 8-char label of a canonical ``local-`` + 64
 * lowercase-hex id, else the id as given (null stays null). */
json_t *w31_gatewayses_1bd734_p1_session_label(const json_t *session_id);

/* _window(limit, offset) -> {"limit", "offset"} or {"error": <SessionWindow's
 * ValueError text>}.  A negative limit is no limit (null). */
json_t *w31_gatewayses_1bd734_p1_window(const json_t *limit, const json_t *offset);

/* _transcript_view(include_inactive, include_compacted) -> "ALL"|"DISPLAY"|"ACTIVE". */
json_t *w31_gatewayses_1bd734_p1_transcript_view(const json_t *include_inactive,
                                                 const json_t *include_compacted);

/* __bool__: true while the connection is not final. */
json_t *w31_gatewayses_1bd734_p1_store_truthy(const json_t *final);

/* __getattr__(name) -> one of "attribute_error" (a dunder), "sanitize_title",
 * "session_yolo_enabled", "read", "noop", "zero", "refused". */
json_t *w31_gatewayses_1bd734_p1_attr_dispatch(const json_t *name);

/* _retry_state(retry) -> {"prepared": <frozen request or null>,
 * "abandon": <bool, the Ctrl+C poll is present>}.  ``retry`` is
 * [prepared, abandon_set] or null. */
json_t *w31_gatewayses_1bd734_p1_retry_state(const json_t *retry);

#ifdef __cplusplus
}
#endif
#endif
