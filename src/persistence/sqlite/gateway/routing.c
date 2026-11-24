/*
 * routing.c - C11 port of upstream persistence/sqlite/gateway/routing.py
 * (gates w32_routing_fa91a2_p1 .. _p2).
 *
 * PoP: w32_routing_fa91a2_normalize_profile              @ persistence/gateway/routing.py:normalize_topic_profile_name
 * PoP: w32_routing_fa91a2_route_namespace                @ persistence/gateway/routing.py:route_namespace
 * PoP: w32_routing_fa91a2_upsert_plan                    @ persistence/sqlite/gateway/routing.py:SQLiteRoutingRepository.upsert_routing_entry
 * PoP: w32_routing_fa91a2_origin_query                   @ persistence/sqlite/gateway/routing.py:SQLiteRoutingRepository.session_for_origin
 * PoP: w32_routing_fa91a2_never_active_query             @ persistence/sqlite/gateway/routing.py:SQLiteRoutingRepository.never_active_keyed_sessions
 * PoP: w32_routing_fa91a2_peer_plan                      @ persistence/sqlite/gateway/routing.py:SQLiteRoutingRepository.latest_session_for_peer
 * PoP: w32_routing_fa91a2_record_peer_plan               @ persistence/sqlite/gateway/routing.py:SQLiteRoutingRepository.record_session_peer
 * PoP: w32_routing_fa91a2_enable_topic_plan              @ persistence/sqlite/gateway/routing.py:SQLiteRoutingRepository.enable_topic_mode
 * PoP: w32_routing_fa91a2_routing_entries_for_sessions   @ persistence/sqlite/gateway/routing.py:SQLiteRoutingRepository._routing_entries_for_sessions
 * PoP: w32_routing_fa91a2_routing_entry_for_session      @ persistence/sqlite/gateway/routing.py:SQLiteRoutingRepository.routing_entry_for_session
 * PoP: w32_routing_fa91a2_session_for_origin_pick        @ persistence/sqlite/gateway/routing.py:SQLiteRoutingRepository.session_for_origin
 * PoP: w32_routing_fa91a2_orphaned_sessions              @ persistence/sqlite/gateway/routing.py:SQLiteRoutingRepository.orphaned_sessions
 * PoP: w32_routing_fa91a2_adopt_gate                     @ persistence/sqlite/gateway/routing.py:SQLiteRoutingRepository.adopt_orphan
 * PoP: w32_routing_fa91a2_bind_guard                     @ persistence/sqlite/gateway/routing.py:SQLiteRoutingRepository.bind_topic
 * PoP: w32_routing_fa91a2_replace_entries                @ persistence/sqlite/gateway/routing.py:SQLiteRoutingRepository.replace_routing_entries
 * PoP: w32_routing_fa91a2_prune_outcome                  @ persistence/sqlite/gateway/routing.py:SQLiteRoutingRepository.prune_never_active
 *
 * What is ported is what these verbs *decide*: the statement each one sends
 * (byte for byte, with the parameters it binds), which routing entry resolves
 * to a session, which predecessor an unkeyed session may be adopted from, which
 * topic binding a guard lets through, and how a prune's outcome is counted.
 *
 * Every seam stays in the caller: the StoreHolder (writer connection, read
 * pool, WAL generation, schema migration), the wall clock, the store's own
 * profile, `_session_row_dict`/`session_record` shaping, the topic-table
 * migration and the session delete.  All of them arrive here as fixture values,
 * so nothing in this file opens a file or touches SQLite.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "hermes_json.h"
#include "noc_w32_routing_fa91a2_p1.h"
#include "noc_w32_routing_fa91a2_p2.h"

/* persistence.authority.vocabulary.RETRY_HORIZON_S */
#define RT_RETRY_HORIZON_S 604800
/* persistence/sqlite/conversation/_store/retirement.py:RETIRED_PREFIX */
#define RT_RETIRED_PREFIX "gateway.retired_session.v1."
/* persistence/sqlite/gateway/_routes.py:ROUTE_FENCE_PREFIX */
#define RT_ROUTE_FENCE_PREFIX "gateway.retired_route.v1."
/* persistence/gateway/routing.py:ORPHAN_ADOPTION_MAX_GAP_S */
#define RT_ORPHAN_GAP_S 900.0
/* The fixture clock when a case names none. */
#define RT_DEFAULT_NOW 1700000000.0

#define RT_LINEAGE_AMBIGUOUS "ambiguous: more than one keyed predecessor matches this window"
#define RT_CONTEST_AMBIGUOUS "ambiguous: more than one unkeyed session claims this predecessor"
#define RT_SUPERSEDED_BY_REPAIR "superseded_by_repair"

/* ── string helpers ───────────────────────────────────────────────────── */

