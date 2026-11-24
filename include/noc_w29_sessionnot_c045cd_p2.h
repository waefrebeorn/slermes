/*
 * noc_w29_sessionnot_c045cd_p2.h — C surface for upstream
 * tui_gateway/session_notifications.py (gate w29_sessionnot_c045cd_p2).
 *
 * Text half of the notification poller: the kanban event line formatter (the
 * same wording the gateway mirrors onto messaging chats), the completion
 * /timeout suffixes, the first-line clip helper, the async-delegation display
 * metadata, and the per-turn note prefix.
 *
 * Seams, injected by the caller: task/event objects arrive as records (title,
 * assignee, result, kind, payload), the cross-module display text is a
 * parameter, and the resolution of a kanban DB path stays in the caller.
 */
#ifndef NOC_W29_SESSIONNOT_C045CD_P2_H
#define NOC_W29_SESSIONNOT_C045CD_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* (value, limit) -> "\n" + first stripped line clipped to limit, or "". */
json_t *w29_sessionnot_c045cd_p2_kb_first_line(const json_t *value, const json_t *limit);

/* (task, payload, title) -> " done — <title><handoff>". */
json_t *w29_sessionnot_c045cd_p2_kb_completed(const json_t *task, const json_t *payload,
                                              const json_t *title);

/* (task, payload, title) -> " timed out (max_runtime=<int>s); will retry". */
json_t *w29_sessionnot_c045cd_p2_kb_timed_out(const json_t *task, const json_t *payload,
                                              const json_t *title);

/* (sub, task, ev, board_slug) -> one kanban notification line, or null for a
 * silent kind (archived/unblocked/unknown). */
json_t *w29_sessionnot_c045cd_p2_format_kanban_event_text(const json_t *sub, const json_t *task,
                                                          const json_t *ev,
                                                          const json_t *board_slug);

/* (evt, display_text) -> display-only metadata object for a delegation
 * completion: display_text, delegation_id, task/completed/failed counts and
 * an optional duration_seconds. */
json_t *w29_sessionnot_c045cd_p2_async_delegation_display_metadata(const json_t *evt,
                                                                   const json_t *display_text);

/* (run_message, note) -> note prefixed onto the MODEL INPUT (string), or a
 * leading {"type":"text"} part when run_message is a content-part list. */
json_t *w29_sessionnot_c045cd_p2_prepend_note(const json_t *run_message, const json_t *note);

#ifdef __cplusplus
}
#endif
#endif
