/*
 * noc_w32_subagents_a171ba_p1.h - C surface for the row-shaping and
 * write-statement half of upstream
 * persistence/sqlite/authority/subagents.py (gate w32_subagents_a171ba_p1).
 *
 * The ledger connections are seams: every function here is a pure function of
 * the values a caller already has (a fetched row, a write record, a limits
 * pair), and answers either the shaped value or the exact statement the verb
 * would run with its exact parameter tuple.  ``now`` is passed in (the
 * upstream ``_at`` clock is a seam).
 */
#ifndef NOC_W32_SUBAGENTS_A171BA_P1_H
#define NOC_W32_SUBAGENTS_A171BA_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _record(row): [subagent_id, state, pinned, stop_cause, owner_pid,
 * owner_started_at, record_json, created_at, updated_at, expires_at] ->
 * {subagent_id, state, pinned, stop_cause, owner_pid, owner_started_at,
 *  document, created_at, updated_at, expires_at} (document decoded). */
json_t *w32_subagents_a171ba_p1_record_shape(const json_t *row);

/* get_delegation_route's shaping of a fetched async_delegations row:
 * {delegation_id, row:[origin_session, origin_ui_session_id,
 *  parent_session_id, origin_session_id, task_json, result_json,
 *  dispatched_at, updated_at]} -> the DelegationRoute fields as an object. */
json_t *w32_subagents_a171ba_p1_route_shape(const json_t *req);

/* _upsert: {subagent_id, write:{state,pinned,stop_cause,owner_pid,
 * owner_started_at,document,expires_at}, now} -> {sql, params}; params[6] is
 * the document without _RUN_ONLY (state, stop_cause, auto_claim, auto_cancel). */
json_t *w32_subagents_a171ba_p1_upsert(const json_t *req);

/* _apply_limits: {limits:{ttl_hours,max_resumable}, now} ->
 * {stmts:[{sql,params} x3]}: TTL retire, retention delete
 * (now - 604800 - ttl_hours*3600), pin cap max(max_resumable, 0). */
json_t *w32_subagents_a171ba_p1_apply_limits(const json_t *req);

/* _insert_completion: {completion:{unit_id,unit_updated_at,event,result,
 * owner_pid,owner_started_at}, now} -> {sql, params}; params[7], params[8] and
 * params[11] are the serialized event, result and task objects. */
json_t *w32_subagents_a171ba_p1_insert_completion(const json_t *req);

/* recover_subagents' guard: {row:[state,owner_pid,owner_started_at]|null,
 * recovery:{owner_pid,owner_started_at}} -> bool: the record is written only
 * while the row is still running/claimed and held by the same dead owner. */
json_t *w32_subagents_a171ba_p1_recover_guard(const json_t *req);

/* _COLUMNS, _RUN_ONLY and SUBAGENT_EXPIRED_RETENTION_SECONDS as an object. */
json_t *w32_subagents_a171ba_p1_constants(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W32_SUBAGENTS_A171BA_P1_H */