static char *rt_dup(const char *s) {
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static char *rt_ndup(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static char *rt_cat3(const char *a, const char *b, const char *c) {
    size_t la = a ? strlen(a) : 0, lb = b ? strlen(b) : 0, lc = c ? strlen(c) : 0;
    char *p = (char *)malloc(la + lb + lc + 1);
    if (!p) return NULL;
    if (la) memcpy(p, a, la);
    if (lb) memcpy(p + la, b, lb);
    if (lc) memcpy(p + la + lb, c, lc);
    p[la + lb + lc] = '\0';
    return p;
}

/* Python str.strip() whitespace. */
static int rt_ws(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* Python str.strip() over the ASCII whitespace these routing keys and profile
 * names carry (a name is a directory leaf, never an exotic space). */
static char *rt_strip(const char *s) {
    size_t a = 0, b;
    if (!s) return rt_dup("");
    b = strlen(s);
    while (a < b && rt_ws((unsigned char)s[a])) a++;
    while (b > a && rt_ws((unsigned char)s[b - 1])) b--;
    return rt_ndup(s + a, b - a);
}

/* ── value helpers ────────────────────────────────────────────────────── */

/* Python truthiness of a JSON value. */
static int rt_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* Python's numeric view: an int, a float or a bool; None is none of those. */
static int rt_number(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* repr() of the numbers a retention or a clock can hold: an integral value
 * prints without a fraction, anything else at the shortest width that
 * round-trips (what CPython's float repr does). */
static char *rt_num_str(double x) {
    char buf[64];
    int p;
    if (x > -1e16 && x < 1e16 && x == (double)(long long)x) {
        snprintf(buf, sizeof buf, "%.0f", x);
        return rt_dup(buf);
    }
    for (p = 12; p <= 17; p++) {
        snprintf(buf, sizeof buf, "%.*g", p, x);
        if (strtod(buf, NULL) == x) return rt_dup(buf);
    }
    snprintf(buf, sizeof buf, "%.17g", x);
    return rt_dup(buf);
}

/* Python str() over the scalar shapes these verbs stringify. */
static char *rt_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return rt_dup("None");
    switch (v->type) {
    case JSON_NULL:   return rt_dup("None");
    case JSON_BOOL:   return rt_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER: return rt_num_str(v->num_val);
    case JSON_STRING: return rt_dup(v->str_val ? v->str_val : "");
    case JSON_ARRAY:
    case JSON_OBJECT: return rt_dup("");
    }
    return rt_dup("");
}

/* str(value or ""): the coercion the topic/profile leaves apply first. */
static char *rt_str_or_empty(const json_t *v) {
    if (!rt_truthy(v)) return rt_dup("");
    return rt_str(v);
}

/* Python '==' over the shapes a routing identity can hold. */
static int rt_eq(const json_t *a, const json_t *b) {
    double x = 0.0, y = 0.0;
    size_t i, n;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if (rt_number(a, &x) && rt_number(b, &y)) return x == y;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type == JSON_ARRAY && b->type == JSON_ARRAY) {
        n = json_len(a);
        if (n != json_len(b)) return 0;
        for (i = 0; i < n; i++)
            if (!rt_eq(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    }
    if (a->type == JSON_OBJECT && b->type == JSON_OBJECT) {
        n = json_object_size(a);
        if (n != json_object_size(b)) return 0;
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(a, i);
            if (!k) return 0;
            if (!rt_eq(json_object_get_at(a, i), json_obj_get(b, k))) return 0;
        }
        return 1;
    }
    return 0;
}

/* A value copied as-is; a missing key reads null like a dict .get. */
static json_t *rt_copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}

static json_t *rt_call(const char *sql, json_t *params) {
    json_t *c = json_object();
    json_set(c, "sql", json_string(sql));
    json_set(c, "params", params ? params : json_array());
    return c;
}

static json_t *rt_out_calls(json_t *calls) {
    json_t *out = json_object();
    json_set(out, "calls", calls);
    return out;
}

/* The clock a verb stamps, supplied by the case. */
static double rt_now(const json_t *in) {
    return json_get_num(in, "now", RT_DEFAULT_NOW);
}

/* route_namespace(): the <ns> slot of "agent:<ns>:...", "main" otherwise. */
static char *rt_namespace_of(const char *key) {
    char *raw = key ? rt_dup(key) : rt_dup("");
    char *out = NULL;
    if (raw) {
        const char *c1 = strchr(raw, ':');
        if (c1) {
            const char *rest = c1 + 1;
            const char *c2 = strchr(rest, ':');
            size_t head = (size_t)(c1 - raw);
            /* len(parts) > 2 and parts[0] == "agent" and parts[1] */
            if (c2 && c2 > rest && head == 5 && !strncmp(raw, "agent", 5))
                out = rt_ndup(rest, (size_t)(c2 - rest));
        }
        free(raw);
    }
    return out ? out : rt_dup("main");
}

/* normalize_topic_profile_name(): stripped, empty reads "default". */
static char *rt_normalize_profile(const json_t *value) {
    char *raw = rt_str_or_empty(value);
    char *trim = rt_strip(raw);
    free(raw);
    if (!trim) return rt_dup("default");
    if (!trim[0]) { free(trim); return rt_dup("default"); }
    return trim;
}

/* _to_int(): None stays None, anything else reads 1/0 by truthiness. */
static json_t *rt_to_int(const json_t *v) {
    if (!v || v->type == JSON_NULL) return json_null();
    return json_number(rt_truthy(v) ? 1 : 0);
}

/* ── statement texts (verbatim from the live f-strings) ───────────────── */

static const char SQL_UPSERT[] =
    "INSERT INTO gateway_routing (scope, session_key, entry_json, updated_at)\n"
    "                   SELECT :scope, :key, :entry, :now WHERE NOT (\n"
    "    EXISTS (SELECT 1 FROM state_meta m\n"
    "             WHERE m.key = :retired_prefix || json_extract(:entry, '$.session_id')\n"
    "               AND (:ns = 'main' OR NOT CASE WHEN json_valid(m.value)\n"
    "                                             THEN json_extract(m.value, '$.own') IS NOT NULL ELSE 0 END))\n"
    "    OR EXISTS (SELECT 1 FROM state_meta f\n"
    "                WHERE f.key = :fence_prefix || :ns || ':' || json_extract(:entry, '$.session_id')\n"
    "                  AND NOT CASE WHEN json_valid(f.value)\n"
    "                               THEN coalesce(json_extract(f.value, '$.expires')\n"
    "                                             AND json_extract(f.value, '$.retired_at') < :now - :horizon, 0)\n"
    "                               ELSE 0 END\n"
    "                  AND (:ns != 'main' OR NOT EXISTS (\n"
    "                          SELECT 1 FROM sessions s WHERE s.id = json_extract(:entry, '$.session_id')))))\n"
    "                   ON CONFLICT(scope, session_key) DO UPDATE SET\n"
    "                       entry_json = excluded.entry_json,\n"
    "                       updated_at = excluded.updated_at";

static const char SQL_ORIGIN[] =
    "\n"
    "            SELECT id, user_id, started_at FROM sessions\n"
    "            WHERE LOWER(source) = LOWER(?)\n"
    "              AND session_key IS NOT NULL\n"
    "              AND chat_id = ?\n"
    "              AND ended_at IS NULL\n"
    "        ";

static const char SQL_ORIGIN_THREAD[] =
    "\n"
    "            SELECT id, user_id, started_at FROM sessions\n"
    "            WHERE LOWER(source) = LOWER(?)\n"
    "              AND session_key IS NOT NULL\n"
    "              AND chat_id = ?\n"
    "              AND ended_at IS NULL\n"
    "         AND COALESCE(thread_id, '') = ?";

static const char SQL_NEVER_ACTIVE[] =
    "\n"
    "                SELECT s.id, s.session_key, s.source, s.chat_id,\n"
    "                       s.chat_type, s.user_id, s.started_at\n"
    "                  FROM sessions s\n"
    "                 WHERE s.session_key IS NOT NULL\n"
    "                   AND s.ended_at IS NULL\n"
    "                   AND s.title IS NULL\n"
    "                   AND s.last_activity_at IS NULL\n"
    "                   AND COALESCE(s.message_count, 0) = 0\n"
    "                   AND COALESCE(s.tool_call_count, 0) = 0\n"
    "                   AND COALESCE(s.api_call_count, 0) = 0\n"
    "                   AND COALESCE(s.input_tokens, 0) = 0\n"
    "                   AND COALESCE(s.output_tokens, 0) = 0\n"
    "                   AND COALESCE(s.pinned, 0) = 0\n"
    "                   AND COALESCE(s.archived, 0) = 0\n"
    "                   AND s.started_at IS NOT NULL\n"
    "                   AND s.started_at < ?\n"
    "                   AND NOT EXISTS (\n"
    "                           SELECT 1 FROM messages m WHERE m.session_id = s.id\n"
    "                       )\n"
    "                 ORDER BY s.started_at\n"
    "                ";

static const char SQL_PEER_KEY[] =
    "\n"
    "                SELECT s.*,\n"
    "                       COALESCE(sp.prompt, s.system_prompt)\n"
    "                           AS _system_prompt_resolved,\n"
    "                       (COALESCE(s.message_count, 0) > 0 OR EXISTS (\n"
    "                           SELECT 1 FROM messages WHERE messages.session_id = s.id LIMIT 1\n"
    "                       )) AS _has_messages\n"
    "                FROM sessions s\n"
    "                LEFT JOIN system_prompts sp ON sp.hash = s.system_prompt_hash\n"
    "                WHERE 1=1\n"
    "                  AND s.session_key = ?\n"
    "                  AND s.source = ?\n"
    "                  AND (COALESCE(json_extract((CASE WHEN json_valid(s.model_config) THEN s.model_config ELSE json_object() END), '$._delegate_from'), '') = '' OR json_extract((CASE WHEN json_valid(s.model_config) THEN s.model_config ELSE json_object() END), '$._reset_from') IS NOT NULL OR json_extract((CASE WHEN json_valid(s.model_config) THEN s.model_config ELSE json_object() END), '$._chat_adopted') IS NOT NULL)\n"
    "                  AND (s.ended_at IS NULL OR s.end_reason IN ('agent_close', 'ws_orphan_reap', 'superseded_by_resume', 'startup_orphan_reap')\n"
    "                       OR (s.handoff_state = 'completed'))\n"
    "                  AND NOT EXISTS (\n"
    "                      SELECT 1 FROM sessions b\n"
    "                      WHERE b.session_key = s.session_key\n"
    "                        AND b.source = s.source\n"
    "                        AND b.ended_at IS NOT NULL\n"
    "                        AND b.end_reason IN ('session_reset', 'session_switch', 'idle', 'daily', 'suspended', 'resume_pending_expired')\n"
    "                        AND b.ended_at\n"
    "                            > COALESCE(s.last_activity_at, s.started_at)\n"
    "                  )\n"
    "                ORDER BY _has_messages DESC,\n"
    "                         COALESCE(s.last_activity_at, s.started_at) DESC\n"
    "                LIMIT 1\n"
    "                ";

static const char SQL_PEER_TUPLE[] =
    "\n"
    "                SELECT s.*,\n"
    "                       COALESCE(sp.prompt, s.system_prompt)\n"
    "                           AS _system_prompt_resolved,\n"
    "                       (COALESCE(s.message_count, 0) > 0 OR EXISTS (\n"
    "                           SELECT 1 FROM messages WHERE messages.session_id = s.id LIMIT 1\n"
    "                       )) AS _has_messages\n"
    "                FROM sessions s\n"
    "                LEFT JOIN system_prompts sp ON sp.hash = s.system_prompt_hash\n"
    "                WHERE 1=1\n"
    "                  AND s.source = ?\n"
    "                  AND COALESCE(s.user_id, '') = COALESCE(?, '')\n"
    "                  AND COALESCE(s.chat_id, '') = COALESCE(?, '')\n"
    "                  AND COALESCE(s.chat_type, '') = COALESCE(?, '')\n"
    "                  AND COALESCE(s.thread_id, '') = COALESCE(?, '')\n"
    "                  AND (COALESCE(json_extract((CASE WHEN json_valid(s.model_config) THEN s.model_config ELSE json_object() END), '$._delegate_from'), '') = '' OR json_extract((CASE WHEN json_valid(s.model_config) THEN s.model_config ELSE json_object() END), '$._reset_from') IS NOT NULL OR json_extract((CASE WHEN json_valid(s.model_config) THEN s.model_config ELSE json_object() END), '$._chat_adopted') IS NOT NULL)\n"
    "                  AND (? IS NULL OR COALESCE(s.profile_name, ?) = ?)\n"
    "                  AND (s.ended_at IS NULL OR s.end_reason IN ('agent_close', 'ws_orphan_reap', 'superseded_by_resume', 'startup_orphan_reap')\n"
    "                       OR (s.handoff_state = 'completed'))\n"
    "                  AND (COALESCE(s.message_count, 0) > 0 OR EXISTS (\n"
    "                      SELECT 1 FROM messages WHERE messages.session_id = s.id LIMIT 1\n"
    "                  ))\n"
    "                  AND NOT EXISTS (\n"
    "                      SELECT 1 FROM sessions b\n"
    "                      WHERE b.source = s.source\n"
    "                        AND COALESCE(b.user_id, '') = COALESCE(s.user_id, '')\n"
    "                        AND COALESCE(b.chat_id, '') = COALESCE(s.chat_id, '')\n"
    "                        AND COALESCE(b.chat_type, '') = COALESCE(s.chat_type, '')\n"
    "                        AND COALESCE(b.thread_id, '') = COALESCE(s.thread_id, '')\n"
    "                        AND (? IS NULL OR COALESCE(b.profile_name, ?) = ?)\n"
    "                        AND b.ended_at IS NOT NULL\n"
    "                        AND b.end_reason IN ('session_reset', 'session_switch', 'idle', 'daily', 'suspended', 'resume_pending_expired')\n"
    "                        AND b.ended_at\n"
    "                            > COALESCE(s.last_activity_at, s.started_at)\n"
    "                  )\n"
    "                ORDER BY COALESCE(s.last_activity_at, s.started_at) DESC\n"
    "                LIMIT 1\n"
    "                ";

static const char SQL_RECORD_UPDATE[] =
    "\n"
    "                   UPDATE sessions\n"
    "                   SET session_key = ?, source = ?, user_id = ?, chat_id = ?,\n"
    "                       chat_type = ?, thread_id = ?,\n"
    "                       display_name = COALESCE(?, display_name),\n"
    "                       origin_json = COALESCE(?, origin_json),\n"
    "                       transport_profile = COALESCE(?, transport_profile)\n"
    "                   WHERE id = ?";

static const char SQL_RECORD_INSERT[] =
    "INSERT INTO sessions (\n"
    "                               id, source, created_source, user_id, session_key, chat_id,\n"
    "                               chat_type, thread_id, display_name, origin_json,\n"
    "                               profile_name, transport_profile, started_at\n"
    "                           )\n"
    "                           VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)\n"
    "                           ON CONFLICT(id) DO UPDATE SET\n"
    "                               session_key = COALESCE(sessions.session_key, excluded.session_key),\n"
    "                               chat_id = COALESCE(sessions.chat_id, excluded.chat_id),\n"
    "                               chat_type = COALESCE(sessions.chat_type, excluded.chat_type),\n"
    "                               thread_id = COALESCE(sessions.thread_id, excluded.thread_id),\n"
    "                               display_name = COALESCE(sessions.display_name, excluded.display_name),\n"
    "                               origin_json = COALESCE(sessions.origin_json, excluded.origin_json),\n"
    "                               transport_profile = COALESCE(sessions.transport_profile, excluded.transport_profile),\n"
    "                               created_source = COALESCE(sessions.created_source, excluded.created_source)";

static const char SQL_RECORD_CTE[] =
    "\n"
    "                    WITH RECURSIVE compression_lineage(id) AS (\n"
    "                        SELECT ?\n"
    "                        UNION\n"
    "                        SELECT parent.id\n"
    "                        FROM compression_lineage lineage\n"
    "                        JOIN sessions child ON child.id = lineage.id\n"
    "                        JOIN sessions parent ON parent.id = child.parent_session_id\n"
    "                        WHERE parent.end_reason = 'compression'\n"
    "                          AND json_extract((CASE WHEN json_valid(child.model_config) THEN child.model_config ELSE json_object() END), '$._branched_from') IS NULL\n"
    "                          AND json_extract((CASE WHEN json_valid(child.model_config) THEN child.model_config ELSE json_object() END), '$._delegate_from') IS NULL\n"
    "                          AND NOT (json_extract((CASE WHEN json_valid(child.model_config) THEN child.model_config ELSE json_object() END), '$._reset_from') IS NOT NULL OR EXISTS (SELECT 1 FROM sessions p            WHERE p.id = child.parent_session_id            AND p.end_reason IN ('session_reset', 'session_switch', 'idle', 'daily', 'suspended', 'resume_pending_expired')            AND child.session_key IS NOT NULL            AND child.session_key != ''            AND child.session_key = p.session_key))\n"
    "                          AND COALESCE(child.source, '') != 'tool'\n"
    "                    )\n"
    "                \n"
    "                   UPDATE sessions\n"
    "                   SET session_key = ?, source = ?, user_id = ?, chat_id = ?,\n"
    "                       chat_type = ?, thread_id = ?,\n"
    "                       display_name = COALESCE(?, display_name),\n"
    "                       origin_json = COALESCE(?, origin_json),\n"
    "                       transport_profile = COALESCE(?, transport_profile)\n"
    "                   WHERE id IN (SELECT id FROM compression_lineage)";

static const char SQL_TOPIC_ENABLE[] =
    "\n"
    "                INSERT INTO telegram_dm_topic_mode (\n"
    "                    profile_name, chat_id, user_id, enabled, activated_at, updated_at,\n"
    "                    has_topics_enabled, allows_users_to_create_topics,\n"
    "                    capability_checked_at\n"
    "                ) VALUES (?, ?, ?, 1, ?, ?, ?, ?, ?)\n"
    "                ON CONFLICT(profile_name, chat_id) DO UPDATE SET\n"
    "                    user_id = excluded.user_id,\n"
    "                    enabled = 1,\n"
    "                    updated_at = excluded.updated_at,\n"
    "                    has_topics_enabled = excluded.has_topics_enabled,\n"
    "                    allows_users_to_create_topics = excluded.allows_users_to_create_topics,\n"
    "                    capability_checked_at = excluded.capability_checked_at\n"
    "                ";

static const char SQL_TOPIC_BIND[] =
    "\n"
    "                    INSERT INTO telegram_dm_topic_bindings (\n"
    "                        profile_name, chat_id, thread_id, user_id, session_key, session_id,\n"
    "                        managed_mode, linked_at, updated_at\n"
    "                    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)\n"
    "                    ON CONFLICT(profile_name, chat_id, thread_id) DO UPDATE SET\n"
    "                        user_id = excluded.user_id,\n"
    "                        session_key = excluded.session_key,\n"
    "                        session_id = excluded.session_id,\n"
    "                        managed_mode = excluded.managed_mode,\n"
    "                        updated_at = excluded.updated_at\n"
    "                    ";

static const char SQL_TOPIC_BIND_DELETE[] =
    "DELETE FROM telegram_dm_topic_bindings WHERE session_id = ?";

static const char SQL_REPLACE_INSERT[] =
    "INSERT INTO gateway_routing (scope, session_key, entry_json, updated_at) SELECT :scope, :key, :entry, :now WHERE NOT (\n"
    "    EXISTS (SELECT 1 FROM state_meta m\n"
    "             WHERE m.key = :retired_prefix || json_extract(:entry, '$.session_id')\n"
    "               AND (:ns = 'main' OR NOT CASE WHEN json_valid(m.value)\n"
    "                                             THEN json_extract(m.value, '$.own') IS NOT NULL ELSE 0 END))\n"
    "    OR EXISTS (SELECT 1 FROM state_meta f\n"
    "                WHERE f.key = :fence_prefix || :ns || ':' || json_extract(:entry, '$.session_id')\n"
    "                  AND NOT CASE WHEN json_valid(f.value)\n"
    "                               THEN coalesce(json_extract(f.value, '$.expires')\n"
    "                                             AND json_extract(f.value, '$.retired_at') < :now - :horizon, 0)\n"
    "                               ELSE 0 END\n"
    "                  AND (:ns != 'main' OR NOT EXISTS (\n"
    "                          SELECT 1 FROM sessions s WHERE s.id = json_extract(:entry, '$.session_id')))))";

static const char SQL_ADOPT[] =
    "UPDATE sessions\n"
    "                      SET session_key = ?,\n"
    "                          chat_id = COALESCE(chat_id, ?),\n"
    "                          chat_type = COALESCE(chat_type, ?),\n"
    "                          thread_id = COALESCE(thread_id, ?),\n"
    "                          user_id = COALESCE(user_id, ?),\n"
    "                          origin_json = COALESCE(origin_json, ?),\n"
    "                          display_name = COALESCE(display_name, ?),\n"
    "                          parent_session_id = COALESCE(parent_session_id, ?)\n"
    "                    WHERE id = ? AND session_key IS NULL";

static const char SQL_ADOPT_END[] =
    "UPDATE sessions SET ended_at = COALESCE(ended_at, ?), end_reason = 'superseded_by_repair' WHERE id = ?";

static const char SQL_PROBE[] =
    "SELECT 1 FROM sessions WHERE id = ? LIMIT 1";

/* ══ gate w32_routing_fa91a2_p1: statements and coercions ══════════════ */

json_t *w32_routing_fa91a2_normalize_profile(const json_t *in) {
    char *name = rt_normalize_profile(json_obj_get(in, "value"));
    json_t *out = json_string(name ? name : "default");
    free(name);
    return out;
}

json_t *w32_routing_fa91a2_route_namespace(const json_t *in) {
    char *raw = rt_str_or_empty(json_obj_get(in, "session_key"));
    char *ns = rt_namespace_of(raw);
    json_t *out = json_string(ns ? ns : "main");
    free(raw);
    free(ns);
    return out;
}

json_t *w32_routing_fa91a2_upsert_plan(const json_t *in) {
    const json_t *key = json_obj_get(in, "session_key");
    const json_t *entry = json_obj_get(in, "entry_json");
    const json_t *scope = json_obj_get(in, "routing_scope");
    json_t *calls = json_array();
    if (rt_truthy(key) && rt_truthy(entry)) {
        char *k = rt_str(key);
        char *ns = rt_namespace_of(k);
        json_t *params = json_object();
        json_set(params, "scope", scope ? json_copy(scope) : json_string(""));
        json_set(params, "key", json_copy(key));
        json_set(params, "entry", json_copy(entry));
        json_set(params, "ns", json_string(ns ? ns : "main"));
        json_set(params, "now", json_number(rt_now(in)));
        json_set(params, "horizon", json_number(RT_RETRY_HORIZON_S));
        json_set(params, "retired_prefix", json_string(RT_RETIRED_PREFIX));
        json_set(params, "fence_prefix", json_string(RT_ROUTE_FENCE_PREFIX));
        json_append(calls, rt_call(SQL_UPSERT, params));
        free(k);
        free(ns);
    }
    return rt_out_calls(calls);
}

/*  chat_id in (None, ""): absent, null or the empty string. */
static int rt_chat_absent(const json_t *chat) {
    if (!chat || chat->type == JSON_NULL) return 1;
    if (chat->type == JSON_STRING && !(chat->str_val && chat->str_val[0])) return 1;
    return 0;
}

json_t *w32_routing_fa91a2_origin_query(const json_t *in) {
    const json_t *platform = json_obj_get(in, "platform");
    const json_t *chat = json_obj_get(in, "chat_id");
    const json_t *thread = json_obj_get(in, "thread_id");
    json_t *calls = json_array();
    if (rt_truthy(platform) && !rt_chat_absent(chat)) {
        char *plat = rt_str(platform);
        char *cid = rt_str(chat);
        json_t *params = json_array();
        char *sql;
        json_append(params, json_string(plat));
        json_append(params, json_string(cid));
        if (thread && thread->type != JSON_NULL) {
            char *tid = rt_str(thread);
            json_append(params, json_string(tid));
            free(tid);
            sql = rt_cat3(SQL_ORIGIN_THREAD, " ORDER BY started_at DESC", "");
        } else {
            sql = rt_cat3(SQL_ORIGIN, " ORDER BY started_at DESC", "");
        }
        json_append(calls, rt_call(sql, params));
        free(sql);
        free(plat);
        free(cid);
    }
    return rt_out_calls(calls);
}

json_t *w32_routing_fa91a2_never_active_query(const json_t *in) {
    const json_t *days = json_obj_get(in, "older_than_days");
    json_t *out;
    double d;
    char msg[512];
    if (!rt_number(days, &d)) {
        out = json_object();
        json_set(out, "error", json_string(
            "TypeError: '<' not supported between instances of 'NoneType' and 'int'"));
        return out;
    }
    if (d < 0) {
        char *repr = rt_num_str(d);
        snprintf(msg, sizeof msg,
                 "older_than_days must be >= 0, got %s: a negative retention builds a future "
                 "cutoff that matches every never-active keyed row.",
                 repr ? repr : "0");
        free(repr);
        out = json_object();
        json_set(out, "error", json_string(msg));
        return out;
    }
    {
        json_t *calls = json_array();
        json_t *params = json_array();
        json_append(params, json_number(rt_now(in) - (d * 86400.0)));
        json_append(calls, rt_call(SQL_NEVER_ACTIVE, params));
        return rt_out_calls(calls);
    }
}

json_t *w32_routing_fa91a2_peer_plan(const json_t *in) {
    const json_t *key = json_obj_get(in, "session_key");
    const json_t *chat = json_obj_get(in, "chat_id");
    const json_t *ctype = json_obj_get(in, "chat_type");
    const json_t *owner = json_obj_get(in, "owner");
    json_t *calls = json_array();
    if (rt_truthy(key)) {
        json_t *keyed = json_array();
        json_append(keyed, json_copy(key));
        json_append(keyed, rt_copy_or_null(json_obj_get(in, "source")));
        json_append(calls, rt_call(SQL_PEER_KEY, keyed));
        /* The tuple lookup runs only when both a chat id and a chat type exist. */
        if (chat && chat->type != JSON_NULL && ctype && ctype->type != JSON_NULL) {
            json_t *tuple = json_array();
            int i;
            json_append(tuple, rt_copy_or_null(json_obj_get(in, "source")));
            json_append(tuple, rt_copy_or_null(json_obj_get(in, "user_id")));
            json_append(tuple, json_copy(chat));
            json_append(tuple, json_copy(ctype));
            json_append(tuple, rt_copy_or_null(json_obj_get(in, "thread_id")));
            for (i = 0; i < 6; i++) json_append(tuple, rt_copy_or_null(owner));
            json_append(calls, rt_call(SQL_PEER_TUPLE, tuple));
        }
    }
    return rt_out_calls(calls);
}

/* The nine routing-column values record_session_peer stamps, in order. */
static void rt_push_identity(json_t *arr, const json_t *in) {
    static const char *const FIELDS[] = {
        "session_key", "source", "user_id", "chat_id", "chat_type",
        "thread_id", "display_name", "origin_json", "transport_profile",
    };
    size_t i;
    for (i = 0; i < sizeof(FIELDS) / sizeof(FIELDS[0]); i++)
        json_append(arr, rt_copy_or_null(json_obj_get(in, FIELDS[i])));
}

json_t *w32_routing_fa91a2_record_peer_plan(const json_t *in) {
    const json_t *sid = json_obj_get(in, "session_id");
    const json_t *key = json_obj_get(in, "session_key");
    int ancestors = rt_truthy(json_obj_get(in, "include_compression_ancestors"));
    json_t *calls = json_array();
    if (!rt_truthy(sid) || !rt_truthy(key)) return rt_out_calls(calls);

    if (ancestors) {
        json_t *params = json_array();
        json_append(params, json_copy(sid));
        rt_push_identity(params, in);
        json_append(calls, rt_call(SQL_RECORD_CTE, params));
        return rt_out_calls(calls);
    }

    {
        json_t *params = json_array();
        rt_push_identity(params, in);
        json_append(params, json_copy(sid));
        json_append(calls, rt_call(SQL_RECORD_UPDATE, params));
    }
    /* The UPDATE silently no-ops on a missing row: the probe decides. */
    {
        json_t *probe = json_array();
        json_append(probe, json_copy(sid));
        json_append(calls, rt_call(SQL_PROBE, probe));
    }
    if (!rt_truthy(json_obj_get(in, "existing_row"))) {
        const json_t *source = json_obj_get(in, "source");
        json_t *row = json_array();
        json_append(row, json_copy(sid));
        json_append(row, rt_copy_or_null(source));
        json_append(row, rt_copy_or_null(source));      /* created_source */
        json_append(row, rt_copy_or_null(json_obj_get(in, "user_id")));
        json_append(row, rt_copy_or_null(key));
        json_append(row, rt_copy_or_null(json_obj_get(in, "chat_id")));
        json_append(row, rt_copy_or_null(json_obj_get(in, "chat_type")));
        json_append(row, rt_copy_or_null(json_obj_get(in, "thread_id")));
        json_append(row, rt_copy_or_null(json_obj_get(in, "display_name")));
        json_append(row, rt_copy_or_null(json_obj_get(in, "origin_json")));
        json_append(row, rt_copy_or_null(json_obj_get(in, "owner")));
        json_append(row, rt_copy_or_null(json_obj_get(in, "transport_profile")));
        json_append(row, json_number(rt_now(in)));
        json_append(calls, rt_call(SQL_RECORD_INSERT, row));
    }
    return rt_out_calls(calls);
}

json_t *w32_routing_fa91a2_enable_topic_plan(const json_t *in) {
    const json_t *profile_in = json_obj_get(in, "profile_name");
    char *profile = rt_normalize_profile(profile_in);
    char *chat = rt_str(json_obj_get(in, "chat_id"));
    char *user = rt_str(json_obj_get(in, "user_id"));
    double now = rt_now(in);
    json_t *params = json_array();
    json_t *calls = json_array();
    json_append(params, json_string(profile ? profile : "default"));
    json_append(params, json_string(chat ? chat : ""));
    json_append(params, json_string(user ? user : ""));
    json_append(params, json_number(now));                                   /* activated_at */
    json_append(params, json_number(now));                                   /* updated_at */
    json_append(params, rt_to_int(json_obj_get(in, "has_topics_enabled")));
    json_append(params, rt_to_int(json_obj_get(in, "allows_users_to_create_topics")));
    json_append(params, json_number(now));                                   /* capability_checked_at */
    json_append(calls, rt_call(SQL_TOPIC_ENABLE, params));
    free(profile);
    free(chat);
    free(user);
    return rt_out_calls(calls);
}

/* ══ gate w32_routing_fa91a2_p2: index reads and decisions ═════════════ */

/* entry.get("session_id") in session_ids -- a set of str never holds a
 * non-string, so only string members can match. */
static int rt_in_str_list(const json_t *ids, const json_t *v) {
    size_t i, n;
    if (!ids || ids->type != JSON_ARRAY || !v || v->type != JSON_STRING) return 0;
    n = json_len(ids);
    for (i = 0; i < n; i++) {
        const json_t *x = json_get(ids, i);
        if (x && x->type == JSON_STRING &&
            !strcmp(x->str_val ? x->str_val : "", v->str_val ? v->str_val : "")) return 1;
    }
    return 0;
}

/* json.loads(row["entry_json"] or "{}") for a dict entry, else NULL: malformed
 * text, non-text and over-nested payloads are all skipped by the live code. */
static json_t *rt_parse_entry(const json_t *row) {
    const json_t *raw = json_obj_get(row, "entry_json");
    const char *text;
    json_t *entry;
    if (!raw || raw->type != JSON_STRING) return NULL;
    text = raw->str_val ? raw->str_val : "";
    if (!text[0]) text = "{}";
    entry = json_parse(text, NULL);
    if (!entry) return NULL;
    if (entry->type != JSON_OBJECT) { json_free(entry); return NULL; }
    return entry;
}

static json_t *rt_entries_for(const json_t *rows, const json_t *ids) {
    json_t *found = json_array();
    size_t i, n = rows ? json_len(rows) : 0;
    if (!ids || ids->type != JSON_ARRAY || json_len(ids) == 0) return found;
    for (i = 0; i < n; i++) {
        const json_t *row = json_get(rows, i);
        const json_t *sid;
        json_t *entry;
        if (!row || row->type != JSON_OBJECT) continue;
        entry = rt_parse_entry(row);
        if (!entry) continue;
        sid = json_obj_get(entry, "session_id");
        if (sid && rt_in_str_list(ids, sid)) {
            json_t *triple = json_array();
            json_append(triple, rt_copy_or_null(json_obj_get(row, "scope")));
            json_append(triple, rt_copy_or_null(json_obj_get(row, "session_key")));
            json_append(triple, json_copy(sid));
            json_append(found, triple);
        }
        json_free(entry);
    }
    return found;
}

json_t *w32_routing_fa91a2_routing_entries_for_sessions(const json_t *in) {
    return rt_entries_for(json_obj_get(in, "rows"), json_obj_get(in, "session_ids"));
}

json_t *w32_routing_fa91a2_routing_entry_for_session(const json_t *in) {
    const json_t *rows = json_obj_get(in, "rows");
    const json_t *want = json_obj_get(in, "session_id");
    size_t i, n = rows ? json_len(rows) : 0;
    for (i = 0; i < n; i++) {
        const json_t *row = json_get(rows, i);
        json_t *entry;
        if (!row || row->type != JSON_OBJECT) continue;
        entry = rt_parse_entry(row);
        if (!entry) continue;
        if (rt_eq(json_obj_get(entry, "session_id"), want)) return entry;
        json_free(entry);
    }
    return json_null();
}

/* user_id as session_for_origin reads it: str(value or "") == str(user_id). */
static char *rt_user_key(const json_t *row) {
    return rt_str_or_empty(json_obj_get(row, "user_id"));
}

json_t *w32_routing_fa91a2_session_for_origin_pick(const json_t *in) {
    const json_t *platform = json_obj_get(in, "platform");
    const json_t *chat = json_obj_get(in, "chat_id");
    const json_t *rows = json_obj_get(in, "rows");
    const json_t *uid = json_obj_get(in, "user_id");
    size_t i, n = rows ? json_len(rows) : 0;

    if (!rt_truthy(platform) || rt_chat_absent(chat)) return json_null();
    if (!rows || rows->type != JSON_ARRAY || n == 0) return json_null();

    if (rt_truthy(uid)) {
        char *want = rt_str(uid);
        for (i = 0; i < n; i++) {
            char *got = rt_user_key(json_get(rows, i));
            int same = want && got && !strcmp(want, got);
            free(got);
            if (same) {
                char *id = rt_str(json_obj_get(json_get(rows, i), "id"));
                json_t *out = json_string(id ? id : "");
                free(id);
                free(want);
                return out;
            }
        }
        free(want);
        if (n > 1) return json_null();
    } else {
        /* more than one distinct stripped user id is an ambiguous origin */
        size_t seen = 0, j;
        char **distinct = (char **)calloc(n, sizeof(char *));
        if (distinct) {
            for (i = 0; i < n; i++) {
                char *raw = rt_user_key(json_get(rows, i));
                char *trim = rt_strip(raw);
                int dup = 0;
                free(raw);
                if (!trim || !trim[0]) { free(trim); continue; }
                for (j = 0; j < seen; j++)
                    if (!strcmp(distinct[j], trim)) { dup = 1; break; }
                if (dup) { free(trim); continue; }
                distinct[seen++] = trim;
            }
            for (j = 0; j < seen; j++) free(distinct[j]);
            free(distinct);
            if (seen > 1) return json_null();
        }
    }
    {
        char *id = rt_str(json_obj_get(json_get(rows, 0), "id"));
        json_t *out = json_string(id ? id : "");
        free(id);
        return out;
    }
}

json_t *w32_routing_fa91a2_orphaned_sessions(const json_t *in) {
    const json_t *orphans = json_obj_get(in, "orphans");
    const json_t *donors = json_obj_get(in, "donors");
    const json_t *owner = json_obj_get(in, "owner");
    const json_t *gapv = json_obj_get(in, "max_gap_s");
    double gap = RT_ORPHAN_GAP_S;
    json_t *records = json_array();
    size_t i, j, n;

    if (gapv && gapv->type != JSON_NULL) {
        double g;
        if (rt_number(gapv, &g)) gap = g;
    }
    n = orphans ? json_len(orphans) : 0;
    for (i = 0; i < n; i++) {
        const json_t *o = json_get(orphans, i);
        const json_t *list = NULL;
        const json_t *donor = NULL;
        const char *evidence = "contiguity";
        char reason[512];
        char *oid;
        json_t *rec;
        if (!o || o->type != JSON_OBJECT) continue;

        oid = rt_str(json_obj_get(o, "id"));
        if (donors && oid) list = json_obj_get(donors, oid);
        reason[0] = '\0';

        if (rt_truthy(json_obj_get(o, "parent_session_id"))) {
            evidence = "lineage";
            if (list && list->type == JSON_ARRAY && json_len(list) > 0) donor = json_get(list, 0);
            if (!donor)
                snprintf(reason, sizeof reason,
                         "parent session carries no gateway identity of this source");
        } else {
            size_t k = (list && list->type == JSON_ARRAY) ? json_len(list) : 0;
            if (k == 0)
                snprintf(reason, sizeof reason,
                         "no keyed predecessor fell quiet within %.0fs of this session's start", gap);
            else if (k > 1)
                snprintf(reason, sizeof reason, "%s", RT_LINEAGE_AMBIGUOUS);
            else
                donor = json_get(list, 0);
        }

        rec = json_object();
        json_set(rec, "orphan_id", rt_copy_or_null(json_obj_get(o, "id")));
        json_set(rec, "source", rt_copy_or_null(json_obj_get(o, "source")));
        json_set(rec, "message_count", rt_copy_or_null(json_obj_get(o, "message_count")));
        json_set(rec, "started_at", rt_copy_or_null(json_obj_get(o, "started_at")));
        json_set(rec, "last_active", rt_copy_or_null(json_obj_get(o, "last_active")));
        json_set(rec, "donor_id", donor ? rt_copy_or_null(json_obj_get(donor, "id")) : json_null());
        json_set(rec, "session_key",
                 donor ? rt_copy_or_null(json_obj_get(donor, "session_key")) : json_null());
        json_set(rec, "evidence", json_string(donor ? evidence : ""));
        json_set(rec, "adoptable", json_bool(donor != NULL));
        json_set(rec, "reason", json_string(reason));
        json_append(records, rec);
        free(oid);
    }
    (void)owner;

    /* Two unkeyed successors claiming one predecessor: at most one continues
     * that chat.  The contest is decided on the snapshot, then applied. */
    n = json_len(records);
    {
        int *contested = (int *)calloc(n ? n : 1, sizeof(int));
        if (contested) {
            for (i = 0; i < n; i++) {
                const json_t *a = json_get(records, i);
                const json_t *da = json_obj_get(a, "donor_id");
                size_t claims = 0;
                if (!json_get_bool(a, "adoptable", false)) continue;
                for (j = 0; j < n; j++)
                    if (rt_eq(da, json_obj_get(json_get(records, j), "donor_id"))) claims++;
                if (claims > 1) contested[i] = 1;
            }
            for (i = 0; i < n; i++) {
                json_t *a;
                if (!contested[i]) continue;
                a = json_get(records, i);
                json_set(a, "adoptable", json_bool(false));
                json_set(a, "reason", json_string(RT_CONTEST_AMBIGUOUS));
            }
            free(contested);
        }
    }
    return records;
}

/* adopt_orphan's gate: a donor is adoptable only when both rows exist, the
 * orphan is unkeyed, the sources agree and the profiles agree under the
 * store's own profile. */
static int rt_adopt_allowed(const json_t *donor, const json_t *orphan, const json_t *owner) {
    if (!donor || !orphan) return 0;
    if (!rt_truthy(json_obj_get(donor, "session_key"))) return 0;
    if (rt_truthy(json_obj_get(orphan, "session_key"))) return 0;
    {
        const json_t *ds = json_obj_get(donor, "source");
        const json_t *os = json_obj_get(orphan, "source");
        char *d = rt_str_or_empty(ds), *o = rt_str_or_empty(os);
        int same = d && o && !strcmp(d, o);
        free(d);
        free(o);
        if (!same) return 0;
    }
    if (owner && owner->type != JSON_NULL) {
        char *op = rt_str(owner);
        char *dp = rt_str_or_empty(json_obj_get(donor, "profile_name"));
        char *orph = rt_str_or_empty(json_obj_get(orphan, "profile_name"));
        int same = op && dp && orph && !strcmp(dp[0] ? dp : op, orph[0] ? orph : op);
        free(op);
        free(dp);
        free(orph);
        if (!same) return 0;
    }
    return 1;
}

json_t *w32_routing_fa91a2_adopt_gate(const json_t *in) {
    const json_t *orphan_id = json_obj_get(in, "orphan_id");
    const json_t *donor_id = json_obj_get(in, "donor_id");
    const json_t *donor = json_obj_get(in, "donor");
    const json_t *orphan = json_obj_get(in, "orphan");
    json_t *writes = json_array();
    json_t *out = json_object();

    if (!rt_truthy(orphan_id) || !rt_truthy(donor_id)) {
        json_set(out, "result", json_bool(false));
        json_set(out, "writes", writes);
        return out;
    }
    {
        char *a = rt_str(orphan_id), *b = rt_str(donor_id);
        int same = a && b && !strcmp(a, b);
        free(a);
        free(b);
        if (same) {
            json_set(out, "result", json_bool(false));
            json_set(out, "writes", writes);
            return out;
        }
    }
    if (!rt_adopt_allowed(donor, orphan, json_obj_get(in, "owner"))) {
        json_set(out, "result", json_bool(false));
        json_set(out, "writes", writes);
        return out;
    }
    {
        json_t *params = json_array();
        json_append(params, rt_copy_or_null(json_obj_get(donor, "session_key")));
        json_append(params, rt_copy_or_null(json_obj_get(donor, "chat_id")));
        json_append(params, rt_copy_or_null(json_obj_get(donor, "chat_type")));
        json_append(params, rt_copy_or_null(json_obj_get(donor, "thread_id")));
        json_append(params, rt_copy_or_null(json_obj_get(donor, "user_id")));
        json_append(params, rt_copy_or_null(json_obj_get(donor, "origin_json")));
        json_append(params, rt_copy_or_null(json_obj_get(donor, "display_name")));
        json_append(params, json_copy(donor_id));
        json_append(params, json_copy(orphan_id));
        json_append(writes, rt_call(SQL_ADOPT, params));
    }
    {
        /* Retire under a reason recovery does not treat as resumable. */
        json_t *params = json_array();
        json_append(params, json_number(rt_now(in)));
        json_append(params, json_copy(donor_id));
        json_append(writes, rt_call(SQL_ADOPT_END, params));
    }
    json_set(out, "result", json_bool(true));
    json_set(out, "writes", writes);
    return out;
}

json_t *w32_routing_fa91a2_bind_guard(const json_t *in) {
    char *profile = rt_normalize_profile(json_obj_get(in, "profile_name"));
    char *chat = rt_str(json_obj_get(in, "chat_id"));
    char *thread = rt_str(json_obj_get(in, "thread_id"));
    char *user = rt_str(json_obj_get(in, "user_id"));
    char *skey = rt_str(json_obj_get(in, "session_key"));
    char *session = rt_str(json_obj_get(in, "session_id"));
    const char *mode = json_get_str(in, "managed_mode", "auto");
    const json_t *expected = json_obj_get(in, "expected_session_id");
    const json_t *existing = json_obj_get(in, "existing");
    double now = rt_now(in);
    json_t *writes = json_array();
    json_t *out = json_object();
    int moved = 0;

    /* expected_session_id: the binding must still name the session the caller saw. */
    if (expected && expected->type != JSON_NULL) {
        const json_t *cur = json_obj_get(in, "current");
        char *want = rt_str(expected);
        char *got = (cur && cur->type == JSON_ARRAY && json_len(cur) > 0)
                        ? rt_str(json_get(cur, 0)) : NULL;
        int same = want && got && !strcmp(want, got);
        free(want);
        free(got);
        if (!same) {
            json_set(out, "result", json_bool(false));
            json_set(out, "writes", writes);
            goto done;
        }
    }

    if (existing && existing->type == JSON_ARRAY && json_len(existing) >= 3) {
        char *lp = rt_str(json_get(existing, 0));
        char *lc = rt_str(json_get(existing, 1));
        char *lt = rt_str(json_get(existing, 2));
        int same = lp && lc && lt && profile && chat && thread &&
                   !strcmp(lp, profile) && !strcmp(lc, chat) && !strcmp(lt, thread);
        if (!same) {
            /* never across profiles, and only an explicit move relinks */
            if (!rt_truthy(json_obj_get(in, "move_session")) ||
                !lp || !profile || strcmp(lp, profile) != 0) {
                json_set(out, "error", json_string("TopicAlreadyLinked"));
                json_set(out, "writes", writes);
                free(lp); free(lc); free(lt);
                goto done;
            }
            moved = 1;
        }
        free(lp);
        free(lc);
        free(lt);
    }

    if (moved) {
        /* The session's old binding goes in the same write. */
        json_t *del = json_array();
        json_append(del, json_string(session));
        json_append(writes, rt_call(SQL_TOPIC_BIND_DELETE, del));
    }
    {
        json_t *params = json_array();
        json_append(params, json_string(profile));
        json_append(params, json_string(chat));
        json_append(params, json_string(thread));
        json_append(params, json_string(user));
        json_append(params, json_string(skey));
        json_append(params, json_string(session));
        json_append(params, json_string(mode));
        json_append(params, json_number(now));
        json_append(params, json_number(now));
        json_append(writes, rt_call(SQL_TOPIC_BIND, params));
    }
    json_set(out, "result", json_bool(true));
    json_set(out, "writes", writes);
done:
    free(profile);
    free(chat);
    free(thread);
    free(user);
    free(skey);
    free(session);
    return out;
}

json_t *w32_routing_fa91a2_replace_entries(const json_t *in) {
    const json_t *entries = json_obj_get(in, "entries");
    const json_t *scope = json_obj_get(in, "routing_scope");
    const json_t *kept = json_obj_get(in, "kept");
    double now = rt_now(in);
    json_t *statements = json_array();
    json_t *rows = json_array();
    json_t *out = json_object();
    size_t i, n = (entries && entries->type == JSON_OBJECT) ? json_object_size(entries) : 0;
    size_t nrows;

    {
        json_t *params = json_array();
        json_append(params, scope ? json_copy(scope) : json_string(""));
        json_append(statements,
                    rt_call("DELETE FROM gateway_routing WHERE scope = ?", params));
    }
    for (i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(entries, i);
        const json_t *v = json_object_get_at(entries, i);
        json_t *row;
        char *ns;
        if (!k || !k[0] || !rt_truthy(v)) continue;
        ns = rt_namespace_of(k);
        row = json_object();
        json_set(row, "scope", scope ? json_copy(scope) : json_string(""));
        json_set(row, "key", json_string(k));
        json_set(row, "entry", json_copy(v));
        json_set(row, "ns", json_string(ns ? ns : "main"));
        json_set(row, "now", json_number(now));
        json_set(row, "horizon", json_number(RT_RETRY_HORIZON_S));
        json_set(row, "retired_prefix", json_string(RT_RETIRED_PREFIX));
        json_set(row, "fence_prefix", json_string(RT_ROUTE_FENCE_PREFIX));
        json_append(rows, row);
        free(ns);
    }
    nrows = json_len(rows);

    /* {row['key'] for row in rows} - kept.  Computed while the rows are still
     * ours: the INSERT statement below takes ownership of the array. */
    {
        json_t *rejected = json_array();
        for (i = 0; i < nrows; i++) {
            const char *k = json_get_str(json_get(rows, i), "key", "");
            size_t j;
            int in_kept = 0, dup = 0;
            for (j = 0; kept && j < json_len(kept); j++) {
                const json_t *x = json_get(kept, j);
                if (x && x->type == JSON_STRING && !strcmp(x->str_val ? x->str_val : "", k)) {
                    in_kept = 1;
                    break;
                }
            }
            if (in_kept) continue;
            for (j = 0; j < json_len(rejected); j++) {
                const char *seen = json_string_value(json_get(rejected, j));
                if (seen && !strcmp(seen, k)) { dup = 1; break; }
            }
            if (!dup) json_append(rejected, json_string(k));
        }
        {
            /* sorted(): the set is reported in code-point order */
            size_t m = json_len(rejected), a, b;
            for (a = 0; a + 1 < m; a++)
                for (b = a + 1; b < m; b++) {
                    const char *x = json_string_value(json_get(rejected, a));
                    const char *y = json_string_value(json_get(rejected, b));
                    if (strcmp(x, y) > 0) {
                        json_t *tmp = json_copy(json_get(rejected, a));
                        json_array_set(rejected, a, json_copy(json_get(rejected, b)));
                        json_array_set(rejected, b, tmp);
                    }
                }
        }
        if (nrows > 0)
            json_append(statements, rt_call(SQL_REPLACE_INSERT, rows));
        else
            json_free(rows);      /* an entry list with nothing insertable */
        json_set(out, "statements", statements);
        json_set(out, "rejected", rejected);
    }
    return out;
}

json_t *w32_routing_fa91a2_prune_outcome(const json_t *in) {
    const json_t *rows = json_obj_get(in, "rows");
    const json_t *lineage = json_obj_get(in, "lineage");
    const json_t *del = json_obj_get(in, "delete");
    const json_t *routing_rows = json_obj_get(in, "routing_rows");
    json_t *ids = json_array();
    json_t *out = json_object();
    size_t i, n = rows ? json_len(rows) : 0;

    /* A candidate survives only when its lineage is itself alone. */
    for (i = 0; i < n; i++) {
        const json_t *row = json_get(rows, i);
        char *sid;
        const json_t *lin;
        int alone = 0;
        if (!row || row->type != JSON_OBJECT) continue;
        /* NeverActiveSession.session_id is the row's id column. */
        sid = rt_str(json_obj_get(row, "id"));
        lin = (lineage && sid) ? json_obj_get(lineage, sid) : NULL;
        if (!lin || lin->type != JSON_ARRAY) {
            alone = 1;                                  /* owned_lineage_ids -> [sid] */
        } else if (json_len(lin) == 1) {
            const json_t *only = json_get(lin, 0);
            alone = only && only->type == JSON_STRING && sid &&
                    !strcmp(only->str_val ? only->str_val : "", sid);
        }
        if (alone) {
            size_t j;
            int dup = 0;
            for (j = 0; j < json_len(ids); j++) {
                const char *seen = json_string_value(json_get(ids, j));
                if (seen && sid && !strcmp(seen, sid)) { dup = 1; break; }
            }
            if (!dup) json_append(ids, json_string(sid));
        }
        free(sid);
    }

    if (json_len(ids) == 0) {
        json_set(out, "sessions_deleted", json_number(0));
        json_set(out, "routing_entries_deleted", json_number(0));
        json_set(out, "sessions_skipped", json_number(0));
        json_set(out, "deleted_ids", json_array());
        json_set(out, "removed_ids", json_array());
        json_free(ids);
        return out;
    }

    {
        const json_t *skipped = json_obj_get(del, "skipped_guarded");
        json_t *routed = rt_entries_for(routing_rows, ids);
        json_t *gone = json_array();
        json_t *removed = json_array();
        size_t c, m;
        int routing_deleted = 0;

        for (i = 0; i < json_len(routed); i++) {
            const json_t *sid = json_get(json_get(routed, i), 2);
            int kept = 0;
            for (c = 0; skipped && c < json_len(skipped); c++) {
                char *s = rt_str(json_get(skipped, c));
                int same = s && sid && sid->type == JSON_STRING &&
                           !strcmp(s, sid->str_val ? sid->str_val : "");
                free(s);
                if (same) { kept = 1; break; }
            }
            if (!kept) routing_deleted++;
        }
        for (i = 0; i < json_len(ids); i++) {
            const char *sid = json_string_value(json_get(ids, i));
            int kept = 0;
            for (c = 0; skipped && c < json_len(skipped); c++) {
                char *s = rt_str(json_get(skipped, c));
                if (s && !strcmp(s, sid)) kept = 1;
                free(s);
                if (kept) break;
            }
            if (!kept) json_append(gone, json_string(sid));
        }
        for (i = 0; del && i < json_len(json_obj_get(del, "removed_ids")); i++) {
            char *s = rt_str(json_get(json_obj_get(del, "removed_ids"), i));
            if (s) json_append(removed, json_string(s));
            free(s);
        }
        /* sorted() over both id tuples */
        for (m = json_len(gone), c = 0; c + 1 < m; c++) {
            size_t d;
            for (d = c + 1; d < m; d++) {
                const char *x = json_string_value(json_get(gone, c));
                const char *y = json_string_value(json_get(gone, d));
                if (strcmp(x, y) > 0) {
                    json_t *tmp = json_copy(json_get(gone, c));
                    json_array_set(gone, c, json_copy(json_get(gone, d)));
                    json_array_set(gone, d, tmp);
                }
            }
        }
        for (m = json_len(removed), c = 0; c + 1 < m; c++) {
            size_t d;
            for (d = c + 1; d < m; d++) {
                const char *x = json_string_value(json_get(removed, c));
                const char *y = json_string_value(json_get(removed, d));
                if (strcmp(x, y) > 0) {
                    json_t *tmp = json_copy(json_get(removed, c));
                    json_array_set(removed, c, json_copy(json_get(removed, d)));
                    json_array_set(removed, d, tmp);
                }
            }
        }
        json_set(out, "sessions_deleted",
                 json_number(del ? json_get_num(del, "deleted", 0) : 0));
        json_set(out, "routing_entries_deleted", json_number(routing_deleted));
        json_set(out, "sessions_skipped", json_number(skipped ? json_len(skipped) : 0));
        json_set(out, "deleted_ids", gone);
        json_set(out, "removed_ids", removed);
        json_free(routed);
        json_free(ids);
    }
    return out;
}
