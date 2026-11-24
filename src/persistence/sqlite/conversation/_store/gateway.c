/*
 * gateway.c -- real C11 port of upstream Hermes
 *   persistence/sqlite/conversation/_store/gateway.py.
 *
 * PoP: w22_gateway_61b987_sessions_query @ persistence/sqlite/conversation/_store/gateway.py:list_gateway_sessions
 *
 * Faithful to the f-string in the source, including its whitespace: the
 * statement is the triple-quoted block verbatim, with the platform clause, the
 * active-only clause and the ORDER BY appended in that order.
 *
 * `_sql_session_last_active("sessions")` is imported from
 * persistence/sqlite/conversation/_store/sql_common.py, which has no C home of
 * its own; the resolved fragment (420 chars, EPOCH window 0.0..4200000000.0)
 * is mirrored below as LAST_ACTIVE_SQL.
 *
 * `_session_row_dict` and the holder's `_read_all`/`flush_for_read` are the DB
 * seam: the port answers the composed query and the parameter list, plus the
 * fact that the flush happens before the read (the source's ordering).
 */
#include "hermes_json.h"
#include "noc_w22_gateway_61b987.h"

#include <stdlib.h>
#include <string.h>

/* Resolved _sql_session_last_active("sessions"). */
static const char *const LAST_ACTIVE_SQL =
    "COALESCE((SELECT MAX(_act_v.v) FROM (SELECT sessions.last_activity_at AS v"
    " UNION ALL SELECT (SELECT MAX(_act_m.timestamp) FROM messages _act_m"
    " INDEXED BY idx_messages_session WHERE _act_m.session_id = sessions.id"
    " AND _act_m.timestamp BETWEEN 0.0 AND 4200000000.0)) _act_v"
    " WHERE _act_v.v BETWEEN 0.0 AND 4200000000.0),"
    " (SELECT _win.v FROM (SELECT sessions.started_at AS v) _win"
    " WHERE _win.v BETWEEN 0.0 AND 4200000000.0))";

static const char *const QUERY_HEAD =
    "\n            SELECT sessions.*,\n"
    "                   COALESCE(sp.prompt, sessions.system_prompt)\n"
    "                       AS _system_prompt_resolved,\n"
    "                   ";

static const char *const QUERY_MID =
    " AS last_active\n"
    "            FROM sessions\n"
    "            LEFT JOIN system_prompts sp\n"
    "              ON sp.hash = sessions.system_prompt_hash\n"
    "            WHERE session_key IS NOT NULL\n"
    "              AND started_at = (\n"
    "                  SELECT MAX(s2.started_at) FROM sessions s2\n"
    "                  WHERE s2.session_key = sessions.session_key\n"
    "              )\n"
    "        ";

/* Python truthiness; NULL reads as None (falsy). */
static int gw_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

json_t *w22_gateway_61b987_sessions_query(const json_t *platform,
                                          const json_t *active_only) {
    /* absent active_only -> the keyword default True */
    int active = active_only ? gw_truthy(active_only) : 1;
    int has_platform = gw_truthy(platform);
    size_t need;
    char *sql;
    json_t *out, *params;

    need = strlen(QUERY_HEAD) + strlen(LAST_ACTIVE_SQL) + strlen(QUERY_MID) +
           strlen(" AND LOWER(source) = LOWER(?)") +
           strlen(" AND ended_at IS NULL") +
           strlen(" ORDER BY last_active DESC") + 1;
    sql = (char *)malloc(need);
    if (!sql) return json_null();

    strcpy(sql, QUERY_HEAD);
    strcat(sql, LAST_ACTIVE_SQL);
    strcat(sql, QUERY_MID);
    if (has_platform) strcat(sql, " AND LOWER(source) = LOWER(?)");
    if (active) strcat(sql, " AND ended_at IS NULL");
    strcat(sql, " ORDER BY last_active DESC");

    out = json_object();
    json_set(out, "sql", json_string(sql));
    free(sql);

    params = json_array();
    if (has_platform) json_append(params, json_copy((json_t *)platform));
    json_set(out, "params", params);
    json_set(out, "flushed", json_number(1.0)); /* holder.flush_for_read() */
    return out;
}
