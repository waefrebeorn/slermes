/*
 * noc_w28_hostedroom_3fa737_p2.h - C surface for the decision layer of
 * upstream Hermes tui_gateway/hosted_room_driver.py (gate
 * w28_hostedroom_3fa737_p2).
 *
 * Whether a session turn is active, whether it is this exact attempt's, when a
 * Stop counts as acknowledged, when a Discard may resolve a lost attempt, and
 * which recovery route a Retry takes.  The transport/store observations are
 * inputs; the routing and the guards are the code.
 */
#ifndef NOC_W28_HOSTEDROOM_3FA737_P2_H
#define NOC_W28_HOSTEDROOM_3FA737_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _info_active: bool(info.get("active", info.get("running", False))). */
json_t *w28_hostedroom_3fa737_p2_info_active(const json_t *info);

/* _info_is_active_for: active and info.task_id admitted (None only when not exact). */
json_t *w28_hostedroom_3fa737_p2_info_is_active_for(const json_t *info, const char *task_id,
                                                    int require_exact);

/* _peer_stop_acknowledged: an inactive exact session reporting cancelled/interrupted
 * for this task id and generation (a probe that found no session is not an ack). */
json_t *w28_hostedroom_3fa737_p2_peer_stop_acknowledged(const json_t *info, const json_t *task,
                                                       int has_session);

/* _stop_outcome's interrupt tail: result.interrupted is True or status in
 * {cancelled, interrupted}. */
json_t *w28_hostedroom_3fa737_p2_interrupt_ack(const json_t *result);

/* _discard_unknown_stop's guard: inactive info proving this exact task/generation. */
json_t *w28_hostedroom_3fa737_p2_discard_guard(const json_t *info, const json_t *task);

/* _discard_unknown_stop's result validation:
 * {"usable": bool, "outcome": "receipt"|"cancelled"|"none", "receipt": obj|null}. */
json_t *w28_hostedroom_3fa737_p2_discard_outcome(const json_t *result, const json_t *task);

/* _stop_outcome router.
 * spec: {info, task, local, has_session, has_discard, discard_result, interrupt_result}
 * -> {"outcome": "ack"|"nack"|"receipt", "receipt": obj|null}. */
json_t *w28_hostedroom_3fa737_p2_stop_outcome(const json_t *spec);

/* _past_execution_deadline: started_at is set and now is past started_at + limit. */
json_t *w28_hostedroom_3fa737_p2_past_execution_deadline(const json_t *task, double now,
                                                         double timeout);

/* _defer_unconfirmed_stop's guard: served elsewhere and past the deferral window. */
json_t *w28_hostedroom_3fa737_p2_defer_unconfirmed_guard(const json_t *task, double now,
                                                         double defer_seconds,
                                                         int served_elsewhere);

/* _expire_attempt_deadline's cancel id: "deadline:<execution_generation>". */
json_t *w28_hostedroom_3fa737_p2_deadline_id(const json_t *task);

/* _reconcile_indeterminate's deferral deadline: defer + (indeterminate_at or
 * updated_at or created_at or now). */
json_t *w28_hostedroom_3fa737_p2_reconcile_deadline(const json_t *task, double now,
                                                    double defer_seconds);

/* retry_indeterminate's routing: "stale"|"invalid"|"unavailable"|task status. */
json_t *w28_hostedroom_3fa737_p2_retry_route(const json_t *task,
                                             const json_t *requested_generation, int has_binding);

#ifdef __cplusplus
}
#endif
#endif
