/*
 * _routes.c - C11 port of upstream persistence/sqlite/gateway/_routes.py
 * (gate w23_routes_70b0a8).
 *
 * PoP: w23_routes_70b0a8_in_window @ persistence/sqlite/gateway/_routes.py:_sql_in_window
 * PoP: w23_routes_70b0a8_freshest_of @ persistence/sqlite/gateway/_routes.py:_sql_freshest_of
 * PoP: w23_routes_70b0a8_last_active @ persistence/sqlite/gateway/_routes.py:_sql_session_last_active
 * PoP: w23_routes_70b0a8_fence_key @ persistence/sqlite/gateway/_routes.py:route_fence_key
 * PoP: w23_routes_70b0a8_namespace @ persistence/gateway/routing.py:route_namespace
 * PoP: w23_routes_70b0a8_retired_params @ persistence/sqlite/gateway/_routes.py:route_retired_params
 *
 * Every exported function is the text the module hands to SQLite: the epoch
 * window, the recency COALESCE, the fence key and the retirement predicate's
 * named parameters.  The connection is a seam the caller owns; this file only
 * decides what the statements say.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_routes_70b0a8.h"

/* -- exact literals ----------------------------------------------------------- */

static const char ROUTES_IN_WINDOW[] = "BETWEEN 0.0 AND 4200000000.0";

static const char ROUTES_EXPIRED[] =
    "json_valid(value) AND json_extract(value, '$.retired_at') < ?";

static const char ROUTES_RECOVERABLE_SQL[] =
    "'agent_close', 'ws_orphan_reap', 'superseded_by_resume', 'startup_orphan_reap'";

static const char ROUTES_DONOR_COLUMNS[] =
    "d.id, d.session_key, d.chat_id, d.chat_type, d.thread_id, "
    "d.user_id, d.origin_json, d.display_name, d.end_reason";

static const char ROUTES_RETIRED_SQL[] =
    "(\n"
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

static const char ROUTES_TOPIC_MODE_DDL[] =
    "\n"
    "                    profile_name TEXT NOT NULL DEFAULT 'default',\n"
    "                    chat_id TEXT NOT NULL,\n"
    "                    user_id TEXT NOT NULL,\n"
    "                    enabled INTEGER NOT NULL DEFAULT 1,\n"
    "                    activated_at REAL NOT NULL,\n"
    "                    updated_at REAL NOT NULL,\n"
    "                    has_topics_enabled INTEGER,\n"
    "                    allows_users_to_create_topics INTEGER,\n"
    "                    capability_checked_at REAL,\n"
    "                    intro_message_id TEXT,\n"
    "                    pinned_message_id TEXT,\n"
    "                    PRIMARY KEY (profile_name, chat_id)\n"
    "                ";

static const char ROUTES_TOPIC_BINDINGS_DDL[] =
    "\n"
    "                    profile_name TEXT NOT NULL DEFAULT 'default',\n"
    "                    chat_id TEXT NOT NULL,\n"
    "                    thread_id TEXT NOT NULL,\n"
    "                    user_id TEXT NOT NULL,\n"
    "                    session_key TEXT NOT NULL,\n"
    "                    session_id TEXT NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,\n"
    "                    managed_mode TEXT NOT NULL DEFAULT 'auto',\n"
    "                    linked_at REAL NOT NULL,\n"
    "                    updated_at REAL NOT NULL,\n"
    "                    PRIMARY KEY (profile_name, chat_id, thread_id)\n"
    "                ";

static const char ROUTES_TOPIC_MODE_COLUMNS[] =
    "profile_name, chat_id, user_id, enabled, activated_at, updated_at, "
    "has_topics_enabled, allows_users_to_create_topics, capability_checked_at, "
    "intro_message_id, pinned_message_id";

static const char ROUTES_TOPIC_BINDINGS_COLUMNS[] =
    "profile_name, chat_id, thread_id, user_id, session_key, session_id, "
    "managed_mode, linked_at, updated_at";

/* -- string buffer ------------------------------------------------------------ */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} rs_buf;

