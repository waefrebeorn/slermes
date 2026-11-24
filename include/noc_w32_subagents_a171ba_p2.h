/*
 * noc_w32_subagents_a171ba_p2.h - C surface for the decision half of upstream
 * persistence/sqlite/authority/subagents.py (gate w32_subagents_a171ba_p2).
 *
 * Every function is a pure function of values: the note/cancel statements the
 * automatic-resume verbs run with their exact parameter tuples, the
 * supersession note taken from a parent's compression-chain tip, and the
 * predicates (owed, unstarted automatic claim, attempt count) those decisions
 * read.  The ledger connection, the chain walk and the clock are seams; the
 * caller supplies what the walk and the clock produced.
 */
#ifndef NOC_W32_SUBAGENTS_A171BA_P2_H
#define NOC_W32_SUBAGENTS_A171BA_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _note: {subagent_id, note, now, state|null, from_states[] (default
 * ["resumable"]), unpin, unstarted_claim, rowcount (default 1)} ->
 * {sql, params, ok}.  A cancel state (superseded/stopped) also clears the
 * debt; state/unpin move the row; unstarted_claim admits a claimed row that
 * an automatic continue holds. */
json_t *w32_subagents_a171ba_p2_note(const json_t *req);

/* _cancel_expired: {subagent_id, note, now, rowcount} -> {sql, params, ok}:
 * an owed expired row's debt is cancelled where the TTL already retired it. */
json_t *w32_subagents_a171ba_p2_cancel_expired(const json_t *req);

/* _cancel_manual_claim: {subagent_id, state, note, now, rowcount} ->
 * {sql, params, ok}: a claimed row the parent's own continue holds records the
 * cancel as auto_cancel {state, note} for its hand-back. */
json_t *w32_subagents_a171ba_p2_cancel_manual_claim(const json_t *req);

/* parent_superseded_note: {tip_end_reason: str|null, tip_exists: bool} ->
 * the advisory string, or null while the parent still wants the child.
 * (Upstream's tip end reason is a str or None; no other type is modelled.) */
json_t *w32_subagents_a171ba_p2_parent_note(const json_t *req);

/* SubagentRecord.owed, _unstarted_auto_claim and _attempts over one record:
 * {record:{state, document, ...}} -> {owed, unstarted_claim, attempts}. */
json_t *w32_subagents_a171ba_p2_predicates(const json_t *req);

/* hand_back_subagent's statement: {subagent_id, state, promised|null,
 * owner_pid|null, found:[auto_cancel.state, auto_cancel.note]|null} ->
 * null when no claimed row of this owner answered, else {sql, params}.
 * A cancel recorded during the claim wins over handing a promise back. */
json_t *w32_subagents_a171ba_p2_hand_back(const json_t *req);

/* settle_auto_refusal: {subagent_id, kind: cancelled|transient|permanent,
 * attempts, note, owner_pid} -> {sql, params}: which attempt/off fields the
 * refusal writes and the row's return from claimed. */
json_t *w32_subagents_a171ba_p2_refusal(const json_t *req);

/* OWED_SQL, CANCEL_DEBT, KEEP_WAITING_UNITS_SQL, _CANCEL_STATES and the
 * automatic-resume constants, as an object. */
json_t *w32_subagents_a171ba_p2_constants(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W32_SUBAGENTS_A171BA_P2_H */
