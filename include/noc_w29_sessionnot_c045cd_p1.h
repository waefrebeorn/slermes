/*
 * noc_w29_sessionnot_c045cd_p1.h — C surface for upstream
 * tui_gateway/session_notifications.py (gate w29_sessionnot_c045cd_p1).
 *
 * Routing identity + dedupe + budget half of the per-session notification
 * poller: the key set a session answers to, the membership predicate, the
 * "must be claimed" test, the UI-emission dedupe tuple, the refusal backoff
 * arithmetic, the gateway-chat route test, the process-owner id set, and the
 * poller's tuned constants.
 *
 * Seams, all injected by the caller (never read here): the live session
 * registry and its lock, the compression-lineage DB lookup, `time.sleep`, and
 * `_session_lookup_key` (session_registry) — that helper is collapsed into the
 * same rule over the session record: agent.session_id or session_key or
 * fallback.
 */
#ifndef NOC_W29_SESSIONNOT_C045CD_P1_H
#define NOC_W29_SESSIONNOT_C045CD_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* (sid, session) -> the session's identity keys, sorted unique:
 * {"", session_key, _session_lookup_key(session, fallback=sid)} */
json_t *w29_sessionnot_c045cd_p1_current_keys(const json_t *sid, const json_t *session);

/* (session, keys[]) -> true when session_key or its lookup key is in keys. */
json_t *w29_sessionnot_c045cd_p1_session_matches(const json_t *session, const json_t *keys);

/* evt -> true when a turn may only run for a positively-claimed owner. */
json_t *w29_sessionnot_c045cd_p1_requires_owner(const json_t *evt);

/* evt -> the UI-emission identity tuple, as an array. */
json_t *w29_sessionnot_c045cd_p1_dedup_key(const json_t *evt);

/* session -> {"delay": <slept>, "next": <delay*2 capped at 5s>}. */
json_t *w29_sessionnot_c045cd_p1_refusal_backoff(const json_t *session);

/* state {route:{platform,chat_id}} -> true when the gateway owns the loop. */
json_t *w29_sessionnot_c045cd_p1_loop_route_is_gateway_chat(const json_t *state);

/* session {session_key, agent:{_process_owner_task_ids}} -> sorted id set
 * (the raw spawning ids close() kills by, plus the current key, minus ""). */
json_t *w29_sessionnot_c045cd_p1_session_process_owners(const json_t *session);

/* The poller's tuned constants (poll cadence, refusal budget, dedupe fields,
 * kanban terminal kinds) as one object. */
json_t *w29_sessionnot_c045cd_p1_constants(void);

#ifdef __cplusplus
}
#endif
#endif
