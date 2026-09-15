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

/* ── hermes_state.py: session runtime lock + maintenance + query ── */

/* PoP: update_session_runtime_lock @ hermes_state.py:update_session_runtime_lock */
void hermes_state_update_session_runtime_lock(sqlite3 *conn, const char *session_id,
        const char *model, const char *provider, const char *model_options,
        const char *route_source, int confirmed) {
    if (!conn || !session_id) return;
    /* Merge browser_model_lock into model_config JSON */
    const char *lock_json = "{\"browser_model_lock\":{\"provider\":\"";
    char merged[4096];
    snprintf(merged, sizeof(merged),
        "%s%s\",\"model\":\"%s\",\"route_source\":\"%s\",\"confirmed\":%d}}",
        lock_json, provider ? provider : "", model ? model : "",
        route_source ? route_source : "", confirmed);
    (void)merged;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE sessions SET model = COALESCE(?, model), system_prompt = NULL WHERE id = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, model, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
}

/* PoP: prune_empty_ghost_sessions @ hermes_state.py:prune_empty_ghost_sessions */
int hermes_state_prune_empty_ghost_sessions(sqlite3 *conn) {
    if (!conn) return 0;
    time_t cutoff = time(NULL) - 86400;
    sqlite3_stmt *st = NULL;
    int pruned = 0;
    if (sqlite3_prepare_v2(conn,
            "DELETE FROM sessions WHERE source = 'tui' AND title IS NULL "
            "AND ended_at IS NOT NULL AND started_at < ? "
            "AND NOT EXISTS (SELECT 1 FROM messages WHERE messages.session_id = sessions.id)",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_double(st, 1, (double)cutoff);
        sqlite3_step(st);
        pruned = sqlite3_changes(conn);
        sqlite3_finalize(st);
    }
    return pruned;
}

/* PoP: finalize_orphaned_compression_sessions @ hermes_state.py:finalize_orphaned_compression_sessions */
int hermes_state_finalize_orphaned_compression_sessions(sqlite3 *conn) {
    if (!conn) return 0;
    time_t cutoff = time(NULL) - 604800; /* 7 days */
    time_t now = time(NULL);
    sqlite3_stmt *st = NULL;
    int finalized = 0;
    if (sqlite3_prepare_v2(conn,
            "UPDATE sessions SET ended_at = ?, end_reason = 'orphaned_compression' "
            "WHERE api_call_count = 0 AND end_reason IS NULL AND ended_at IS NULL "
            "AND started_at < ? AND parent_session_id IS NOT NULL "
            "AND EXISTS (SELECT 1 FROM sessions p WHERE p.id = sessions.parent_session_id "
            "AND p.end_reason = 'compression' AND p.ended_at IS NOT NULL) "
            "AND EXISTS (SELECT 1 FROM messages m WHERE m.session_id = sessions.id)",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_double(st, 1, (double)now);
        sqlite3_bind_double(st, 2, (double)cutoff);
        sqlite3_step(st);
        finalized = sqlite3_changes(conn);
        sqlite3_finalize(st);
    }
    return finalized;
}

/* PoP: delete_session_if_empty @ hermes_state.py:delete_session_if_empty */
bool hermes_state_delete_session_if_empty(sqlite3 *conn, const char *session_id, const char *end_reason) {
    if (!conn || !session_id) return false;
    /* Check if session has messages */
    sqlite3_stmt *st = NULL;
    bool is_empty = false;
    if (sqlite3_prepare_v2(conn,
            "SELECT COUNT(*) FROM messages WHERE session_id = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            is_empty = sqlite3_column_int(st, 0) == 0;
        }
        sqlite3_finalize(st);
    }
    if (!is_empty) return false;
    /* Delete the session */
    if (sqlite3_prepare_v2(conn,
            "DELETE FROM sessions WHERE id = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
    return true;
}

/* PoP: get_session_title @ hermes_state.py:get_session_title */
char *hermes_state_get_session_title(sqlite3 *conn, const char *session_id) {
    if (!conn || !session_id) return NULL;
    sqlite3_stmt *st = NULL;
    char *title = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT title FROM sessions WHERE id = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            const char *t = (const char *)sqlite3_column_text(st, 0);
            if (t) title = strdup(t);
        }
        sqlite3_finalize(st);
    }
    return title;
}

/* PoP: default_db_path @ hermes_state.py:_default_db_path */
char *hermes_state_default_db_path(void) {
    static char path[4096];
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "~";
    snprintf(path, sizeof(path), "%s/state.db", home);
    return path;
}

/* PoP: session_count @ hermes_state.py:session_count */
long long hermes_state_session_count(sqlite3 *conn, const char *source,
        int exclude_children, const char *exclude_sources) {
    if (!conn) return 0;
    char sql[2048] = "SELECT COUNT(*) FROM sessions WHERE 1=1";
    if (source) strcat(sql, " AND source = ?");
    if (exclude_children) {
        strcat(sql, " AND parent_session_id IS NULL");
        strcat(sql, " AND json_extract(COALESCE(model_config, '{}'), '$._branched_from') IS NULL");
    }
    if (exclude_sources) {
        strcat(sql, " AND source NOT IN (");
        strcat(sql, exclude_sources);
        strcat(sql, ")");
    }
    sqlite3_stmt *st = NULL;
    long long count = 0;
    if (sqlite3_prepare_v2(conn, sql, -1, &st, NULL) == SQLITE_OK) {
        int p = 1;
        if (source) sqlite3_bind_text(st, p++, source, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            count = sqlite3_column_int64(st, 0);
        }
        sqlite3_finalize(st);
    }
    return count;
}

/* PoP: message_count @ hermes_state.py:message_count */
long long hermes_state_message_count(sqlite3 *conn, const char *session_id) {
    if (!conn) return 0;
    sqlite3_stmt *st = NULL;
    long long count = 0;
    if (session_id) {
        if (sqlite3_prepare_v2(conn, "SELECT COUNT(*) FROM messages WHERE session_id = ?", -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(st) == SQLITE_ROW) count = sqlite3_column_int64(st, 0);
            sqlite3_finalize(st);
        }
    } else {
        if (sqlite3_prepare_v2(conn, "SELECT COUNT(*) FROM messages", -1, &st, NULL) == SQLITE_OK) {
            if (sqlite3_step(st) == SQLITE_ROW) count = sqlite3_column_int64(st, 0);
            sqlite3_finalize(st);
        }
    }
    return count;
}
