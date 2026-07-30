/*
 * port_web_server_sessions_admin.c — sessions admin cluster.
 * Faithful port of SessionDB.count_empty_sessions, delete_empty_sessions,
 * message_count, session_count (hermes_state.py) and the get_session_stats
 * aggregation (hermes_cli/web_server.py) against the real sqlite store.
 */

#include "web_server_sessions_admin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "sqlite3.h"

/* hermes_state.py module constants:
 * _BRANCH_CHILD_SQL / _LISTABLE_CHILD_SQL / _delegate_from_json */
#define BRANCH_CHILD_SQL(a) \
    "json_extract(COALESCE(" a ".model_config, '{}'), '$._branched_from') IS NOT NULL" \
    " OR EXISTS (SELECT 1 FROM sessions p" \
    "            WHERE p.id = " a ".parent_session_id" \
    "            AND p.end_reason = 'branched'" \
    "            AND " a ".started_at >= p.ended_at)"

#define LISTABLE_CHILD_SQL \
    "(s.parent_session_id IS NULL OR " BRANCH_CHILD_SQL("s") ")"

#define DELEGATE_IS_NULL \
    "json_extract(COALESCE(s.model_config, '{}'), '$._delegate_from') IS NULL"

static sqlite3 *open_db(const char *path, bool rw) {
    sqlite3 *db = NULL;
    int flags = rw ? SQLITE_OPEN_READWRITE : SQLITE_OPEN_READONLY;
    if (sqlite3_open_v2(path, &db, flags, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return NULL;
    }
    return db;
}

static int scalar_int(sqlite3 *db, const char *sql) {
    sqlite3_stmt *st = NULL;
    int out = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK &&
        sqlite3_step(st) == SQLITE_ROW)
        out = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return out;
}

/* ── count_empty_sessions ───────────────────────────────────────────────── */
/* PoP: ws_sessions_count_empty @ hermes_state.py:count_empty_sessions */
int ws_sessions_count_empty(const char *db_path) {
    sqlite3 *db = open_db(db_path, false);
    if (!db) return 0;
    int n = scalar_int(db,
        "SELECT COUNT(*) FROM sessions WHERE message_count = 0 "
        "AND ended_at IS NOT NULL AND archived = 0");
    sqlite3_close(db);
    return n;
}

/* ── delete_empty_sessions ──────────────────────────────────────────────── */
/* PoP: ws_sessions_delete_empty @ hermes_state.py:delete_empty_sessions */
int ws_sessions_delete_empty(const char *db_path) {
    sqlite3 *db = open_db(db_path, true);
    if (!db) return -1;

    sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);

    /* Select candidate ids first. */
    char **ids = NULL;
    size_t n = 0, cap = 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db,
            "SELECT id FROM sessions WHERE message_count = 0 "
            "AND ended_at IS NOT NULL AND archived = 0",
            -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            if (n == cap) {
                cap = cap ? cap * 2 : 16;
                ids = realloc(ids, cap * sizeof *ids);
            }
            const unsigned char *t = sqlite3_column_text(st, 0);
            ids[n++] = strdup(t ? (const char *)t : "");
        }
    }
    sqlite3_finalize(st);

    if (n == 0) {
        sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
        free(ids);
        sqlite3_close(db);
        return 0;
    }

    /* Orphan children of the kill list. */
    size_t sql_len = 128 + n * 2;
    char *sql = malloc(sql_len);
    strcpy(sql, "UPDATE sessions SET parent_session_id = NULL "
                "WHERE parent_session_id IN (");
    for (size_t i = 0; i < n; i++)
        strcat(sql, i + 1 < n ? "?," : "?");
    strcat(sql, ")");
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
        for (size_t i = 0; i < n; i++)
            sqlite3_bind_text(st, (int)i + 1, ids[i], -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
    }
    sqlite3_finalize(st);
    free(sql);

    /* Delete messages (paranoia) + session rows. */
    for (size_t i = 0; i < n; i++) {
        if (sqlite3_prepare_v2(db, "DELETE FROM messages WHERE session_id = ?",
                               -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, ids[i], -1, SQLITE_TRANSIENT);
            sqlite3_step(st);
        }
        sqlite3_finalize(st);
        if (sqlite3_prepare_v2(db, "DELETE FROM sessions WHERE id = ?",
                               -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, ids[i], -1, SQLITE_TRANSIENT);
            sqlite3_step(st);
        }
        sqlite3_finalize(st);
    }
    sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);

    for (size_t i = 0; i < n; i++) free(ids[i]);
    free(ids);
    sqlite3_close(db);
    return (int)n;
}

