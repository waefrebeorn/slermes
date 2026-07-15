/*
 * kanban_notify.c — notification subscriptions for hermes_cli/kanban_db.py
 *
 * Concern: the gateway kanban-notifier's per-task subscription cursor
 * (add/remove/advance/rewind) plus the read/claim helpers
 * (list_notify_subs / unseen_events_for_sub / claim_unseen_events_for_sub).
 * Reuses the write-txn primitives. Self-contained.
 *
 * Minimal includes. No god header.
 *
 * PoP: exact port. Semantic source of truth = hermes_cli/kanban_db.py
 *      (add_notify_sub / remove_notify_sub / advance_notify_cursor /
 *       rewind_notify_cursor / list_notify_subs / unseen_events_for_sub /
 *       claim_unseen_events_for_sub).
 */

#include "kanban_db.h"
#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* PoP: kdb_add_notify_sub @ hermes_cli/kanban_db.py:add_notify_sub */
int kdb_add_notify_sub(sqlite3 *conn, const char *task_id, const char *platform,
                          const char *chat_id, const char *thread_id,
                          const char *user_id, const char *notifier_profile)
{
    if (!conn || !task_id || !platform || !chat_id) return 0;
    long now = kdb_now();
    if (kdb_write_begin(conn) != 0) return 0;
    int ok = 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "INSERT OR IGNORE INTO kanban_notify_subs "
            "(task_id, platform, chat_id, thread_id, user_id, notifier_profile, created_at) "
            "VALUES (?,?,?,?,?,?,?)", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, platform, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, chat_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 4, thread_id ? thread_id : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 5, user_id ? user_id : NULL, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 6, notifier_profile ? notifier_profile : NULL, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(st, 7, now);
        ok = (sqlite3_step(st) == SQLITE_DONE);
        sqlite3_finalize(st);
    }
    if (ok && notifier_profile) {
        sqlite3_stmt *up = NULL;
        if (sqlite3_prepare_v2(conn,
                "UPDATE kanban_notify_subs SET notifier_profile=? "
                "WHERE task_id=? AND platform=? AND chat_id=? AND thread_id=? "
                "AND (notifier_profile IS NULL OR notifier_profile='')", -1, &up, NULL) == SQLITE_OK) {
            sqlite3_bind_text(up, 1, notifier_profile, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(up, 2, task_id, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(up, 3, platform, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(up, 4, chat_id, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(up, 5, thread_id ? thread_id : "", -1, SQLITE_TRANSIENT);
            sqlite3_step(up); sqlite3_finalize(up);
        }
    }
    kdb_write_end(conn, 1);
    return ok;
}

/* PoP: kdb_remove_notify_sub @ hermes_cli/kanban_db.py:remove_notify_sub */
int kdb_remove_notify_sub(sqlite3 *conn, const char *task_id, const char *platform,
                             const char *chat_id, const char *thread_id)
{
    if (!conn || !task_id || !platform || !chat_id) return 0;
    if (kdb_write_begin(conn) != 0) return 0;
    sqlite3_stmt *st = NULL;
    int ok = 0;
    if (sqlite3_prepare_v2(conn,
            "DELETE FROM kanban_notify_subs WHERE task_id=? AND platform=? AND chat_id=? AND thread_id=?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, platform, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, chat_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 4, thread_id ? thread_id : "", -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(st) == SQLITE_DONE && sqlite3_changes(conn) > 0);
        sqlite3_finalize(st);
    }
    kdb_write_end(conn, 1);
    return ok;
}

/* PoP: kdb_advance_notify_cursor @ hermes_cli/kanban_db.py:advance_notify_cursor */
int kdb_advance_notify_cursor(sqlite3 *conn, const char *task_id, const char *platform,
                                 const char *chat_id, const char *thread_id, int new_cursor)
{
    if (!conn || !task_id || !platform || !chat_id) return 0;
    if (kdb_write_begin(conn) != 0) return 0;
    sqlite3_stmt *st = NULL;
    int ok = 0;
    if (sqlite3_prepare_v2(conn,
            "UPDATE kanban_notify_subs SET last_event_id=? "
            "WHERE task_id=? AND platform=? AND chat_id=? AND thread_id=?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int(st, 1, new_cursor);
        sqlite3_bind_text(st, 2, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, platform, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 4, chat_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 5, thread_id ? thread_id : "", -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(st) == SQLITE_DONE);
        sqlite3_finalize(st);
    }
    kdb_write_end(conn, 1);
    return ok;
}

/* PoP: kdb_rewind_notify_cursor @ hermes_cli/kanban_db.py:rewind_notify_cursor */
int kdb_rewind_notify_cursor(sqlite3 *conn, const char *task_id, const char *platform,
                                const char *chat_id, const char *thread_id,
                                int claimed_cursor, int old_cursor)
{
    if (!conn || !task_id || !platform || !chat_id) return 0;
    if (kdb_write_begin(conn) != 0) return 0;
    sqlite3_stmt *st = NULL;
    int ok = 0;
    if (sqlite3_prepare_v2(conn,
            "UPDATE kanban_notify_subs SET last_event_id=? "
            "WHERE task_id=? AND platform=? AND chat_id=? AND thread_id=? AND last_event_id=?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int(st, 1, old_cursor);
        sqlite3_bind_text(st, 2, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, platform, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 4, chat_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 5, thread_id ? thread_id : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(st, 6, claimed_cursor);
        ok = (sqlite3_step(st) == SQLITE_DONE && sqlite3_changes(conn) > 0);
        sqlite3_finalize(st);
    }
    kdb_write_end(conn, 1);
    return ok;
}

/* PoP: kdb_list_notify_subs @ hermes_cli/kanban_db.py:list_notify_subs */
char **kdb_list_notify_subs(sqlite3 *conn, const char *task_id, int *out_n)
{
    if (!conn) { if (out_n) *out_n = 0; return NULL; }
    sqlite3_stmt *st = NULL;
    int n = 0, cap = 8;
    char **rows = malloc(sizeof(char*) * cap);
    const char *q = task_id
        ? "SELECT * FROM kanban_notify_subs WHERE task_id=?"
        : "SELECT * FROM kanban_notify_subs";
    if (sqlite3_prepare_v2(conn, q, -1, &st, NULL) == SQLITE_OK) {
        if (task_id) sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(st) == SQLITE_ROW) {
            const char *tid = (const char*)sqlite3_column_text(st, 0);
            const char *plat = (const char*)sqlite3_column_text(st, 1);
            const char *cid = (const char*)sqlite3_column_text(st, 2);
            const char *thr = (const char*)sqlite3_column_text(st, 3);
            int leid = sqlite3_column_type(st, 8) == SQLITE_NULL ? 0 : (int)sqlite3_column_int64(st, 8);
            char buf[1024];
            snprintf(buf, sizeof(buf),
                     "{\"task_id\":%s,\"platform\":%s,\"chat_id\":%s,\"thread_id\":%s,"
                     "\"last_event_id\":%d}",
                     tid ? tid : "", plat ? plat : "", cid ? cid : "", thr ? thr : "", leid);
            if (n >= cap) { cap *= 2; rows = realloc(rows, sizeof(char*) * cap); }
            rows[n++] = strdup(buf);
        }
        sqlite3_finalize(st);
    }
    rows = realloc(rows, sizeof(char*) * (n + 1));
    rows[n] = NULL;
    if (out_n) *out_n = n;
    return rows;
}

/* PoP: kdb_unseen_events_for_sub @ hermes_cli/kanban_db.py:unseen_events_for_sub */
int kdb_unseen_events_for_sub(sqlite3 *conn, const char *task_id,
                               const char *platform, const char *chat_id,
                               const char *thread_id, char **kinds, int n_kinds,
                               int *out_new_cursor, kanban_event_t ***out_events, int *out_n)
{
    if (!conn || !task_id || !platform || !chat_id) {
        if (out_new_cursor) *out_new_cursor = 0;
        if (out_events) *out_events = NULL;
        if (out_n) *out_n = 0;
        return 0;
    }
    sqlite3_stmt *row = NULL;
    int cursor = 0;
    if (sqlite3_prepare_v2(conn,
            "SELECT last_event_id FROM kanban_notify_subs "
            "WHERE task_id=? AND platform=? AND chat_id=? AND thread_id=?",
            -1, &row, NULL) == SQLITE_OK) {
        sqlite3_bind_text(row, 1, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(row, 2, platform, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(row, 3, chat_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(row, 4, thread_id ? thread_id : "", -1, SQLITE_TRANSIENT);
        if (sqlite3_step(row) == SQLITE_ROW)
            cursor = sqlite3_column_type(row, 0) == SQLITE_NULL ? 0 : (int)sqlite3_column_int64(row, 0);
        sqlite3_finalize(row);
    } else return 0;

    /* Build the query with optional kind filter. */
    char qbuf[2048];
    int off = snprintf(qbuf, sizeof(qbuf),
        "SELECT * FROM task_events WHERE task_id=? AND id>?");
    if (n_kinds > 0) {
        off += snprintf(qbuf + off, sizeof(qbuf) - off, " AND kind IN (");
        for (int i = 0; i < n_kinds; i++)
            off += snprintf(qbuf + off, sizeof(qbuf) - off, i ? ",?" : "?");
        off += snprintf(qbuf + off, sizeof(qbuf) - off, ")");
    }
    off += snprintf(qbuf + off, sizeof(qbuf) - off, " ORDER BY id ASC");

    sqlite3_stmt *st = NULL;
    int n = 0, cap = 8, max_id = cursor;
    kanban_event_t **ev = malloc(sizeof(kanban_event_t*) * cap);
    if (sqlite3_prepare_v2(conn, qbuf, -1, &st, NULL) == SQLITE_OK) {
        int p = 1;
        sqlite3_bind_text(st, p++, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(st, p++, cursor);
        for (int i = 0; i < n_kinds; i++)
            sqlite3_bind_text(st, p++, kinds[i], -1, SQLITE_TRANSIENT);
        while (sqlite3_step(st) == SQLITE_ROW) {
            kanban_event_t *e = kdb_event_from_row(st);
            if (!e) continue;
            int id = kdb_event_id(e);
            if (id > max_id) max_id = id;
            if (n >= cap) { cap *= 2; ev = realloc(ev, sizeof(kanban_event_t*) * cap); }
            ev[n++] = e;
        }
        sqlite3_finalize(st);
    }
    ev = realloc(ev, sizeof(kanban_event_t*) * (n + 1));
    ev[n] = NULL;
    if (out_new_cursor) *out_new_cursor = max_id;
    if (out_events) *out_events = ev; else { kdb_event_list_free(ev); }
    if (out_n) *out_n = n;
    return n;
}

/* PoP: kdb_claim_unseen_events_for_sub @ hermes_cli/kanban_db.py:claim_unseen_events_for_sub */
int kdb_claim_unseen_events_for_sub(sqlite3 *conn, const char *task_id,
                                     const char *platform, const char *chat_id,
                                     const char *thread_id, char **kinds, int n_kinds,
                                     int *out_old_cursor, int *out_new_cursor,
                                     kanban_event_t ***out_events, int *out_n)
{
    if (!conn || !task_id || !platform || !chat_id) {
        if (out_old_cursor) *out_old_cursor = 0;
        if (out_new_cursor) *out_new_cursor = 0;
        if (out_events) *out_events = NULL;
        if (out_n) *out_n = 0;
        return 0;
    }
    if (kdb_write_begin(conn) != 0) return 0;
    int old_cursor = 0, new_cursor = 0, n = 0;
    kanban_event_t **ev = NULL;
    sqlite3_stmt *row = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT last_event_id FROM kanban_notify_subs "
            "WHERE task_id=? AND platform=? AND chat_id=? AND thread_id=?",
            -1, &row, NULL) == SQLITE_OK) {
        sqlite3_bind_text(row, 1, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(row, 2, platform, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(row, 3, chat_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(row, 4, thread_id ? thread_id : "", -1, SQLITE_TRANSIENT);
        if (sqlite3_step(row) == SQLITE_ROW)
            old_cursor = sqlite3_column_type(row, 0) == SQLITE_NULL ? 0 : (int)sqlite3_column_int64(row, 0);
        sqlite3_finalize(row);
    }
    n = kdb_unseen_events_for_sub(conn, task_id, platform, chat_id, thread_id,
                                  kinds, n_kinds, &new_cursor, &ev, out_n);
    if (n > 0) {
        sqlite3_stmt *up = NULL;
        if (sqlite3_prepare_v2(conn,
                "UPDATE kanban_notify_subs SET last_event_id=? "
                "WHERE task_id=? AND platform=? AND chat_id=? AND thread_id=? AND last_event_id=?",
                -1, &up, NULL) == SQLITE_OK) {
            sqlite3_bind_int(up, 1, new_cursor);
            sqlite3_bind_text(up, 2, task_id, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(up, 3, platform, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(up, 4, chat_id, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(up, 5, thread_id ? thread_id : "", -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(up, 6, old_cursor);
            sqlite3_step(up);
            sqlite3_finalize(up);
        }
    }
    kdb_write_end(conn, 1);
    if (out_old_cursor) *out_old_cursor = old_cursor;
    if (out_new_cursor) *out_new_cursor = new_cursor;
    if (out_events) *out_events = ev; else { kdb_event_list_free(ev); }
    return n;
}
