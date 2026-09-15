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

/* ── hermes_state.py: session ops + export + title + yolo + vacuum ── */

/* PoP: resolved_max_export_messages @ hermes_state.py:resolved_max_export_messages */
int hermes_state_resolved_max_export_messages(void) {
    return 10000; /* default limit; reads from config in Python */
}

/* PoP: format_session_db_unavailable @ hermes_state.py:format_session_db_unavailable */
char *hermes_state_format_session_db_unavailable(const char *prefix) {
    if (!prefix) prefix = "Session database not available";
    const char *cause = getenv("HERMES_STATE_DB_ERROR");
    if (!cause || !*cause) {
        char *result = malloc(strlen(prefix) + 2);
        sprintf(result, "%s.", prefix);
        return result;
    }
    size_t len = strlen(prefix) + strlen(cause) + 16;
    char *result = malloc(len);
    snprintf(result, len, "%s: %s.", prefix, cause);
    return result;
}

/* PoP: update_session_model @ hermes_state.py:update_session_model */
void hermes_state_update_session_model(sqlite3 *conn, const char *session_id,
        const char *model, const char *provider) {
    if (!conn || !session_id) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE sessions SET model = ?, system_prompt = NULL, system_prompt_hash = NULL WHERE id = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, model, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
}

/* PoP: session_yolo_enabled @ hermes_state.py:session_yolo_enabled */
bool hermes_state_session_yolo_enabled(const char *model_config_json) {
    if (!model_config_json) return false;
    return strstr(model_config_json, "\"yolo_mode\":true") != NULL ||
           strstr(model_config_json, "\"yolo_mode\": true") != NULL;
}

/* PoP: set_session_yolo @ hermes_state.py:set_session_yolo */
void hermes_state_set_session_yolo(sqlite3 *conn, const char *session_id, bool enabled) {
    if (!conn || !session_id) return;
    /* Merge yolo_mode into model_config */
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE sessions SET model_config = json_set(COALESCE(model_config, '{}'), '$.yolo_mode', ?) WHERE id = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int(st, 1, enabled ? 1 : 0);
        sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
}

/* PoP: set_session_title @ hermes_state.py:set_session_title */
bool hermes_state_set_session_title(sqlite3 *conn, const char *session_id, const char *title) {
    if (!conn || !session_id) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, "UPDATE sessions SET title = ? WHERE id = ?", -1, &st, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    bool changed = sqlite3_changes(conn) > 0;
    sqlite3_finalize(st);
    return changed;
}

/* PoP: assert_export_safe @ hermes_state.py:assert_export_safe */
int hermes_state_assert_export_safe(sqlite3 *conn, const char *session_id, int max_messages) {
    if (!conn || !session_id) return 0;
    if (max_messages <= 0) return 0; /* guard disabled */
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT COUNT(*) FROM (SELECT 1 FROM messages WHERE session_id = ? AND active = 1 LIMIT ?)",
            -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 2, max_messages + 1);
    int count = 0;
    if (sqlite3_step(st) == SQLITE_ROW) count = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return count;
}

/* PoP: sanitize_context @ hermes_state.py:sanitize_context */
char *hermes_state_sanitize_context(const char *content) {
    if (!content) return strdup("");
    return strdup(content); /* simplified: full impl sanitizes context */
}

/* PoP: vacuum @ hermes_state.py:vacuum */
int hermes_state_vacuum(sqlite3 *conn) {
    if (!conn) return 0;
    /* Merge FTS5 segments first, then VACUUM */
    sqlite3_exec(conn, "INSERT INTO messages_fts(messages_fts) VALUES('optimize')", NULL, NULL, NULL);
    int rc = sqlite3_exec(conn, "VACUUM", NULL, NULL, NULL);
    return rc == SQLITE_OK ? 1 : 0;
}
