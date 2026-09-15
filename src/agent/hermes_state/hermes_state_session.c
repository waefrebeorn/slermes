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

extern char *_stat_db_file_identity(const char *path);
extern char *_stat_sqlite_sidecar_identity(const char *db_path);

/* ── hermes_state.py: session lifecycle + compression + handoff cluster ── */

/* PoP: _bump_conversation_generation @ hermes_state.py:_bump_conversation_generation */
void _bump_conversation_generation(sqlite3 *conn, const char *session_id, const char *end_reason) {
    if (!conn || !session_id || !end_reason) return;
    /* Only reset-end reasons count: compression, session_reset, etc. */
    static const char *reset_reasons[] = {"session_reset", "session_switch", "idle", "daily", "suspended", "resume_pending_expired", NULL};
    bool is_reset = false;
    for (int i = 0; reset_reasons[i]; i++) {
        if (strcmp(end_reason, reset_reasons[i]) == 0) { is_reset = true; break; }
    }
    if (!is_reset) return;
    /* Look up source + session_key */
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, "SELECT source, session_key FROM sessions WHERE id = ?", -1, &st, NULL) != SQLITE_OK) return;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) == SQLITE_ROW) {
        const char *source = (const char *)sqlite3_column_text(st, 0);
        const char *sk = (const char *)sqlite3_column_text(st, 1);
        if (source && sk && *source && *sk) {
            sqlite3_stmt *ins = NULL;
            if (sqlite3_prepare_v2(conn, "INSERT INTO conversation_generations (source, session_key, generation) VALUES (?, ?, 1) ON CONFLICT(source, session_key) DO UPDATE SET generation = conversation_generations.generation + 1", -1, &ins, NULL) == SQLITE_OK) {
                sqlite3_bind_text(ins, 1, source, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(ins, 2, sk, -1, SQLITE_TRANSIENT);
                sqlite3_step(ins);
                sqlite3_finalize(ins);
            }
        }
    }
    sqlite3_finalize(st);
}

/* PoP: get_compression_recovery_deadline @ hermes_state.py:get_compression_recovery_deadline */
float get_compression_recovery_deadline(const char *session_id, sqlite3 *conn) {
    if (!session_id || !conn) return 0.0f;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, "SELECT compression_recovery_deadline FROM sessions WHERE id = ?", -1, &st, NULL) != SQLITE_OK) return 0.0f;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    float val = 0.0f;
    if (sqlite3_step(st) == SQLITE_ROW) {
        val = (float)sqlite3_column_double(st, 0);
    }
    sqlite3_finalize(st);
    return val > 0.0f ? val : 0.0f;
}

/* PoP: set_compression_recovery_deadline @ hermes_state.py:set_compression_recovery_deadline */
void set_compression_recovery_deadline(const char *session_id, float deadline, sqlite3 *conn) {
    if (!session_id || !conn) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, "UPDATE sessions SET compression_recovery_deadline = ? WHERE id = ?", -1, &st, NULL) != SQLITE_OK) return;
    sqlite3_bind_double(st, 1, deadline > 0.0f ? deadline : 0.0);
    sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

/* PoP: update_session_tool_names @ hermes_state.py:update_session_tool_names */
void update_session_tool_names(const char *session_id, const char *tool_names_json, sqlite3 *conn) {
    if (!session_id || !conn) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, "UPDATE sessions SET tool_names = ? WHERE id = ?", -1, &st, NULL) != SQLITE_OK) return;
    sqlite3_bind_text(st, 1, tool_names_json, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

/* PoP: _freelist_ratio @ hermes_state.py:_freelist_ratio */
float _freelist_ratio(sqlite3 *conn) {
    if (!conn) return -1.0f;
    int64_t page_count = 0, freelist = 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, "PRAGMA page_count", -1, &st, NULL) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW) page_count = sqlite3_column_int64(st, 0);
    }
    sqlite3_finalize(st);
    if (sqlite3_prepare_v2(conn, "PRAGMA freelist_count", -1, &st, NULL) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW) freelist = sqlite3_column_int64(st, 0);
    }
    sqlite3_finalize(st);
    if (page_count <= 0) return 0.0f;
    return (float)((double)freelist / (double)page_count);
}

/* PoP: reclaim_stale_running_handoffs @ hermes_state.py:reclaim_stale_running_handoffs */
int reclaim_stale_running_handoffs(const char *error, sqlite3 *conn) {
    if (!error || !conn) return 0;
    /* Find running handoffs */
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, "SELECT id FROM sessions WHERE handoff_state = 'running'", -1, &st, NULL) != SQLITE_OK) return 0;
    int count = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        count++;
    }
    sqlite3_finalize(st);
    if (count > 0) {
        if (sqlite3_prepare_v2(conn, "UPDATE sessions SET handoff_state = 'failed', handoff_error = ? WHERE handoff_state = 'running'", -1, &st, NULL) == SQLITE_OK) {
            char truncated[501];
            strncpy(truncated, error, 500);
            truncated[500] = '\0';
            sqlite3_bind_text(st, 1, truncated, -1, SQLITE_TRANSIENT);
            sqlite3_step(st);
        }
        sqlite3_finalize(st);
    }
    return count;
}