/* ── message_count ──────────────────────────────────────────────────────── */
/* PoP: ws_sessions_message_count @ hermes_state.py:message_count */
int ws_sessions_message_count(const char *db_path, const char *session_id) {
    sqlite3 *db = open_db(db_path, false);
    if (!db) return 0;
    int out = 0;
    sqlite3_stmt *st = NULL;
    if (session_id && *session_id) {
        if (sqlite3_prepare_v2(db,
                "SELECT COUNT(*) FROM messages WHERE session_id = ?",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(st) == SQLITE_ROW) out = sqlite3_column_int(st, 0);
        }
        sqlite3_finalize(st);
    } else {
        out = scalar_int(db, "SELECT COUNT(*) FROM messages");
    }
    sqlite3_close(db);
    return out;
}

/* ── session_count ──────────────────────────────────────────────────────── */
/* PoP: ws_sessions_session_count @ hermes_state.py:session_count */
int ws_sessions_session_count(const char *db_path,
                              const ws_session_count_opts_t *opts) {
    sqlite3 *db = open_db(db_path, false);
    if (!db) return 0;

    char sql[2048];
    char where[1536] = "";
    int nclauses = 0;
    const char *params[4];
    int nparams = 0;

#define ADD_CLAUSE(c) do { \
        if (nclauses++) strcat(where, " AND "); \
        strcat(where, c); \
    } while (0)

    if (opts && opts->exclude_children) {
        ADD_CLAUSE(LISTABLE_CHILD_SQL);
        ADD_CLAUSE(DELEGATE_IS_NULL);
    }
    if (opts && opts->source) {
        ADD_CLAUSE("s.source = ?");
        params[nparams++] = opts->source;
    }
    if (opts && opts->min_message_count > 0)
        ADD_CLAUSE("s.message_count >= ?");
    if (opts && opts->archived_only)
        ADD_CLAUSE("s.archived = 1");
    else if (!opts || !opts->include_archived)
        ADD_CLAUSE("s.archived = 0");
#undef ADD_CLAUSE

    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM sessions s%s%s",
             nclauses ? " WHERE " : "", where);

    sqlite3_stmt *st = NULL;
    int out = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
        int bind = 1;
        for (int i = 0; i < nparams; i++)
            sqlite3_bind_text(st, bind++, params[i], -1, SQLITE_TRANSIENT);
        if (opts && opts->min_message_count > 0)
            sqlite3_bind_int(st, bind++, opts->min_message_count);
        if (sqlite3_step(st) == SQLITE_ROW) out = sqlite3_column_int(st, 0);
    }
    sqlite3_finalize(st);
    sqlite3_close(db);
    return out;
}

/* ── get_session_stats ──────────────────────────────────────────────────── */
/* PoP: ws_sessions_stats @ hermes_cli/web_server.py:get_session_stats */
json_t *ws_sessions_stats(const char *db_path) {
    ws_session_count_opts_t all = {.include_archived = true};
    ws_session_count_opts_t store = {.include_archived = false};
    ws_session_count_opts_t arch = {.archived_only = true};

    json_t *out = json_object();
    json_set(out, "total",
             json_number(ws_sessions_session_count(db_path, &all)));
    json_set(out, "active_store",
             json_number(ws_sessions_session_count(db_path, &store)));
    json_set(out, "archived",
             json_number(ws_sessions_session_count(db_path, &arch)));
    json_set(out, "messages",
             json_number(ws_sessions_message_count(db_path, NULL)));

    /* by_source over list_sessions_rich(include_archived=True,
     * compact_rows=True) rows: default include_children=False → the
     * listable-child + delegate filters; no archived clause. */
    json_t *by_source = json_object();
    sqlite3 *db = open_db(db_path, false);
    if (db) {
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(db,
                "SELECT s.source, COUNT(*) FROM sessions s WHERE "
                LISTABLE_CHILD_SQL " AND " DELEGATE_IS_NULL
                " GROUP BY s.source",
                -1, &st, NULL) == SQLITE_OK) {
            while (sqlite3_step(st) == SQLITE_ROW) {
                const unsigned char *src = sqlite3_column_text(st, 0);
                const char *key =
                    (src && ((const char *)src)[0]) ? (const char *)src : "cli";
                int cnt = sqlite3_column_int(st, 1);
                json_t *prev = json_object_get(by_source, key);
                double base = prev && prev->type == JSON_NUMBER ? prev->num_val : 0;
                json_set(by_source, key, json_number(base + cnt));
            }
        }
        sqlite3_finalize(st);
        sqlite3_close(db);
    }
    json_set(out, "by_source", by_source);
    return out;
}
