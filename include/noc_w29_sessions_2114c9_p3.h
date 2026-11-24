/*
 * noc_w29_sessions_2114c9_p3.h - C surface for upstream
 * persistence/sqlite/conversation/sessions.py (gate w29_sessions_2114c9_p3).
 *
 * Gate p3: the statement text and the refusal predicate - the SQL fragments
 * the session verbs embed (the lineage CTEs, the local-chat predicate, the
 * recency expression, the Bots roster queries) built byte for byte, and the
 * store's title ValueError classified into TitleConflict / CanonicalTitleLocked.
 */
#ifndef NOC_W29_SESSIONS_2114C9_P3_H
#define NOC_W29_SESSIONS_2114C9_P3_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req {text} -> quoted SQL literal  (_schema_ddl.py:_sql_literal) */
json_t *w29_sessions_2114c9_p3_sql_literal(const json_t *req);
/* req {expression,path} -> non-throwing json_extract  (_schema_ddl.py:_sql_json_extract) */
json_t *w29_sessions_2114c9_p3_json_extract(const json_t *req);
/* req {alias,reasons_sql} -> legacy reset-child predicate  (_schema_ddl.py:_legacy_reset_child_sql) */
json_t *w29_sessions_2114c9_p3_legacy_reset_child(const json_t *req);
/* req {alias} -> _RESET_CHILD_SQL with the alias applied  (_schema_ddl.py:_RESET_CHILD_SQL) */
json_t *w29_sessions_2114c9_p3_reset_child(const json_t *req);
/* -> the descendants recursive CTE verbatim  (sessions.py:_DESCENDANTS_SQL) */
json_t *w29_sessions_2114c9_p3_descendants(const json_t *req);
/* req {alias} -> session recency expression  (_store/sql_common.py:_sql_session_last_active) */
json_t *w29_sessions_2114c9_p3_session_last_active(const json_t *req);
/* -> the local-chat predicate  (sessions.py:_LOCAL_CHAT) */
json_t *w29_sessions_2114c9_p3_local_chat(const json_t *req);
/* req {operator,subject} -> {sql, params}  (sessions.py:_read_bot_sessions.owner_filter) */
json_t *w29_sessions_2114c9_p3_owner_filter(const json_t *req);
/* req {operator,subject} -> {canonical, latest, worker} query text  (sessions.py:_read_bot_sessions) */
json_t *w29_sessions_2114c9_p3_roster_sqls(const json_t *req);
/* -> the last_active() statement  (sessions.py:SQLiteSessionRepository.last_active) */
json_t *w29_sessions_2114c9_p3_last_active_select(const json_t *req);
/* req {text} -> "conflict" {title,holder} | "canonical" | "pass"  (sessions.py:_title_refusals) */
json_t *w29_sessions_2114c9_p3_title_refusal(const json_t *req);
/* -> hidden_sources placeholder list, reset reasons, internal sources, prefixes  (sessions.py) */
json_t *w29_sessions_2114c9_p3_fragments(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
