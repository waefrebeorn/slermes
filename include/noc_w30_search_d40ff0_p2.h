/*
 * noc_w30_search_d40ff0_p2.h - C surface for upstream
 * persistence/sqlite/conversation/_store/search.py (gate
 * w30_search_d40ff0_p2).
 *
 * The statement-composition and routing half of message search: the shared
 * WHERE/ORDER/SELECT builders, the LIKE-boolean compiler, the FTS/LIKE/gap/CJK
 * statement pair (SQL text + bind order) each route executes, the route
 * predicate itself, and the session-id result ranking.
 *
 * Execution is a seam: each builder returns the exact SQL and parameter list
 * the live code hands to sqlite, which is what the oracle compares.
 */
#ifndef NOC_W30_SEARCH_D40FF0_P2_H
#define NOC_W30_SEARCH_D40FF0_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {"where": [str,...], "params": [...], "include_inactive": bool,
 *  "source_filter": [str]|null, "exclude_sources": [str]|null,
 *  "role_filter": [str]|null, "after_ts": int|null, "before_ts": int|null}
 *   -> {"where": [str,...], "params": [...]}      (_search_filter_clauses) */
json_t *w30_search_d40ff0_p2_filter_clauses(const json_t *req);

/* {"snippet_sql": str, "from_sql": str, "where": [str], "order_by": str,
 *  "limit_sql": str} -> str                      (_search_select_sql) */
json_t *w30_search_d40ff0_p2_select_sql(const json_t *req);

/* {"table", "match_query", "order_by_sql", "limit", "offset", + filter keys}
 *   -> {"sql": str, "params": [...]}             (_fts_match_sql) */
json_t *w30_search_d40ff0_p2_fts_match_sql(const json_t *req);

/* {"query": str} -> {"predicate": str, "params": [...], "snippet_term": str|null}
 * (_compile_like_boolean_query) */
json_t *w30_search_d40ff0_p2_like_boolean(const json_t *req);

/* {"query", "limit", "offset", "sort", + filter keys} ->
 * {"sql": str, "params": [...]} or null          (_search_messages_like_fallback) */
json_t *w30_search_d40ff0_p2_like_fallback(const json_t *req);

/* {"fts_query", "limit", "meta": {"high_water", "progress"}|null, + filters}
 *   -> {"sql", "params"} or null                 (_search_unindexed_gap) */
json_t *w30_search_d40ff0_p2_gap(const json_t *req);

/* {"query", "limit", "offset", + filter keys} -> {"sql", "params"}
 * (_search_cjk, LIKE branch; trigram availability is the seam that selects it) */
json_t *w30_search_d40ff0_p2_cjk_like(const json_t *req);

/* {"query": str, "limit": int, "rows": [{"id", "_lineage_root_id"}, ...]} ->
 * {"needle": str|null, "fetch_limit": int|null, "ranked": [row, ...]}
 * (search_sessions_by_id ranking) */
json_t *w30_search_d40ff0_p2_session_rank(const json_t *req);

/* {"sort": str|null} -> the ORDER BY text       (_FTS_ORDER_BY.get) */
json_t *w30_search_d40ff0_p2_order_by(const json_t *req);

/* {"query", "role_filter", "source_filter", "fts_stale", "fts_enabled",
 *  "trigram_available"} -> "empty"|"tool_like"|"stale"|"disabled"|"fts"|
 *                           "trigram"|"like_scan"   (_search_messages_impl route) */
json_t *w30_search_d40ff0_p2_route(const json_t *req);

/* {"query", "fts_stale", "trigram_available"} -> path name  (_describe_search_path) */
json_t *w30_search_d40ff0_p2_describe_path(const json_t *req);

/* The pinned SQL fragments / limits, as an object. */
json_t *w30_search_d40ff0_p2_sql_constants(void);

#ifdef __cplusplus
}
#endif
#endif
