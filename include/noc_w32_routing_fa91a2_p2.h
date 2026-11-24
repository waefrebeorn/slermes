/*
 * noc_w32_routing_fa91a2_p2.h - C surface for the routing index reads, the
 * orphan/adoption decisions and the topic-binding guard, upstream
 * persistence/sqlite/gateway/routing.py (gate w32_routing_fa91a2_p2).
 *
 * Ported: which gateway_routing rows name a session (the entry_json payload is
 * parsed here, not by SQLite), the single entry a session resolves to, the
 * session_for_origin tie-break over the rows a query returned, the
 * orphaned_sessions report (lineage vs contiguity evidence, the ambiguous
 * donor contest), the adopt_orphan gate, the bind_topic guard
 * (expected_session_id fence, TopicAlreadyLinked, same-profile moves), the
 * replace_routing_entries rejected-key set and the prune_never_active outcome.
 *
 * Every row list, owner and clock is a fixture value: the holder is a seam.
 */
#ifndef NOC_W32_ROUTING_FA91A2_P2_H
#define NOC_W32_ROUTING_FA91A2_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* in: {rows: [{scope, session_key, entry_json}], session_ids: [...]} ->
 * [[scope, session_key, session_id], ...]; malformed entry_json is skipped. */
json_t *w32_routing_fa91a2_routing_entries_for_sessions(const json_t *in);

/* in: {rows: [...], session_id} -> the first entry_json dict naming that
 * session, or null. */
json_t *w32_routing_fa91a2_routing_entry_for_session(const json_t *in);

/* in: {rows: [{id, user_id, ...}], platform, chat_id, thread_id?, user_id?} ->
 * the session id the origin resolves to, or null. */
json_t *w32_routing_fa91a2_session_for_origin_pick(const json_t *in);

/* in: {orphans: [{id, source, message_count, started_at, last_active,
 *      parent_session_id, profile_name}], donors: {orphan_id: [row]},
 *      owner, max_gap_s?} -> the OrphanReport list. */
json_t *w32_routing_fa91a2_orphaned_sessions(const json_t *in);

/* in: {orphan_id, donor_id, donor: row|null, orphan: row|null, owner?, now?} ->
 * {result, writes: [{sql, params}]}. */
json_t *w32_routing_fa91a2_adopt_gate(const json_t *in);

/* in: {chat_id, thread_id, user_id, session_key, session_id, profile_name?,
 *      managed_mode?, expected_session_id?, move_session?, current?, existing?,
 *      now?} -> {result} / {error: "TopicAlreadyLinked"}, each with the writes
 * the guard lets through. */
json_t *w32_routing_fa91a2_bind_guard(const json_t *in);

/* in: {entries: {key: entry_json}, routing_scope?, kept: [key], now?} ->
 * {statements: [{sql, params}], rejected: [key]}, the keys the save refused. */
json_t *w32_routing_fa91a2_replace_entries(const json_t *in);

/* in: {rows: [{id, session_key, ...}], lineage: {id: [id]},
 *      delete: {deleted, removed_ids, skipped_guarded}, routing_rows: [...]} ->
 * the NeverActivePrune outcome. */
json_t *w32_routing_fa91a2_prune_outcome(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif
