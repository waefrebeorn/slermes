/*
 * noc_w29_hostedroom_8927a5_p2.h - C surface for upstream
 * persistence/sqlite/gateway/hosted_room_driver.py (gate w29_hostedroom_8927a5_p2).
 *
 * The fenced lease lifecycle and the row predicates it is decided by: the run
 * fence a lease names, a lease built from its row, the row/lease match, the
 * identity match, the room-authority gate, the active-lease gate, the
 * generation predicates, and the acquire / renew / release decisions with the
 * exact bind values the fenced statements take.
 */
#ifndef NOC_W29_HOSTEDROOM_8927A5_P2_H
#define NOC_W29_HOSTEDROOM_8927A5_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* [gateway_id, process_generation, lease_generation] */
json_t *w29_hostedroom_8927a5_p2_run_fence(const json_t *lease);

/* {room_id, gateway_id, authority_epoch, process_generation,
 *  lease_generation, expires_at, reclaimed} */
json_t *w29_hostedroom_8927a5_p2_lease_from_row(const json_t *row, int reclaimed);

/* The row holds exactly this lease (gateway, epoch, process generation,
 * lease generation). */
int w29_hostedroom_8927a5_p2_lease_row_matches(const json_t *row, const json_t *lease);

/* {room_id, task_id, thread_id, turn_id} */
json_t *w29_hostedroom_8927a5_p2_task_identity(const json_t *row);

/* {"error","message"} for a missing (when required) or differently bound
 * task row, else null. */
json_t *w29_hostedroom_8927a5_p2_load_task(const json_t *row, const json_t *identity, int required);

/* {"error","message"} when the room row is absent or disbanded, else null. */
json_t *w29_hostedroom_8927a5_p2_active_room(const json_t *room, int no_table);

/* {"error","message"} when the room's authority moved, else null. */
json_t *w29_hostedroom_8927a5_p2_room_authority(const json_t *room, const char *gateway_id, long long epoch);

/* {"error","message"} for a changed authority or a stale, released or (unless
 * allow_expired) expired lease, else null. */
json_t *w29_hostedroom_8927a5_p2_lease_gate(const json_t *room, const json_t *lease_row, const json_t *lease,
                                            double now, int allow_expired);

/* The row is at exactly this status and both generations. */
int w29_hostedroom_8927a5_p2_generations_match(const json_t *row, const char *status, long long execution_generation,
                                               long long cancel_generation);

/* {"error","message"} when the cancellation generation moved, else null. */
json_t *w29_hostedroom_8927a5_p2_cancel_generation_guard(const json_t *row, long long expected);

/* acquire_lease as a decision: {outcome: insert|renew|held|reclaim|error,
 * error, message, params} from {room, lease_row, room_id, gateway_id,
 * authority_epoch, process_generation, ttl_seconds, now}. */
json_t *w29_hostedroom_8927a5_p2_acquire(const json_t *req);

/* renew_lease: {outcome: renewed|error, error, message, params} from
 * {room, lease_row, lease, ttl_seconds, now}; the expiry never moves back. */
json_t *w29_hostedroom_8927a5_p2_renew(const json_t *req);

/* release_lease: {outcome: released|idempotent|error, error, message, params}
 * from {room, lease_row, lease, has_running, now}. */
json_t *w29_hostedroom_8927a5_p2_release(const json_t *req);

/* recover_room: {outcome: recovered|error, update, params, stale_ids, queued,
 * indeterminate} from {room, lease_row, lease, stale_rows, queued_rows,
 * indeterminate_rows, now}; only foreign running tasks move to indeterminate. */
json_t *w29_hostedroom_8927a5_p2_recover(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
