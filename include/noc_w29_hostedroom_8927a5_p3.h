/*
 * noc_w29_hostedroom_8927a5_p3.h - C surface for upstream
 * persistence/sqlite/gateway/hosted_room_driver.py (gate w29_hostedroom_8927a5_p3).
 *
 * The task state machine as a decision: the caller's task view of a stored
 * row, the settlement and cancellation replays, the deferral payload, and the
 * verbs admit / start / cancel / settle / the lease-first generation family /
 * prune / mark-indeterminate as outcome plus bind values.
 *
 * The verbs take one request object each so the harness stays a dispatcher:
 * every field named below is the value the caller already validated.
 */
#ifndef NOC_W29_HOSTEDROOM_8927A5_P3_H
#define NOC_W29_HOSTEDROOM_8927A5_P3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The task view of one stored row: identity, payload, the columns from
 * payload_digest on (result_json exposed as "result"), idempotent. */
json_t *w29_hostedroom_8927a5_p3_task_view(const json_t *row, int idempotent);

/* The _SETTLE_SET bind values: [status, settlement_id, status, result_json,
 * terminal_at, updated_at]. */
json_t *w29_hostedroom_8927a5_p3_settlement_params(const char *settlement_id, const char *status,
                                                   const json_t *result, double now);

/* {outcome: none|idempotent|conflict, task?, error?, message?} for one
 * already-committed settlement. */
json_t *w29_hostedroom_8927a5_p3_settlement_replay(const json_t *row, const char *settlement_id, const char *status,
                                                   const json_t *result, double now);

/* {outcome: none|idempotent, task?}: the same cancel_id already committed in
 * <status>. */
json_t *w29_hostedroom_8927a5_p3_cancel_replay(const json_t *row, const char *cancel_id, const char *status);

/* compact_json({"reason": reason, "retryable": true}) */
json_t *w29_hostedroom_8927a5_p3_defer_result(const char *reason);

/* admit_task: {outcome: insert|replay|error, params?, task?, error?, message?}
 * from {room, no_room_table, identity, payload_json, payload_digest,
 * source_event_seq, existing, thread_row, now}.  The payload normalization and
 * digest are the caller's seam. */
json_t *w29_hostedroom_8927a5_p3_admit(const json_t *req);

/* start_task: {outcome: started|error, execution_generation, params} from
 * {room, lease_row, lease, identity, task_row, expected_cancel_generation,
 * has_blocker, next_task_id, now, rowcount}. */
json_t *w29_hostedroom_8927a5_p3_start(const json_t *req);

/* cancel_task: {outcome: cancelled|replayed|error, params?, task?} from
 * {identity, task_row, cancel_id, expected_cancel_generation, now, rowcount}. */
json_t *w29_hostedroom_8927a5_p3_cancel(const json_t *req);

/* settle_task: {outcome: settled|replayed|error, params?, task?} from
 * {room, lease_row, lease, attempt{identity, execution_generation,
 * cancel_generation}, task_row, settlement_id, status, result, now,
 * allow_expired, rowcount}. */
json_t *w29_hostedroom_8927a5_p3_settle(const json_t *req);

/* The lease-first generation family (resolve, resolve_cancel, requeue,
 * defer, defer_stopping, reopen_deferred, requeue_deferred):
 * {outcome: applied|replayed|error, params, task?} from
 * {name, room, lease_row, lease, identity, task_row, execution_generation,
 * cancel_generation, settlement_id, status, result, cancel_id, reason, now,
 * rowcount}. */
json_t *w29_hostedroom_8927a5_p3_generation(const json_t *req);

/* prune_published_terminal_tasks: {count, ids} from
 * {rows[{task_id, terminal_at}], now, retention_seconds, retain,
 * has_publications}.  One batch at most. */
json_t *w29_hostedroom_8927a5_p3_prune(const json_t *req);

/* mark_attempt_indeterminate: {outcome: marked|skipped|error, params} */
json_t *w29_hostedroom_8927a5_p3_mark_indeterminate(const json_t *req);

/* settle_stopping_task: {outcome: settled|replayed|error, params?, task?} from
 * {room, lease_row, lease, identity, task_row, expected_execution_generation,
 * expected_cancel_generation, settlement_id, status, result, now, rowcount}. */
json_t *w29_hostedroom_8927a5_p3_settle_stopping(const json_t *req);

/* begin_task_cancel: {outcome: stopping|replayed|error, params?, task?} from
 * {identity, task_row, cancel_id, expected_cancel_generation, now, rowcount}. */
json_t *w29_hostedroom_8927a5_p3_begin_cancel(const json_t *req);

/* complete_task_cancel: {outcome: cancelled|replayed|error, params?, task?} */
json_t *w29_hostedroom_8927a5_p3_complete_cancel(const json_t *req);

/* requeue_not_admitted_task: {outcome: requeued|replayed|error, params?, task?}
 * from {room, lease_row, lease, attempt, task_row, now, rowcount}. */
json_t *w29_hostedroom_8927a5_p3_requeue_not_admitted(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
