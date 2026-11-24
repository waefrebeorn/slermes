/*
 * noc_w23_coordinati_5a8f30.h - C surface for upstream
 * persistence/sqlite/authority/coordination.py (gate w23_coordinati_5a8f30).
 *
 * The SQLite coordination repository: the compression lock / turn lease
 * claim, the delegation ledger's delivery decisions and the finished-work
 * query.  SQLite, the store holder and the clock are seams -- the caller
 * hands over the rows the connection returned and the fixtures that answer
 * them; this file owns the decision over those rows and the statements it
 * would issue.
 *
 * Every function returns JSON; list-valued outputs carry "sql" (each
 * statement with its parameters) next to the decision, because the statement
 * is part of this module's contract with the store.
 */
#ifndef NOC_W23_COORDINATI_5A8F30_H
#define NOC_W23_COORDINATI_5A8F30_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* persistence.authority.coordination's ledger constants. */
#define W23_COORDINATI_5A8F30_REPLAY_MAX_AGE 172800.0
#define W23_COORDINATI_5A8F30_ORPHAN_STALE 60.0
#define W23_COORDINATI_5A8F30_CLAIM_STALE 300.0
#define W23_COORDINATI_5A8F30_RETENTION 604800.0
#define W23_COORDINATI_5A8F30_FAILURE_WINDOW 86400.0
#define W23_COORDINATI_5A8F30_TURN_LEASE_SWEEP_GRACE 86400.0
#define W23_COORDINATI_5A8F30_MAX_DELIVERY_ATTEMPTS 8
#define W23_COORDINATI_5A8F30_MAX_RETAINED_TERMINAL 50
#define W23_COORDINATI_5A8F30_MAX_DURABLE_PENDING 1000

/* {"replay_max_age","orphan_stale","claim_stale","retention","failure_window",
 *  "turn_lease_sweep_grace","max_delivery_attempts","max_retained_terminal",
 *  "max_durable_pending","completion_deliveries"} */
json_t *w23_coordinati_5a8f30_consts(void);

/* _past_replay_age(now, completed_at, dispatched_at): the age basis is
 * `completed_at or dispatched_at`, and only a basis older than the replay cap
 * is past it. */
json_t *w23_coordinati_5a8f30_past_replay_age(const json_t *now,
                                              const json_t *completed_at,
                                              const json_t *dispatched_at);

/* _json_object(raw): a JSON object column as a dict; {} when empty,
 * undecodable or not an object. */
json_t *w23_coordinati_5a8f30_json_object(const char *raw);

/* _claim_lease_row over scripted facts:
 * req{table, key_col, key, holder, now, expires_at, mode:"lt"|"le",
 *     existing:{holder, expires_at}|null, dead:[holders]}
 * -> {acquired, reclaimed, sql} */
json_t *w23_coordinati_5a8f30_claim_lease(const json_t *req);

/* _replay_pending: req{now, rows:[[id, event_json, completed_at,
 * dispatched_at], ...]} -> {offered, dropped, sql} */
json_t *w23_coordinati_5a8f30_replay_pending(const json_t *req);

/* get_admitted_completion: req{delegation_id, now, row:[event_json,
 * completed_at, dispatched_at, delivery_attempts]|null} */
json_t *w23_coordinati_5a8f30_admitted_completion(const json_t *req);

/* sweep_orphaned_completions: req{now, exclude_ids, lost_ids,
 * dead_owners:[[pid, started_at], ...], rows:[[id, event_json, completed_at,
 * dispatched_at, owner_pid, owner_started_at, delivery_attempts], ...]} */
json_t *w23_coordinati_5a8f30_sweep(const json_t *req);

/* list_finished_delegations: req{origin_ui_session_id, parent_session_id,
 * limit, now, rows:[[delegation_id, state, dispatched_at, completed_at,
 * task_json, result_json], ...]} */
json_t *w23_coordinati_5a8f30_finished(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