static char *rs_dup(const char *s) {
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int rs_grow(rs_buf *b, size_t need) {
    size_t cap;
    char *p;
    if (b->len + need + 1 <= b->cap) return 1;
    cap = b->cap ? b->cap : 128;
    while (cap < b->len + need + 1) cap *= 2;
    p = (char *)realloc(b->buf, cap);
    if (!p) return 0;
    b->buf = p;
    b->cap = cap;
    return 1;
}

static int rs_add(rs_buf *b, const char *s) {
    size_t n = strlen(s);
    if (!rs_grow(b, n)) return 0;
    memcpy(b->buf + b->len, s, n + 1);
    b->len += n;
    return 1;
}

static int rs_addf(rs_buf *b, const char *fmt, ...) {
    va_list ap, ap2;
    int n;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) {
        va_end(ap2);
        return 0;
    }
    if (!rs_grow(b, (size_t)n)) {
        va_end(ap2);
        return 0;
    }
    vsnprintf(b->buf + b->len, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    b->len += (size_t)n;
    return 1;
}

/* Steal the buffer as a malloc'd NUL-terminated string. */
static char *rs_take_cstr(rs_buf *b) {
    char *out = b->buf;
    if (!out) {
        out = (char *)malloc(1);
        if (out) out[0] = '\0';
    }
    b->buf = NULL;
    b->len = 0;
    b->cap = 0;
    return out;
}

/* -- builders ----------------------------------------------------------------- */

char *w23_routes_70b0a8_in_window(const char *expr) {
    rs_buf b = {NULL, 0, 0};
    int ok = rs_add(&b, "(SELECT _win.v FROM (SELECT ") &&
             rs_addf(&b, "%s", expr ? expr : "None") &&
             rs_add(&b, " AS v) _win WHERE _win.v ") &&
             rs_add(&b, ROUTES_IN_WINDOW) &&
             rs_add(&b, ")");
    if (!ok) {
        free(b.buf);
        return NULL;
    }
    return rs_take_cstr(&b);
}

char *w23_routes_70b0a8_freshest_of(const char *activity,
                                    const char *session_id_expr,
                                    const char *started) {
    rs_buf b = {NULL, 0, 0};
    char *fallback;
    int ok;

    fallback = w23_routes_70b0a8_in_window(started ? started : "None");
    if (!fallback) return NULL;
    ok = rs_add(&b, "COALESCE((SELECT MAX(_act_v.v) FROM (SELECT ") &&
         rs_addf(&b, "%s", activity ? activity : "None") &&
         rs_add(&b, " AS v UNION ALL SELECT (SELECT MAX(_act_m.timestamp) FROM messages _act_m"
                    " INDEXED BY idx_messages_session WHERE _act_m.session_id = ") &&
         rs_addf(&b, "%s", session_id_expr ? session_id_expr : "None") &&
         rs_add(&b, " AND _act_m.timestamp ") &&
         rs_add(&b, ROUTES_IN_WINDOW) &&
         rs_add(&b, ")) _act_v WHERE _act_v.v ") &&
         rs_add(&b, ROUTES_IN_WINDOW) &&
         rs_add(&b, "), ") &&
         rs_add(&b, fallback) &&
         rs_add(&b, ")");
    free(fallback);
    if (!ok) {
        free(b.buf);
        return NULL;
    }
    return rs_take_cstr(&b);
}

char *w23_routes_70b0a8_last_active(const char *alias) {
    const char *a = alias ? alias : "s";
    rs_buf b = {NULL, 0, 0};
    char *activity = NULL, *started = NULL, *sid = NULL, *out = NULL;

    if (!rs_addf(&b, "%s.last_activity_at", a)) goto done;
    activity = rs_take_cstr(&b);
    if (!activity) goto done;
    if (!rs_addf(&b, "%s.started_at", a)) goto done;
    started = rs_take_cstr(&b);
    if (!started) goto done;
    if (!rs_addf(&b, "%s.id", a)) goto done;
    sid = rs_take_cstr(&b);
    if (!sid) goto done;
    out = w23_routes_70b0a8_freshest_of(activity, sid, started);
done:
    free(activity);
    free(started);
    free(sid);
    free(b.buf);
    return out;
}

char *w23_routes_70b0a8_namespace(const char *key) {
    const char *p1, *p2;
    size_t n;
    char *out;

    if (!key) return rs_dup("main");
    p1 = strchr(key, ':');
    if (!p1) return rs_dup("main");
    if ((size_t)(p1 - key) != 5 || memcmp(key, "agent", 5) != 0) return rs_dup("main");
    p2 = strchr(p1 + 1, ':');
    if (!p2) return rs_dup("main");
    n = (size_t)(p2 - (p1 + 1));
    if (n == 0) return rs_dup("main");
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, p1 + 1, n);
    out[n] = '\0';
    return out;
}

