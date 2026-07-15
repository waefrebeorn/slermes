/*
 * kanban_notify.c — notification subscriptions for hermes_cli/kanban_db.py
 *
 * Concern: the gateway kanban-notifier's per-task subscription cursor
 * (add/remove/advance/rewind). Reuses the write-txn primitives. Self-contained.
 *
 * Minimal includes. No god header.
 *
 * PoP: exact port. Semantic source of truth = hermes_cli/kanban_db.py
 *      (add_notify_sub / remove_notify_sub / advance_notify_cursor /
 *       rewind_notify_cursor).
 * PoP: kdb_add_notify_sub @ hermes_cli/kanban_db.py:add_notify_sub
 * PoP: kdb_remove_notify_sub @ hermes_cli/kanban_db.py:remove_notify_sub
 * PoP: kdb_advance_notify_cursor @ hermes_cli/kanban_db.py:advance_notify_cursor
 * PoP: kdb_rewind_notify_cursor @ hermes_cli/kanban_db.py:rewind_notify_cursor
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
                                 const char *chat_id, const char *thread_id, long new_cursor)
{
    if (!conn || !task_id || !platform || !chat_id) return 0;
    if (kdb_write_begin(conn) != 0) return 0;
    sqlite3_stmt *st = NULL;
    int ok = 0;
    if (sqlite3_prepare_v2(conn,
            "UPDATE kanban_notify_subs SET last_event_id=? "
            "WHERE task_id=? AND platform=? AND chat_id=? AND thread_id=?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(st, 1, new_cursor);
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
                                long claimed_cursor, long old_cursor)
{
    if (!conn || !task_id || !platform || !chat_id) return 0;
    if (kdb_write_begin(conn) != 0) return 0;
    sqlite3_stmt *st = NULL;
    int ok = 0;
    if (sqlite3_prepare_v2(conn,
            "UPDATE kanban_notify_subs SET last_event_id=? "
            "WHERE task_id=? AND platform=? AND chat_id=? AND thread_id=? AND last_event_id=?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(st, 1, old_cursor);
        sqlite3_bind_text(st, 2, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, platform, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 4, chat_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 5, thread_id ? thread_id : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(st, 6, claimed_cursor);
        ok = (sqlite3_step(st) == SQLITE_DONE && sqlite3_changes(conn) > 0);
        sqlite3_finalize(st);
    }
    kdb_write_end(conn, 1);
    return ok;
}
