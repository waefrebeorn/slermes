/*
 * noc_w28_hostedroom_e01c0e_p1.h - C surface for the task/attempt identity
 * half of upstream tui_gateway/hosted_room_service.py
 * (gate w28_hostedroom_e01c0e_p1).
 *
 * What a hosted Discussion room names when it talks about work: which member a
 * task is for, what a pending action points at, which attempts are approvable,
 * which rows offer Retry, and whether a seated member lives on another gateway.
 * The store, the peer transports and the worker runtime stay in the caller.
 */
#ifndef NOC_W28_HOSTEDROOM_E01C0E_P1_H
#define NOC_W28_HOSTEDROOM_E01C0E_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* task_member_id(task): str(payload.target_member_id or payload.target_profile) */
json_t *hrs_task_member_id(const json_t *task);

/* _member_of(task): str(payload.get("target_member_id") or payload.get("target_profile") or "") */
json_t *hrs_member_of(const json_t *task);

/* attempt_action(kind, task): {"kind","member_id","task_id","execution_generation"} */
json_t *hrs_attempt_action(const char *kind, const json_t *task);

/* _approvable_attempts(tasks): sorted, deduped [member_id, task_id, generation] */
json_t *hrs_approvable_attempts(const json_t *tasks);

/* _attempt_actions(room_id, tasks_shown): Retry rows for retryable tasks, in order */
json_t *hrs_attempt_actions(const json_t *tasks);

/* _member_is_peer(room_id, member_id) over an already-read room state */
json_t *hrs_member_is_peer(const json_t *room, const char *member_id);

#ifdef __cplusplus
}
#endif
#endif