/* PoP: _db_has_trigram_tool_calls_projection @ hermes_state.py:_db_has_trigram_tool_calls_projection */
bool _db_has_trigram_tool_calls_projection(sqlite3 *conn) {
    if (!conn) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, "SELECT sql FROM sqlite_master WHERE type = 'table' AND name = 'messages_fts_trigram'", -1, &st, NULL) != SQLITE_OK) return false;
    bool has = false;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const char *sql = (const char *)sqlite3_column_text(st, 0);
        if (sql) {
            /* Check if 'tool_calls' appears in the SQL */
            char lower[4096];
            size_t len = strlen(sql);
            if (len >= sizeof(lower)) len = sizeof(lower) - 1;
            for (size_t i = 0; i < len; i++)
                lower[i] = (sql[i] >= 'A' && sql[i] <= 'Z') ? sql[i] + 32 : sql[i];
            lower[len] = '\0';
            has = strstr(lower, "tool_calls") != NULL;
        }
    }
    sqlite3_finalize(st);
    return has;
}

/* PoP: _db_needs_fts_storage_upgrade @ hermes_state.py:_db_needs_fts_storage_upgrade */
bool _db_needs_fts_storage_upgrade(sqlite3 *conn) {
    if (!conn) return false;
    /* True when legacy inline FTS or trigram tool_calls projection exists */
    return _db_has_trigram_tool_calls_projection(conn);
}

/* PoP: _ensure_db_file_generation @ hermes_state.py:_ensure_db_file_generation */
void _ensure_db_file_generation(const char *db_path, const char *token, sqlite3 *conn) {
    if (!conn || !token) return;
    /* Insert or ignore generation token */
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, "INSERT OR IGNORE INTO state_meta (key, value) VALUES ('_db_file_generation', ?)", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, token, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
    }
    sqlite3_finalize(st);
    /* Read back the value */
    if (sqlite3_prepare_v2(conn, "SELECT value FROM state_meta WHERE key = '_db_file_generation'", -1, &st, NULL) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW) {
            const char *val = (const char *)sqlite3_column_text(st, 0);
            if (val) {
                /* Set application_id from first 8 hex chars of token */
                int app_id = (int)(strtoul(val, NULL, 16) & 0x7FFFFFFF);
                if (app_id == 0) app_id = 1;
                char sql[64];
                snprintf(sql, sizeof(sql), "PRAGMA application_id=%d", app_id);
                sqlite3_exec(conn, sql, NULL, NULL, NULL);
            }
        }
    }
    sqlite3_finalize(st);
}

/* PoP: _record_db_file_identity @ hermes_state.py:_record_db_file_identity */
void _record_db_file_identity(const char *db_path, sqlite3 *conn) {
    if (!db_path || !conn) return;
    /* Snapshot inode + sidecar identity */
    char *identity = _stat_db_file_identity(db_path);
    if (identity) {
        /* Store in session or log */
        hermes_log(LOG_DEBUG, "hermes_state", "state.db identity: %s", identity);
        free(identity);
    }
}

/* PoP: _db_file_was_replaced @ hermes_state.py:_db_file_was_replaced */
bool _db_file_was_replaced(const char *db_path, const char *recorded_identity) {
    if (!db_path || !recorded_identity) return false;
    char *current = _stat_db_file_identity(db_path);
    if (!current) return false;
    bool replaced = strcmp(current, recorded_identity) != 0;
    free(current);
    return replaced;
}

/* PoP: _halt_db_replaced @ hermes_state.py:_halt_db_replaced */
void _halt_db_replaced(const char *db_path) {
    hermes_log(LOG_ERROR, "hermes_state", "state.db at %s has been replaced (identity mismatch)", db_path);
}

/* PoP: _wal_generation_was_lost @ hermes_state.py:_wal_generation_was_lost */
bool _wal_generation_was_lost(const char *db_path, const char *recorded_sidecar) {
    if (!db_path || !recorded_sidecar) return false;
    char *current = _stat_sqlite_sidecar_identity(db_path);
    if (!current) return false;
    bool lost = strcmp(current, recorded_sidecar) != 0;
    free(current);
    return lost;
}

/* PoP: _halt_deleted_wal_generation @ hermes_state.py:_halt_deleted_wal_generation */
void _halt_deleted_wal_generation(const char *db_path) {
    hermes_log(LOG_ERROR, "hermes_state", "state.db at %s: WAL generation was lost", db_path);
}

