#ifndef NOC_W21_HOSTEDROOM_0F248D_H
#define NOC_W21_HOSTEDROOM_0F248D_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of tui_gateway/hosted_room_prepare_stall.py (gate w21_hostedroom_0f248d).
 *
 * `PrepareStallTracker` is a lock around a per-room `_Streak`; the streak's
 * transitions, `_describe`, `retry_action` and the one-event rule in
 * `report_stall` are pure.  What stays upstream: the store calls
 * (`hosted_rooms.room_state`, `read_events`, `append_event`), the logger and the
 * module's exception imports -- `report_stall`'s tail read is exposed here as
 * the `tail_event_id` a caller already fetched.
 */

#define W21_HOSTEDROOM_0F248D_STALL_ATTEMPTS 3
#define W21_HOSTEDROOM_0F248D_RETRY_TASK_PREFIX "prepare:"
#define W21_HOSTEDROOM_0F248D_EVENT_PREFIX "prepare-stalled:"
#define W21_HOSTEDROOM_0F248D_MAX_ERROR_CHARS 300

/* PoP: w21_hostedroom_0f248d_describe @ tui_gateway/hosted_room_prepare_stall.py:_describe
 * `str(exc).strip() or type(exc).__name__`, cut to 300 characters with a
 * trailing ellipsis.  `message` is str(exc), `exc_type` the class name used
 * when the message is blank.  Returns a malloc'd string. */
char *w21_hostedroom_0f248d_describe(const char *message, const char *exc_type);

/* PoP: w21_hostedroom_0f248d_feed @ tui_gateway/hosted_room_prepare_stall.py:PrepareStallTracker.failed
 * Replays a room's event stream through one `_Streak`.  `events` is an array of
 *   {"op":"failed"|"clear"|"stall", "kind":"permanent"|"transient"|"missing_table",
 *    "message":str, "type":str}
 * (`op` defaults to "failed").  `kind` says how the driver classified the
 * exception: transient contention / an ownership move, a `PermanentStoreError`
 * wrapping sqlite's "no such table", or a real failure that counts toward the
 * stall.  Answer: {"stalled":bool, "error":str|null, "failures":int,
 * "emitted":[bool,...]} -- `emitted` is what each `failed()` call returned, so
 * exactly one entry is true per stall (a `stall()` emits nothing). */
json_t *w21_hostedroom_0f248d_feed(const json_t *events);

/* PoP: w21_hostedroom_0f248d_retry_action @ tui_gateway/hosted_room_prepare_stall.py:retry_action
 * The stalled room's Retry row: {"kind":"retry","member_id":<", "-joined handles
 * or member_ids, else room_id>,"task_id":"prepare:<room_id>",
 * "execution_generation":1}. */
json_t *w21_hostedroom_0f248d_retry_action(const char *room_id, const json_t *room);

/* PoP: w21_hostedroom_0f248d_report_plan @ tui_gateway/hosted_room_prepare_stall.py:report_stall
 * The room event `report_stall` would append, or the fact that it appends
 * nothing because the last event in the log is already a stall notice.
 * `latest_seq` is the room's `latest_seq` as a decimal string; `tail_event_id`
 * is the `event_id` of the newest event (`read_events(since_seq=latest_seq-1,
 * limit=1)`), or NULL/absent when the room has none.
 * Answer: {"append":bool, "event_id":str, "kind":"member.unavailable",
 *          "payload":{"error":"Members could not be woken: <descr>",
 *                     "reason_code":"prepare_failed"}}. */
json_t *w21_hostedroom_0f248d_report_plan(const char *latest_seq, const char *tail_event_id,
                                          const char *message, const char *exc_type);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W21_HOSTEDROOM_0F248D_H */
