/*
 * noc_w30_search_d40ff0_p1.h - C surface for upstream
 * persistence/sqlite/conversation/_store/search.py (gate
 * w30_search_d40ff0_p1).
 *
 * The query-normalisation half of message search: the FTS5 sanitiser, the
 * token quoter/normaliser the CJK routes use, the CJK classifiers, LIKE
 * escaping, multimodal text flattening, the result-field projection and the
 * deferred-rebuild percentage.  Nothing here opens a database or reads the
 * environment; every value is a pure function of the request object.
 */
#ifndef NOC_W30_SEARCH_D40FF0_P1_H
#define NOC_W30_SEARCH_D40FF0_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MAX_FTS5_QUERY_CHARS — the sanitiser caps input here before any scan. */
#define W30_SEARCH_D40FF0_P1_MAX_QUERY_CHARS 2048

/* {"codepoint": <int>} -> bool   (_is_cjk) */
json_t *w30_search_d40ff0_p1_is_cjk(const json_t *req);

/* {"text": <str>} -> bool        (_contains_cjk) */
json_t *w30_search_d40ff0_p1_contains_cjk(const json_t *req);

/* {"text": <str>} -> int         (_count_cjk) */
json_t *w30_search_d40ff0_p1_count_cjk(const json_t *req);

/* {"term": <str>} -> str         (sql_common.escape_like) */
json_t *w30_search_d40ff0_p1_escape_like(const json_t *req);

/* {"term": <str>} -> ["%t%", "%t%", "%t%"]   (_like_params) */
json_t *w30_search_d40ff0_p1_like_params(const json_t *req);

/* {"raw": <str>} -> [str, ...]   (_non_operator_tokens) */
json_t *w30_search_d40ff0_p1_non_op_tokens(const json_t *req);

/* {"raw": <str>} -> str          (_quote_fts_tokens) */
json_t *w30_search_d40ff0_p1_quote_tokens(const json_t *req);

/* {"raw": <str>} -> str          (_strip_cjk_wildcards) */
json_t *w30_search_d40ff0_p1_strip_wildcards(const json_t *req);

/* {"query": <str>} -> str        (_sanitize_fts5_query) */
json_t *w30_search_d40ff0_p1_sanitize(const json_t *req);

/* {"query": <str>} -> str|null   (_or_relaxed_query) */
json_t *w30_search_d40ff0_p1_or_relaxed(const json_t *req);

/* {"query": <str>} -> bool       (_trigram_eligible_tokens) */
json_t *w30_search_d40ff0_p1_trigram_eligible(const json_t *req);

/* {"raw": <str>} -> bool         (_has_short_cjk_token) */
json_t *w30_search_d40ff0_p1_short_cjk(const json_t *req);

/* {"raw": <str>, "trigram_available": <bool>} -> bool   (_trigram_route_ok;
 * the holder's index-availability flag is the seam.) */
json_t *w30_search_d40ff0_p1_trigram_route_ok(const json_t *req);

/* {"decoded": <any>} -> str      (_flatten_text) */
json_t *w30_search_d40ff0_p1_flatten_text(const json_t *req);

/* {"fields": null|[str,...]|str} -> {"ok": true, "fields": ...|null} or
 *                                  {"ok": false, "error": "<Type>: <msg>"}
 * (_search_message_fields) */
json_t *w30_search_d40ff0_p1_fields(const json_t *req);

/* {"phase": str, "total": int, "indexed": int} ->
 * {"phase", "percent", "indexed", "total"}   (_BackfillProgress.info) */
json_t *w30_search_d40ff0_p1_backfill_info(const json_t *req);

/* The normalisation constants, as an object. */
json_t *w30_search_d40ff0_p1_constants(void);

#ifdef __cplusplus
}
#endif
#endif
