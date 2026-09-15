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

/* ── hermes_state.py: message ops + repair + delete ── */

/* PoP: latest_message_row_id @ hermes_state.py:latest_message_row_id */
long long hermes_state_latest_message_row_id(sqlite3 *conn, const char *session_id) {
    if (!conn || !session_id) return -1;
    sqlite3_stmt *st = NULL;
    long long id = -1;
    if (sqlite3_prepare_v2(conn, "SELECT MAX(id) FROM messages WHERE session_id = ?", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) id = sqlite3_column_int64(st, 0);
        sqlite3_finalize(st);
    }
    return id;
}

/* PoP: set_message_reaction @ hermes_state.py:set_message_reaction */
void hermes_state_set_message_reaction(sqlite3 *conn, const char *session_id, long long row_id, const char *reaction) {
    if (!conn || !session_id) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, "UPDATE messages SET reaction = ? WHERE session_id = ? AND id = ?", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, reaction, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(st, 3, row_id);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
}

/* PoP: get_message_role @ hermes_state.py:get_message_role */
char *hermes_state_get_message_role(sqlite3 *conn, const char *session_id, long long row_id) {
    if (!conn || !session_id) return NULL;
    sqlite3_stmt *st = NULL;
    char *role = NULL;
    if (sqlite3_prepare_v2(conn, "SELECT role FROM messages WHERE session_id = ? AND id = ?", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(st, 2, row_id);
        if (sqlite3_step(st) == SQLITE_ROW) {
            const char *r = (const char *)sqlite3_column_text(st, 0);
            if (r) role = strdup(r);
        }
        sqlite3_finalize(st);
    }
    return role;
}

/* PoP: encode_content @ hermes_state.py:encode_content */
char *hermes_state_encode_content(const char *content) {
    if (!content) return strdup("\"\"");
    size_t len = strlen(content);
    char *encoded = malloc(len * 2 + 3);
    if (!encoded) return strdup("\"\"");
    size_t j = 0;
    encoded[j++] = '"';
    for (size_t i = 0; i < len; i++) {
        if (content[i] == '"' || content[i] == '\\') encoded[j++] = '\\';
        encoded[j++] = content[i];
    }
    encoded[j++] = '"';
    encoded[j] = '\0';
    return encoded;
}

/* PoP: encode_display_metadata @ hermes_state.py:encode_display_metadata */
char *hermes_state_encode_display_metadata(const char *metadata) {
    if (!metadata) return strdup("null");
    return hermes_state_encode_content(metadata);
}

/* PoP: repair_ledger_path @ hermes_state.py:repair_ledger_path */
char *hermes_state_repair_ledger_path(const char *db_path) {
    if (!db_path) return NULL;
    size_t len = strlen(db_path) + 32;
    char *path = malloc(len);
    snprintf(path, len, "%s.repair-attempts.json", db_path);
    return path;
}

/* PoP: read_repair_ledger @ hermes_state.py:read_repair_ledger */
char *hermes_state_read_repair_ledger(const char *db_path) {
    if (!db_path) return strdup("{}");
    char *path = hermes_state_repair_ledger_path(db_path);
    if (!path) return strdup("{}");
    FILE *f = fopen(path, "r");
    free(path);
    if (!f) return strdup("{}");
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *content = malloc(size + 1);
    if (!content) { fclose(f); return strdup("{}"); }
    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);
    return content;
}

/* PoP: repair_snapshot_timeout_seconds @ hermes_state.py:repair_snapshot_timeout_seconds */
double hermes_state_repair_snapshot_timeout_seconds(const char *source_path) {
    if (!source_path) return 30.0;
    struct stat st;
    long long total = 0;
    if (stat(source_path, &st) == 0) total = st.st_size;
    static const char *suffixes[] = {"-wal", "-shm", "-journal"};
    char sidecar[4096];
    for (int i = 0; i < 3; i++) {
        snprintf(sidecar, sizeof(sidecar), "%s%s", source_path, suffixes[i]);
        struct stat sst;
        if (stat(sidecar, &sst) == 0) total += sst.st_size;
    }
    double timeout = total / (10.0 * 1024 * 1024); /* 10MB/s min throughput */
    if (timeout < 30.0) timeout = 30.0;
    return timeout;
}

/* PoP: delete_session @ hermes_state.py:delete_session */
bool hermes_state_delete_session(sqlite3 *conn, const char *session_id) {
    if (!conn || !session_id) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, "DELETE FROM sessions WHERE id = ?", -1, &st, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    bool changed = sqlite3_changes(conn) > 0;
    sqlite3_finalize(st);
    return changed;
}
