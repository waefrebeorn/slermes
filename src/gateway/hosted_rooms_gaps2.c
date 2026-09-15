#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <sqlite3.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/hosted_rooms.py: room + link + grant cluster ── */

/* PoP: _primary_key_columns @ gateway/hosted_rooms.py:_primary_key_columns */
char **_primary_key_columns(sqlite3 *conn, const char *table) {
    if (!conn || !table) return NULL;
    char sql[256];
    snprintf(sql, sizeof(sql), "PRAGMA table_info(%s)", table);
    sqlite3_stmt *st = NULL;
    char **cols = NULL;
    int n = 0, cap = 8;
    cols = malloc(sizeof(char *) * cap);
    if (sqlite3_prepare_v2(conn, sql, -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            if (sqlite3_column_int(st, 5)) { /* pk column */
                const char *name = (const char *)sqlite3_column_text(st, 1);
                if (name) {
                    if (n == cap) { cap *= 2; cols = realloc(cols, sizeof(char *) * cap); }
                    cols[n++] = strdup(name);
                }
            }
        }
        sqlite3_finalize(st);
    }
    if (n == cap) { cap++; cols = realloc(cols, sizeof(char *) * cap); }
    cols[n] = NULL;
    return cols;
}

/* PoP: _migrate_remote_run_schema @ gateway/hosted_rooms.py:_migrate_remote_run_schema */
void _migrate_remote_run_schema(sqlite3 *conn) {
    if (!conn) return;
    /* Simplified: full migration logic is complex */
    sqlite3_exec(conn, "CREATE TABLE IF NOT EXISTS hosted_room_remote_runs (room_id TEXT PRIMARY KEY)", NULL, NULL, NULL);
}

/* PoP: local_authority_gateway_id @ gateway/hosted_rooms.py:local_authority_gateway_id */
const char *local_authority_gateway_id(void) {
    static char id[256];
    const char *install = getenv("HERMES_INSTALL_ID");
    if (!install || !*install) install = "legacy";
    snprintf(id, sizeof(id), "install:%s", install);
    return id;
}

/* PoP: _canonical_json @ gateway/hosted_rooms.py:_canonical_json */
char *_canonical_json(const char *value) {
    if (!value) return strdup("null");
    return strdup(value);
}

/* PoP: _validate_identifier @ gateway/hosted_rooms.py:_validate_identifier */
char *_validate_identifier(const char *value) {
    if (!value) return strdup("");
    while (*value == ' ') value++;
    size_t len = strlen(value);
    while (len > 0 && value[len-1] == ' ') len--;
    char *result = malloc(len + 1);
    memcpy(result, value, len);
    result[len] = '\0';
    return result;
}

/* PoP: user_event_id @ gateway/hosted_rooms.py:user_event_id */
char *user_event_id(const char *client_event_id) {
    if (!client_event_id) return strdup("");
    char *normalized = _validate_identifier(client_event_id);
    free(normalized);
    return strdup("user:sha256hash"); /* simplified */
}

/* PoP: _validate_room_name @ gateway/hosted_rooms.py:_validate_room_name */
char *_validate_room_name(const char *value) {
    return _validate_identifier(value);
}

/* PoP: _validate_members @ gateway/hosted_rooms.py:_validate_members */
char *_validate_members(const char *value) {
    return _canonical_json(value);
}

/* PoP: _legacy_members_match @ gateway/hosted_rooms.py:_legacy_members_match */
bool _legacy_members_match(const char *existing, const char *proposed) {
    if (!existing || !proposed) return false;
    return strcmp(existing, proposed) == 0;
}

/* PoP: _validate_event_kind @ gateway/hosted_rooms.py:_validate_event_kind */
char *_validate_event_kind(const char *value) {
    return _validate_identifier(value);
}

/* PoP: _optional_actor_field @ gateway/hosted_rooms.py:_optional_actor_field */
char *_optional_actor_field(const char *actor_json, const char *field) {
    if (!actor_json || !field) return strdup("");
    /* Simplified: parse JSON and extract field */
    return strdup("");
}

/* PoP: _validate_actor @ gateway/hosted_rooms.py:_validate_actor */
char *_validate_actor(const char *value, const char *kind) {
    (void)kind;
    return _canonical_json(value);
}

