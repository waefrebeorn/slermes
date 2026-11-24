/*
 * noc_w28_hostedroom_e01c0e_p3.h - C surface for the envelope/tally/guard half
 * of upstream tui_gateway/hosted_room_service.py
 * (gate w28_hostedroom_e01c0e_p3).
 *
 * The bytes a room writes and the decisions it refuses: the room.activity and
 * room.continued event envelopes (ids included), the Stop tally over a task
 * snapshot, which attempt an explicit Retry lands on, and one cycle of the owed
 * grant revokes with its backoff. Store writes, peer calls and the clock are
 * fixtures here.
 */
#ifndef NOC_W28_HOSTEDROOM_E01C0E_P3_H
#define NOC_W28_HOSTEDROOM_E01C0E_P3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _append_room_status(room, decision): append_event kwargs, or null when the
 * decision names no discussion event. */
json_t *hrs_activity_event(const json_t *room, const json_t *decision);

/* _append_continue(...): {"outcome": not_paused|exhausted|append, "event": {...}} */
json_t *hrs_continue_event(const char *room_id, const char *discussion_event_id, const char *gateway_id,
                           double epoch, const json_t *paused, double max_continues,
                           const json_t *event_seqs);

/* _stop_room(...): cancels issued, pending/unconfirmed counts, the returned count */
json_t *hrs_stop_room_tally(const json_t *fixture);

/* retry_room_task(...): retry | prepared | error | invalid, with the fence it names */
json_t *hrs_retry_room_task(const json_t *fixture);

/* retry_owed_revokes(): settled count, settle rows (retry_at included) and errors */
json_t *hrs_owed_revokes(const json_t *fixture);

#ifdef __cplusplus
}
#endif
#endif
