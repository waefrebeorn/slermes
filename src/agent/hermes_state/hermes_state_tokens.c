#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <sqlite3.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_state.py: token counts + session status + compression cooldown + kanban ── */

/* PoP: flush_token_counts @ hermes_state.py:flush_token_counts */
void hermes_state_flush_token_counts(sqlite3 *conn, const char *session_id) {
    if (!conn || !session_id) return;
    /* Flush pending token count deltas */
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE sessions SET message_count = (SELECT COUNT(*) FROM messages WHERE session_id = ? AND active = 1) WHERE id = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
}

/* PoP: classify_session_status @ hermes_state.py:classify_session_status */
const char *hermes_state_classify_session_status(const char *role, int has_tool_calls, const char *finish_reason) {
    if (!role) return "empty";
    if (strcmp(role, "assistant") == 0) {
        if (finish_reason && strcmp(finish_reason, "stop") == 0) return "complete";
        if (finish_reason && strcmp(finish_reason, "tool_calls") == 0) return "complete";
        if (finish_reason && strcmp(finish_reason, "length") == 0) return "interrupted";
        return "error";
    }
    if (strcmp(role, "tool") == 0) return "complete";
    return "empty";
}

/* PoP: restore_compression_failure_cooldown_row @ hermes_state.py:restore_compression_failure_cooldown_row */
void hermes_state_restore_compression_failure_cooldown_row(sqlite3 *conn, const char *session_id) {
    if (!conn || !session_id) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE sessions SET compression_failure_cooldown_until = NULL WHERE id = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
}

/* PoP: clear_compression_failure_cooldown @ hermes_state.py:clear_compression_failure_cooldown */
void hermes_state_clear_compression_failure_cooldown(sqlite3 *conn, const char *session_id) {
    hermes_state_restore_compression_failure_cooldown_row(conn, session_id);
}

/* PoP: record_compression_failure_cooldown @ hermes_state.py:record_compression_failure_cooldown */
void hermes_state_record_compression_failure_cooldown(sqlite3 *conn, const char *session_id, float cooldown_seconds) {
    if (!conn || !session_id) return;
    time_t until = time(NULL) + (time_t)cooldown_seconds;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE sessions SET compression_failure_cooldown_until = ? WHERE id = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_double(st, 1, (double)until);
        sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
}

/* PoP: get_compression_failure_cooldown_row @ hermes_state.py:get_compression_failure_cooldown_row */
double hermes_state_get_compression_failure_cooldown_row(sqlite3 *conn, const char *session_id) {
    if (!conn || !session_id) return 0.0;
    sqlite3_stmt *st = NULL;
    double until = 0.0;
    if (sqlite3_prepare_v2(conn,
            "SELECT compression_failure_cooldown_until FROM sessions WHERE id = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            until = sqlite3_column_double(st, 0);
        }
        sqlite3_finalize(st);
    }
    return until;
}

/* PoP: retag_kanban_worker_sessions @ hermes_state.py:retag_kanban_worker_sessions */
int hermes_state_retag_kanban_worker_sessions(sqlite3 *conn, const char *old_tag, const char *new_tag) {
    if (!conn || !old_tag || !new_tag) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE sessions SET source = ? WHERE source = ?",
            -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, new_tag, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, old_tag, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    int changes = sqlite3_changes(conn);
    sqlite3_finalize(st);
    return changes;
}