char *w23_routes_70b0a8_fence_key(const char *ns, const char *session_id) {
    rs_buf b = {NULL, 0, 0};
    int ok = rs_add(&b, W23_ROUTES_70B0A8_FENCE_PREFIX) &&
             rs_addf(&b, "%s", ns ? ns : "None") &&
             rs_add(&b, ":") &&
             rs_addf(&b, "%s", session_id ? session_id : "None");
    if (!ok) {
        free(b.buf);
        return NULL;
    }
    return rs_take_cstr(&b);
}

/* Python str() of the session_key slot: numbers print as integers, bools as
 * True/False, None stays absent ("key or ''" reads main). */
static const char *rs_key_text(const json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) return NULL;
    if (v->type == JSON_STRING) return v->str_val;
    if (v->type == JSON_BOOL) return v->bool_val ? "True" : "False";
    if (v->type == JSON_NUMBER) {
        long long n = (long long)v->num_val;
        if ((double)n == v->num_val) snprintf(buf, cap, "%lld", n);
        else snprintf(buf, cap, "%g", v->num_val);
        return buf;
    }
    return NULL;
}

json_t *w23_routes_70b0a8_retired_params(const json_t *req) {
    json_t *out = json_object();
    json_t *v;
    char num_buf[64];
    char *ns;

    if (!out) return NULL;

    v = req && req->type == JSON_OBJECT ? json_obj_get(req, "entry_json") : NULL;
    json_set(out, "entry", v ? json_copy(v) : json_null());

    v = req && req->type == JSON_OBJECT ? json_obj_get(req, "session_key") : NULL;
    ns = w23_routes_70b0a8_namespace(rs_key_text(v, num_buf, sizeof(num_buf)));
    json_set(out, "ns", ns ? json_string(ns) : json_string("main"));
    free(ns);

    v = req && req->type == JSON_OBJECT ? json_obj_get(req, "now") : NULL;
    json_set(out, "now", v ? json_copy(v) : json_null());
    json_set(out, "horizon", json_number((double)W23_ROUTES_70B0A8_RETRY_HORIZON_S));
    json_set(out, "retired_prefix", json_string(W23_ROUTES_70B0A8_RETIRED_PREFIX));
    json_set(out, "fence_prefix", json_string(W23_ROUTES_70B0A8_FENCE_PREFIX));
    return out;
}

json_t *w23_routes_70b0a8_constants(void) {
    static const char *const recoverable[] = {
        "agent_close", "ws_orphan_reap", "superseded_by_resume", "startup_orphan_reap"
    };
    json_t *out = json_object();
    json_t *reasons, *tables, *columns, *ddl;
    size_t i, n;

    if (!out) return NULL;
    json_set(out, "fence_prefix", json_string(W23_ROUTES_70B0A8_FENCE_PREFIX));
    json_set(out, "retired_prefix", json_string(W23_ROUTES_70B0A8_RETIRED_PREFIX));
    json_set(out, "in_window", json_string(ROUTES_IN_WINDOW));
    json_set(out, "expired", json_string(ROUTES_EXPIRED));
    json_set(out, "horizon", json_number((double)W23_ROUTES_70B0A8_RETRY_HORIZON_S));
    json_set(out, "recoverable_sql", json_string(ROUTES_RECOVERABLE_SQL));
    json_set(out, "donor_columns", json_string(ROUTES_DONOR_COLUMNS));
    json_set(out, "retired_sql", json_string(ROUTES_RETIRED_SQL));

    reasons = json_array();
    n = sizeof(recoverable) / sizeof(recoverable[0]);
    for (i = 0; i < n; i++) json_append(reasons, json_string(recoverable[i]));
    json_set(out, "recoverable", reasons);

    tables = json_array();
    columns = json_array();
    ddl = json_array();
    json_append(tables, json_string("telegram_dm_topic_mode"));
    json_append(columns, json_string(ROUTES_TOPIC_MODE_COLUMNS));
    json_append(ddl, json_string(ROUTES_TOPIC_MODE_DDL));
    json_append(tables, json_string("telegram_dm_topic_bindings"));
    json_append(columns, json_string(ROUTES_TOPIC_BINDINGS_COLUMNS));
    json_append(ddl, json_string(ROUTES_TOPIC_BINDINGS_DDL));
    json_set(out, "topic_tables", tables);
    json_set(out, "topic_columns", columns);
    json_set(out, "topic_ddl", ddl);
    return out;
}
