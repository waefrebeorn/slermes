/*
 * noc_w34_webhostter_a3f534_p2.h - C surface for hermes_cli/
 * web_host_terminal_sessions.py, part 2 (gate w34_webhostter_a3f534_p2):
 * payload building and the input pump's value decisions.
 *
 * Ported here: the metadata/closed frames a host terminal writes, the bytes a
 * queued frame would write and what it costs the backpressure bound, when the
 * input queue counts as full, how consecutive keystrokes coalesce around a
 * resize frame, the undo bookkeeping a resize returns, the admission decision
 * of ``_admit`` (before any spawn), and the attach-token shape.
 *
 * Payloads are compared byte for byte (``{"bytes": [0..255, ...]}``) because
 * they carry an embedded NUL: META_PREFIX starts with ``\0``.
 *
 * Seams (in the caller, never here): WebSocket send/close, the PTY write and
 * resize calls, the session's output counter, and the resize clock.
 */
#ifndef NOC_W34_WEBHOSTTER_A3F534_P2_H
#define NOC_W34_WEBHOSTTER_A3F534_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The input-queue bound and the per-frame charge. */
#define W34_WEBHOSTTER_A3F534_INPUT_QUEUE_FRAMES 64
#define W34_WEBHOSTTER_A3F534_INPUT_QUEUE_BYTES 1048576  /* 1024 * 1024 */
#define W34_WEBHOSTTER_A3F534_FRAME_OVERHEAD_BYTES 256

/* state: {token, shell, cwd, reconnected, truncated, ttl} -> {"bytes": [...]}
 * of META_PREFIX + compact json.dumps ({shell, terminalId, cwd, reconnected,
 * retentionSeconds, truncated}). */
json_t *w34_webhostter_a3f534_p2_metadata_payload(const json_t *state);

/* state: {token} -> {"bytes": [...]} of META_PREFIX + json.dumps
 * ({terminalId, closed: true}) and the close code _send_closed uses. */
json_t *w34_webhostter_a3f534_p2_closed_payload(const json_t *state);

/* state: {msg: {"text": str}|{"bytes": [0..255]}|null} -> {"bytes": [...]}:
 * a binary frame's bytes, else the UTF-8 of its text (or ""). */
json_t *w34_webhostter_a3f534_p2_frame_payload(const json_t *state);

/* state: {item: <any>} -> the item's charge: payload length + overhead, or
 * the overhead alone for the reader's non-dict last item. */
json_t *w34_webhostter_a3f534_p2_frame_cost(const json_t *state);

/* state: {frames, queued} -> _InputQueue.full(): both bounds reached. */
json_t *w34_webhostter_a3f534_p2_queue_full(const json_t *state);

/* state: {frames: [msg...]} -> [{"kind":"bytes","data":[..]} |
 * {"kind":"resize","cols":n,"rows":n}] with consecutive keystrokes joined. */
json_t *w34_webhostter_a3f534_p2_input_runs(const json_t *state);

/* state: {size: [c,r], before: [c,r]|null, outputs: n,
 *         undo: [[c,r], outputs]|null} -> {"waited_for": n|null,
 * "resize": [c,r], "undo": [[c,r], n]|null}. */
json_t *w34_webhostter_a3f534_p2_resize_undo(const json_t *state);

/* state: {attach, action, identity, sessions: [token]} -> {"result":
 * "denied"|"expired"|"none"|"created"|"resolved", "removed": [token],
 * "sent": [[bytes]], "codes": [n]}: what _admit does before any spawn. */
json_t *w34_webhostter_a3f534_p2_admit_verdict(const json_t *state);

/* state: {token} -> TOKEN_RE.fullmatch(token) is not None. */
json_t *w34_webhostter_a3f534_p2_token_ok(const json_t *state);

#ifdef __cplusplus
}
#endif
#endif
