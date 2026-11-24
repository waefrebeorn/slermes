/*
 * noc_w33_maintenanc_3e1b75_p1.h - C surface for upstream
 * persistence/sqlite/housekeeping/maintenance.py (gate w33_maintenanc_3e1b75_p1).
 *
 * Part 1 of 2: the selection-SQL builders.  Every function here is a pure
 * value->value map: text escaping, placeholder lists, id chunking, the cwd
 * prefix arm, the recency expression, the compression-edge CTEs, the pinned
 * tail, the stored-bytes sum, the projection field selection, the orphan-sweep
 * scope plan and the idle-archive selection SQL.  The SQLite connection, the
 * clock (``now`` is an argument) and the filesystem are seams.
 *
 * The strings are the upstream bytes: whitespace, newlines inside the
 * triple-quoted archive query, quoting and column order are significant.
 * Text case folding is ASCII (the wildcard/LIKE inputs are paths, model and
 * title text the ASCII range covers); non-ASCII case mapping is out of scope.
 */
#ifndef NOC_W33_MAINTENANC_3E1B75_P1_H
#define NOC_W33_MAINTENANC_3E1B75_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Alias used by the recency expression and the prune clauses. */
#define W33_MAINTENANC_3E1B75_P1_DEFAULT_ALIAS "s"

/* PoP: w33_maintenanc_3e1b75_p1_escape_like @ ...maintenance.py:_escape_like
 * ``\`` -> ``\\``, ``%`` -> ``\%``, ``_`` -> ``\_``; nothing else changes. */
json_t *w33_maintenanc_3e1b75_p1_escape_like(const char *text);

/* PoP: w33_maintenanc_3e1b75_p1_like @ ...maintenance.py:_like
 * ``"%" + escape_like(value.lower()) + "%"``. */
json_t *w33_maintenanc_3e1b75_p1_like(const char *value);

/* PoP: w33_maintenanc_3e1b75_p1_placeholders @ ...maintenance.py:_placeholders
 * ``?,?,?`` for an int count (0 or less -> ""). */
json_t *w33_maintenanc_3e1b75_p1_placeholders(double count);

/* PoP: w33_maintenanc_3e1b75_p1_id_chunks @ ...maintenance.py:_id_chunks
 * ids split into consecutive lists of at most ``size`` elements (900 when
 * ``size`` is absent). */
json_t *w33_maintenanc_3e1b75_p1_id_chunks(const json_t *ids, double size);

/* PoP: w33_maintenanc_3e1b75_p1_cwd_prefix_clause @
 * persistence/sqlite/conversation/_store/sessions.py:_cwd_prefix_clause
 * -> {"clause":..., "params":[prefix, esc+"/%", esc+"\\\\%"]}. */
json_t *w33_maintenanc_3e1b75_p1_cwd_prefix_clause(const char *prefix);

/* PoP: w33_maintenanc_3e1b75_p1_last_active_sql @
 * persistence/sqlite/gateway/_routes.py:_sql_session_last_active
 * The freshest-of(activity, newest message, started) recency expression. */
json_t *w33_maintenanc_3e1b75_p1_last_active_sql(const char *alias);

/* PoP: w33_maintenanc_3e1b75_p1_continuation_edge @ ...maintenance.py:_CONTINUATION_EDGE_SQL
 * ``p.end_reason = 'compression'`` followed by the non-continuation child
 * clauses (branch/delegate/reset markers and tool sessions). */
json_t *w33_maintenanc_3e1b75_p1_continuation_edge(void);

/* PoP: w33_maintenanc_3e1b75_p1_continued_ancestors_sql @ ...maintenance.py:_continued_ancestors_sql
 * The recursive CTE of compression ancestors a candidate WHERE does not select. */
json_t *w33_maintenanc_3e1b75_p1_continued_ancestors_sql(const char *candidates_where);

/* PoP: w33_maintenanc_3e1b75_p1_pinned_tail_sql @ ...maintenance.py:_PINNED_TAIL_SQL
 * The recursive CTE of unpinned continuations a pinned segment covers. */
json_t *w33_maintenanc_3e1b75_p1_pinned_tail_sql(void);

/* PoP: w33_maintenanc_3e1b75_p1_not_pinned_sql @ ...maintenance.py:_not_pinned_sql
 * ``COALESCE(alias.pinned, 0) = 0 AND alias.id NOT IN (<pinned tail>)``. */
json_t *w33_maintenanc_3e1b75_p1_not_pinned_sql(const char *alias);

/* PoP: w33_maintenanc_3e1b75_p1_row_bytes_sql @ ...maintenance.py:_row_bytes_sql
 * columns: PRAGMA table_info rows (["cid","name","type",...]) -> the sum of the
 * byte lengths of every TEXT/BLOB/typeless column, or "0" when none. */
json_t *w33_maintenanc_3e1b75_p1_row_bytes_sql(const json_t *columns);

/* PoP: w33_maintenanc_3e1b75_p1_projection_fields @ ...maintenance.py:SQLiteSessionMaintenance.session_projection_digest
 * available: the sessions columns present -> {"fields":[...],"order":" ORDER BY id"|""}
 * or null when no projection field is present. */
json_t *w33_maintenanc_3e1b75_p1_projection_fields(const json_t *available);

/* PoP: w33_maintenanc_3e1b75_p1_sweep_scope @ ...maintenance.py:SQLiteSessionMaintenance.sweep_orphaned_sessions
 * req: {max_idle_seconds, sources, exclude_pinned, heartbeat_staleness_seconds,
 * heartbeat_ownership_grace_seconds, respect_gateway_heartbeats, now} ->
 * {"skip":true} or {"select_sql":..., "select_params":[...]}. */
json_t *w33_maintenanc_3e1b75_p1_sweep_scope(const json_t *req);

/* PoP: w33_maintenanc_3e1b75_p1_idle_archive_sql @ ...maintenance.py:SQLiteSessionMaintenance.list_idle_archive_candidates
 * req: {idle_days, exclude_pinned, now, title} -> {"skip":true} or
 * {"sql":..., "params":[title, cutoff]}. */
json_t *w33_maintenanc_3e1b75_p1_idle_archive_sql(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
