/*
 * noc_w33_maintenanc_3e1b75_p2.h - C surface for upstream
 * persistence/sqlite/housekeeping/maintenance.py (gate w33_maintenanc_3e1b75_p2).
 *
 * Part 2 of 2: the filter assembly, the guard rewrites, the report shaping and
 * the statement builders the prune/archive/export/retention verbs run.  Every
 * function is a pure value->value map; ``now`` is an argument, and the
 * connection, the row data and the clock stay in the caller.
 */
#ifndef NOC_W33_MAINTENANC_3E1B75_P2_H
#define NOC_W33_MAINTENANC_3E1B75_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w33_maintenanc_3e1b75_p2_prune_filter_names @ ...maintenance.py:_PRUNE_FILTER_NAMES
 * The 25 keyword names the prune filters accept, sorted. */
json_t *w33_maintenanc_3e1b75_p2_prune_filter_names(void);

/* PoP: w33_maintenanc_3e1b75_p2_prune_filter_where @ ...maintenance.py:_prune_filter_where
 * req: {"filters":{...}, "now":n} -> {"where":..., "params":[...]} or
 * {"error":"TypeError", "message":...}. */
json_t *w33_maintenanc_3e1b75_p2_prune_filter_where(const json_t *req);

/* PoP: w33_maintenanc_3e1b75_p2_prune_where @ ...maintenance.py:_prune_where
 * req: {"older_than_days":n|null, "source":str|null, "filters":{...},
 * "whole_lineages":bool, "now":n} -> {"where","params"} or
 * {"error":"ValueError"|"TypeError", "message":...}. */
json_t *w33_maintenanc_3e1b75_p2_prune_where(const json_t *req);

/* PoP: w33_maintenanc_3e1b75_p2_count_open_sql @
 * ...maintenance.py:SQLiteSessionMaintenance.count_open_prune_matches
 * req: {"older_than_days","source","filters","now"} -> {"sql":...},
 * {"zero":true} when include_open is set, or {"error":"RuntimeError",...}. */
json_t *w33_maintenanc_3e1b75_p2_count_open_sql(const json_t *req);

/* PoP: w33_maintenanc_3e1b75_p2_prune_open_guard @
 * ...maintenance.py:SQLiteSessionMaintenance.prune_sessions
 * {"ok":true} unless include_open, which is a ValueError (delete is not reversible). */
json_t *w33_maintenanc_3e1b75_p2_prune_open_guard(int include_open);

/* PoP: w33_maintenanc_3e1b75_p2_reclaim_estimate @
 * ...maintenance.py:SQLiteSessionMaintenance.prune_reclaim_estimate
 * req: {prunable,total,prunable_bytes,total_bytes,measure_total_bytes} ->
 * the PruneReclaimEstimate fields (ints; total_bytes null when not measured). */
json_t *w33_maintenanc_3e1b75_p2_reclaim_estimate(const json_t *req);

/* PoP: w33_maintenanc_3e1b75_p2_export_safe @
 * ...maintenance.py:SQLiteSessionMaintenance.assert_export_safe
 * req: {max_messages, message_count, session_id} -> {"count":n} or
 * {"error":"ValueError"|"ExportTooLarge", "message":...}. */
json_t *w33_maintenanc_3e1b75_p2_export_safe(const json_t *req);

/* PoP: w33_maintenanc_3e1b75_p2_tree_key @ ...maintenance.py:_inside_tree.key
 * Native path identity: normcase (lowercase on Windows), ``\`` -> ``/``, no
 * trailing ``/``. */
json_t *w33_maintenanc_3e1b75_p2_tree_key(const char *path);

/* PoP: w33_maintenanc_3e1b75_p2_inside_tree @ ...maintenance.py:_inside_tree.inside
 * req: {"worktree_path":str, "cwd":str} -> 0/1.  The tree's own spelling and
 * its resolved spelling are both roots; the caller supplies a path whose
 * resolution is itself (an absolute, non-symlinked path), so ``Path.resolve``
 * stays a documented seam. */
json_t *w33_maintenanc_3e1b75_p2_inside_tree(const json_t *req);

/* PoP: w33_maintenanc_3e1b75_p2_tree_serve_facts @
 * ...maintenance.py:SQLiteSessionMaintenance.tree_served_sessions
 * req: {"served":bool, "chat_ids":[...]} -> {"served", "chat_ids": sorted|null}. */
json_t *w33_maintenanc_3e1b75_p2_tree_serve_facts(const json_t *req);

/* PoP: w33_maintenanc_3e1b75_p2_auto_archive_extra_set @
 * ...maintenance.py:SQLiteSessionMaintenance.auto_archive_lineage
 * The ``extra_set_sql`` fragment that stamps sweep provenance. */
json_t *w33_maintenanc_3e1b75_p2_auto_archive_extra_set(void);

/* PoP: w33_maintenanc_3e1b75_p2_heartbeat_register @
 * ...maintenance.py:SQLiteSessionMaintenance.register_backend_heartbeat
 * req: {backend_id,pid,started_at,last_heartbeat,profile,host,now} ->
 * {"skip":true} or {"sql":..., "params":[...]}. */
json_t *w33_maintenanc_3e1b75_p2_heartbeat_register(const json_t *req);

/* PoP: w33_maintenanc_3e1b75_p2_heartbeat_delete @
 * ...maintenance.py:SQLiteSessionMaintenance.clear_backend_heartbeat
 * req: {backend_id} -> {"skip":true} or {"sql":..., "params":[...]}. */
json_t *w33_maintenanc_3e1b75_p2_heartbeat_delete(const json_t *req);

/* PoP: w33_maintenanc_3e1b75_p2_prune_batches @
 * ...maintenance.py:SQLiteSessionMaintenance.prune_sessions (batched deletes)
 * req: {ids:[...], size:n} -> [{"placeholders","update_parent_sql",
 * "delete_messages_sql","delete_sessions_sql","params":[...]}]. */
json_t *w33_maintenanc_3e1b75_p2_prune_batches(const json_t *req);

/* PoP: w33_maintenanc_3e1b75_p2_retained_tree_sql @
 * ...maintenance.py:SQLiteSessionMaintenance.retained_tree_is_live
 * The live-tree probe; pending_only drops the message_count arm. */
json_t *w33_maintenanc_3e1b75_p2_retained_tree_sql(int pending_only);

/* PoP: w33_maintenanc_3e1b75_p2_served_tree_sql @ ...maintenance.py:_SERVED_TREE_SQL
 * The probe behind ``tree_served_sessions``. */
json_t *w33_maintenanc_3e1b75_p2_served_tree_sql(void);

#ifdef __cplusplus
}
#endif
#endif
