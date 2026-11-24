/*
 * noc_w31_compressio_220b6d_p2.h - C surface for upstream
 * persistence/sqlite/conversation/_store/compression.py (gate
 * w31_compressio_220b6d_p2): the statement builders, the sessions-column
 * coercions and the durable counter/cooldown writes.
 *
 * The statements are returned as {sql, params} pairs, exactly the bytes and
 * bound values the writer would send; the connection that executes them and
 * the clock are seams and arrive as answers/inputs.
 */
#ifndef NOC_W31_COMPRESSIO_220B6D_P2_H
#define NOC_W31_COMPRESSIO_220B6D_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ``_CHAIN_STEP_SQL`` assembled from the fragments the source interpolates:
 * the branch/delegate json-extract probes, ``_RESET_CHILD_SQL.format(a='child')``
 * and ``_sql_session_last_active("child")``. */
json_t *w31_compressio_220b6d_p2_chain_step_sql(const char *jx_branched,
                                                const char *jx_delegate,
                                                const char *reset_child,
                                                const char *last_active);

/* ``_UNSET_ANCESTOR_SQL`` around ``_UNSET_CREATED_SQL.format(col='s.created_source')``. */
json_t *w31_compressio_220b6d_p2_unset_ancestor_sql(const char *unset_sql);

/* ``_UNSTAMPED_EDGE_CHILD_SQL`` around the parent/child unset predicates and
 * ``_non_continuation_child_sql("child.", "parent.id")``. */
json_t *w31_compressio_220b6d_p2_unstamped_edge_sql(const char *unset_parent,
                                                    const char *unset_child,
                                                    const char *non_continuation);

/* ``record_compression_failure_cooldown``: merge-max deadline write, null when
 * the session id is empty. */
json_t *w31_compressio_220b6d_p2_record_cooldown(const char *session_id,
                                                 const json_t *cooldown_until,
                                                 const json_t *error);

/* ``clear_compression_failure_cooldown``: null when the id is empty. */
json_t *w31_compressio_220b6d_p2_clear_cooldown(const char *session_id);

/* ``finalize_orphaned_compression_sessions``: {sql, params} with the 7-day cutoff. */
json_t *w31_compressio_220b6d_p2_finalize_orphans(double now);

/* ``_read_session_number``: max(zero, cast(value or zero)), zero on a missing
 * row or a value the cast rejects. kind is "int" or "float". */
json_t *w31_compressio_220b6d_p2_read_number(const json_t *value,
                                             const char *kind,
                                             const json_t *zero);

/* ``set_compression_fallback_streak`` / ``_ineffective_count`` /
 * ``_overload_streak``: max(0, int(streak)) written to the column; null when
 * the session id is empty. */
json_t *w31_compressio_220b6d_p2_streak_write(const char *column,
                                              const char *session_id,
                                              const json_t *streak);

/* ``set_compression_recovery_deadline``: max(0.0, float(deadline or 0.0)), or
 * NULL when it lands at zero; null (no write) when the id is empty. */
json_t *w31_compressio_220b6d_p2_deadline_write(const char *session_id,
                                                const json_t *deadline);

/* ``increment_compression_overload_streak``: the RETURNING row as int, else null. */
json_t *w31_compressio_220b6d_p2_overload_bump(const char *session_id,
                                               const json_t *row);

/* ``_publish_compression_child_on_conn``'s parent gate: "missing",
 * "proceed", "clear_stamp" (automatic stamp, cleared and re-stamped) or
 * "already_ended" (deliberate boundary, fails closed). */
json_t *w31_compressio_220b6d_p2_parent_gate(int parent_exists, int ended_at_set,
                                             int automatic_reason);

#ifdef __cplusplus
}
#endif
#endif