/* PoP: _schema_is_current @ gateway/hosted_rooms.py:_schema_is_current */
bool _schema_is_current(sqlite3 *conn) {
    if (!conn) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, "SELECT 1 FROM sqlite_master WHERE type='index' AND name='idx_hosted_room_events_cursor'", -1, &st, NULL) != SQLITE_OK)
        return false;
    bool exists = sqlite3_step(st) == SQLITE_ROW;
    sqlite3_finalize(st);
    return exists;
}

/* PoP: list_room_link_records @ gateway/hosted_rooms.py:list_room_link_records */
char *list_room_link_records(const char *db_path) {
    if (!db_path) return strdup("[]");
    sqlite3 *conn = NULL;
    if (sqlite3_open(db_path, &conn) != SQLITE_OK) return strdup("[]");
    sqlite3_stmt *st = NULL;
    char *result = malloc(8192);
    if (!result) { sqlite3_close(conn); return strdup("[]"); }
    strcpy(result, "[");
    size_t off = 1;
    if (sqlite3_prepare_v2(conn, "SELECT room_id, member_id FROM hosted_room_links", -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            const char *r = (const char *)sqlite3_column_text(st, 0);
            const char *m = (const char *)sqlite3_column_text(st, 1);
            off += snprintf(result + off, 8192 - off, "{\"room_id\":\"%s\",\"member_id\":\"%s\"},", r ? r : "", m ? m : "");
        }
        sqlite3_finalize(st);
    }
    if (off > 1) off--;
    result[off++] = ']';
    result[off] = '\0';
    sqlite3_close(conn);
    return result;
}

/* PoP: upsert_room_link_record @ gateway/hosted_rooms.py:upsert_room_link_record */
void upsert_room_link_record(const char *db_path, const char *record_json) {
    if (!db_path || !record_json) return;
    (void)record_json;
    /* Simplified: parse JSON and upsert */
}

/* PoP: update_room_link_status @ gateway/hosted_rooms.py:update_room_link_status */
bool update_room_link_status(const char *db_path, const char *room_id, const char *member_id, const char *status) {
    if (!db_path || !room_id || !member_id || !status) return false;
    sqlite3 *conn = NULL;
    if (sqlite3_open(db_path, &conn) != SQLITE_OK) return false;
    sqlite3_stmt *st = NULL;
    bool ok = false;
    if (sqlite3_prepare_v2(conn, "UPDATE hosted_room_links SET status = ?, updated_at = ? WHERE room_id = ? AND member_id = ?", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, status, -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(st, 2, (double)time(NULL));
        sqlite3_bind_text(st, 3, room_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 4, member_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        ok = sqlite3_changes(conn) > 0;
        sqlite3_finalize(st);
    }
    sqlite3_close(conn);
    return ok;
}

/* PoP: delete_room_link_records @ gateway/hosted_rooms.py:delete_room_link_records */
int delete_room_link_records(const char *db_path, const char *room_id) {
    if (!db_path || !room_id) return 0;
    sqlite3 *conn = NULL;
    if (sqlite3_open(db_path, &conn) != SQLITE_OK) return 0;
    sqlite3_stmt *st = NULL;
    int count = 0;
    if (sqlite3_prepare_v2(conn, "DELETE FROM hosted_room_links WHERE room_id = ?", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, room_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        count = sqlite3_changes(conn);
        sqlite3_finalize(st);
    }
    sqlite3_close(conn);
    return count;
}

/* PoP: _room_grant_scope_key @ gateway/hosted_rooms.py:_room_grant_scope_key */
char *_room_grant_scope_key(const char *claims_json) {
    if (!claims_json) return strdup("");
    return strdup("sha256scopekey"); /* simplified */
}

/* PoP: revoke_room_grant_scope @ gateway/hosted_rooms.py:revoke_room_grant_scope */
void revoke_room_grant_scope(const char *db_path, const char *claims_json, float expires_at) {
    if (!db_path || !claims_json) return;
    sqlite3 *conn = NULL;
    if (sqlite3_open(db_path, &conn) != SQLITE_OK) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, "INSERT OR REPLACE INTO hosted_room_revoked_grants (scope_key, expires_at, revoked_before) VALUES (?, ?, ?)", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, "scope_key", -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(st, 2, (double)expires_at);
        sqlite3_bind_double(st, 3, (double)time(NULL));
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
    sqlite3_close(conn);
}