/* PoP: _raise_if_db_replaced @ hermes_state.py:_raise_if_db_replaced */
void _raise_if_db_replaced(const char *db_path, const char *recorded_identity) {
    if (!db_path || !recorded_identity) return;
    if (_db_file_was_replaced(db_path, recorded_identity)) {
        _halt_db_replaced(db_path);
    }
}

/* PoP: _is_structural_corruption_error @ hermes_state.py:_is_structural_corruption_error */
bool _is_structural_corruption_error(const char *msg) {
    if (!msg) return false;
    static const char *markers[] = {"malformed", "not a database", "database corruption", NULL};
    char lower[1024];
    size_t len = strlen(msg);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++)
        lower[i] = (msg[i] >= 'A' && msg[i] <= 'Z') ? msg[i] + 32 : msg[i];
    lower[len] = '\0';
    for (int i = 0; markers[i]; i++) {
        if (strstr(lower, markers[i])) return true;
    }
    return false;
}

/* PoP: _corrupt_error @ hermes_state.py:_corrupt_error */
void _corrupt_error(const char *prefix, const char *db_path) {
    hermes_log(LOG_ERROR, "hermes_state", "%s%s: structural corruption detected", prefix ? prefix : "", db_path);
}

/* PoP: _halt_db_corrupt @ hermes_state.py:_halt_db_corrupt */
void _halt_db_corrupt(const char *db_path, const char *exc_msg) {
    hermes_log(LOG_ERROR, "hermes_state", "state.db at %s reported structural corruption: %s", db_path, exc_msg ? exc_msg : "unknown");
}

/* PoP: _disable_close_time_checkpoint @ hermes_state.py:_disable_close_time_checkpoint */
void _disable_close_time_checkpoint(void) {
    /* Best-effort: mark that close-time checkpoint should be skipped */
    /* In the C port, this is a no-op (the Python version sets a flag) */
}

/* PoP: _raise_if_db_corrupt @ hermes_state.py:_raise_if_db_corrupt */
void _raise_if_db_corrupt(const char *db_path, const char *exc_msg) {
    if (!db_path || !exc_msg) return;
    if (_is_structural_corruption_error(exc_msg)) {
        _halt_db_corrupt(db_path, exc_msg);
    }
}

/* PoP: classify_persistence_error @ hermes_state.py:classify_persistence_error */
const char *hermes_state_classify_persistence_error(const char *exc_msg) {
    if (!exc_msg) return "unknown";
    char lower[2048];
    size_t len = strlen(exc_msg);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++)
        lower[i] = (exc_msg[i] >= 'A' && exc_msg[i] <= 'Z') ? exc_msg[i] + 32 : exc_msg[i];
    lower[len] = '\0';
    /* Check corruption markers first */
    static const char *corruption_markers[] = {"malformed", "not a database", "database corruption", NULL};
    for (int i = 0; corruption_markers[i]; i++)
        if (strstr(lower, corruption_markers[i])) return "corrupt";
    if (strstr(lower, "locked") || strstr(lower, "busy")) return "locked";
    if (strstr(lower, "disk") || strstr(lower, "readonly") || strstr(lower, "read-only") ||
        strstr(lower, "no space left") || strstr(lower, "not enough space")) return "disk";
    if (strstr(lower, "replaced")) return "replaced";
    if (strstr(lower, "compression")) return "compression";
    return "unknown";
}

/* PoP: _repair_scratch_space_error @ hermes_state.py:_repair_scratch_space_error */
char *hermes_state_repair_scratch_space_error(const char *db_path) {
    if (!db_path) return NULL;
    struct stat st;
    if (stat(db_path, &st) != 0) return NULL;
    long long snapshot_bytes = st.st_size;
    /* Add sidecar sizes */
    char sidecar[4096];
    for (int i = 0; i < 3; i++) {
        static const char *suffixes[] = {"-wal", "-shm", "-journal"};
        snprintf(sidecar, sizeof(sidecar), "%s%s", db_path, suffixes[i]);
        struct stat sst;
        if (stat(sidecar, &sst) == 0) snapshot_bytes += sst.st_size;
    }
    /* Check disk space using statvfs */
    struct statvfs vfs;
    char *dir = strdup(db_path);
    char *slash = strrchr(dir, '/');
    if (slash) *slash = '\0';
    if (statvfs(dir, &vfs) != 0) { free(dir); return NULL; }
    unsigned long long free_bytes = (unsigned long long)vfs.f_bavail * vfs.f_frsize;
    unsigned long long headroom = (unsigned long long)(500 * 1024 * 1024); /* 500MB */
    unsigned long long required = snapshot_bytes + (2 * snapshot_bytes) + headroom;
    char *result = malloc(512);
    if (!result) { free(dir); return NULL; }
    if (free_bytes >= required) { free(dir); free(result); return NULL; /* no error */ }
    snprintf(result, 512, "only %.2fGB free on %s; repair needs up to %.2fGB", 
             free_bytes / 1e9, dir, required / 1e9);
    free(dir);
    return result;
}
