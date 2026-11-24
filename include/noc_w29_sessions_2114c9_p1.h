/*
 * noc_w29_sessions_2114c9_p1.h - C surface for upstream
 * persistence/sqlite/conversation/sessions.py (gate w29_sessions_2114c9_p1).
 *
 * Gate p1: the value-level prelude the repository leans on - text/JSON
 * coercion, the id key builders, the filter/pagination arguments and the
 * stored model_config decode.  Every function takes the request object and
 * answers with the value the Python helper answers with.
 */
#ifndef NOC_W29_SESSIONS_2114C9_P1_H
#define NOC_W29_SESSIONS_2114C9_P1_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req {value} -> stripped string or null  (sessions.py:_stripped) */
json_t *w29_sessions_2114c9_p1_stripped(const json_t *req);
/* req {text} -> decoded JSON or null  (sessions.py:_json_value) */
json_t *w29_sessions_2114c9_p1_json_value(const json_t *req);
/* req {text} -> null for a 64-hex content hash, else the decoded value  (sessions.py:_tool_pin) */
json_t *w29_sessions_2114c9_p1_tool_pin(const json_t *req);
/* req {values} -> array of SessionId strings  (sessions.py:_ids) */
json_t *w29_sessions_2114c9_p1_ids(const json_t *req);
/* req {values} -> de-duplicated id array, first occurrence order  (sessions.py:_removed) */
json_t *w29_sessions_2114c9_p1_removed(const json_t *req);
/* req {values} -> list or null when empty/absent  (sessions.py:_listed) */
json_t *w29_sessions_2114c9_p1_listed(const json_t *req);
/* req {values} -> list or null when None  (sessions.py:_optional_list) */
json_t *w29_sessions_2114c9_p1_optional_list(const json_t *req);
/* req {window:{limit,offset}} -> LIMIT operand, -1 for no limit  (sessions.py:_sql_limit) */
json_t *w29_sessions_2114c9_p1_sql_limit(const json_t *req);
/* req {row} -> float(last_active) or null  (sessions.py:_last_active) */
json_t *w29_sessions_2114c9_p1_last_active(const json_t *req);
/* req {raw} -> dict copy / decoded dict / {}  (_store/sessions.py:_parse_model_config) */
json_t *w29_sessions_2114c9_p1_parse_model_config(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
