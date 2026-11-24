/*
 * noc_w32_hostedroom_be7d8d_p1.h - C surface for upstream
 * persistence/sqlite/gateway/hosted_room_policy.py (gate w32_hostedroom_be7d8d_p1).
 *
 * The projection's bounded decisions: the active-event budget a caller may
 * raise (never lower), the file replay capacity that outlives a restart with a
 * smaller budget, the transcript page bound, the oldest-pending-thread pick,
 * the derived-cache rebuild verdict, the page-advance verdict of one sync step,
 * the canonical user sources a retained settlement still needs, and the two
 * event ids a published task receipt names.  Every connection, file handle,
 * clock and lock stays in the caller.
 */
#ifndef NOC_W32_HOSTEDROOM_BE7D8D_P1_H
#define NOC_W32_HOSTEDROOM_BE7D8D_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The projection bounds, as the truth module declares them. */
#define W32_HOSTEDROOM_BE7D8D_MAX_ACTIVE_POLICY_EVENTS 64
#define W32_HOSTEDROOM_BE7D8D_MAX_ACTIVE_RETRY_RECEIPTS 64
#define W32_HOSTEDROOM_BE7D8D_MAX_THREAD_TRANSCRIPT_EVENTS 24
#define W32_HOSTEDROOM_BE7D8D_TRANSCRIPT_SCHEMA_VERSION 5
/* min(2**63 - 2, ...): the largest value SQLite's LIMIT accepts as an integer. */
#define W32_HOSTEDROOM_BE7D8D_SQLITE_INT_MAX 9223372036854775806.0

/* value + its Python type ("int", "float", "bool", "str", "null") ->
 * {"ok", "error", "budget"}: the validated, clamped active-event budget. */
json_t *w32_hostedroom_be7d8d_p1_budget(const json_t *value, const char *value_type);

/* cached (_PREPARED entry) + stored (file capacity) + budget ->
 * {"reinitialize", "capacity"}. */
json_t *w32_hostedroom_be7d8d_p1_capacity(const json_t *cached, const json_t *stored, double budget);

/* max_active_events -> min(2**63-2, 24 * (max + 64 + 2)). */
json_t *w32_hostedroom_be7d8d_p1_transcript_bound(double max_active_events);

/* {stopped_through_seq, threads:[{thread_id, discussion_event_id, completed,
 * latest_user_seq}]} -> {"thread_id", "discussion_event_id"} or null. */
json_t *w32_hostedroom_be7d8d_p1_snapshot_thread(const json_t *req);

/* {has_state, schema_version} -> {"rebuild", "write_version", "tables", "cursor_reset"}. */
json_t *w32_hostedroom_be7d8d_p1_replay_state(const json_t *req);

/* {cursor, latest_seq, rows, next_cursor, through_seq} ->
 * {"action", "cursor", "error"}. */
json_t *w32_hostedroom_be7d8d_p1_sync_step(const json_t *req);

/* [{seq, event_id, kind, payload}, ...] -> the sorted source event ids. */
json_t *w32_hostedroom_be7d8d_p1_settled_sources(const json_t *events);

/* task_id -> {"digest", "event_ids"}. */
json_t *w32_hostedroom_be7d8d_p1_task_event_ids(const json_t *task_id);

#ifdef __cplusplus
}
#endif
#endif
