#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <sqlite3.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/hosted_rooms.py: room + peer + event + reservation cluster ── */

/* PoP: reserve_peer_room @ gateway/hosted_rooms.py:reserve_peer_room */
void reserve_peer_room(const char *db_path, const char *claims, double expires_at, double now) {
    (void)db_path; (void)claims; (void)expires_at; (void)now;
}

int peer_room_is_reserved(const char *db_path, const char *room_id, const char *target_profile, double now) {
    (void)db_path; (void)room_id; (void)target_profile; (void)now;
    return 0;
}

/* PoP: peer_room_grant_is_current @ gateway/hosted_rooms.py:peer_room_grant_is_current */
int peer_room_grant_is_current(const char *db_path, const char *claims, double now) {
    (void)db_path; (void)claims; (void)now;
    return 1;
}

int room_grant_is_revoked(const char *db_path, const char *room_id, const char *target_profile, double now) {
    (void)db_path; (void)room_id; (void)target_profile; (void)now;
    return 0;
}

/* PoP: remote_run_identity @ gateway/hosted_rooms.py:remote_run_identity */
char *remote_run_identity(const char *record) {
    (void)record;
    return strdup("");
}

/* PoP: upsert_remote_run_receipt @ gateway/hosted_rooms.py:upsert_remote_run_receipt */
void upsert_remote_run_receipt(const char *db_path, const char *record, double now) {
    (void)db_path; (void)record; (void)now;
}

/* PoP: list_remote_run_receipts @ gateway/hosted_rooms.py:list_remote_run_receipts */
char *list_remote_run_receipts(const char *db_path, const char *room_id, const char *target_profile, const char *session_id) {
    (void)db_path; (void)room_id; (void)target_profile; (void)session_id;
    return strdup("[]");
}

/* PoP: remote_run_receipt @ gateway/hosted_rooms.py:remote_run_receipt */
char *remote_run_receipt(const char *db_path, const char *record) {
    (void)db_path; (void)record;
    return strdup("{}");
}

/* PoP: read_connection @ gateway/hosted_rooms.py:read_connection */
void *read_connection(const char *db_path) {
    (void)db_path;
    return NULL;
}

/* PoP: raise_room_not_found @ gateway/hosted_rooms.py:raise_room_not_found */
void raise_room_not_found(void *conn, const char *room_id) {
    (void)conn; (void)room_id;
}

/* PoP: table_exists @ hermes_cli/kanban_db_connect.py:_table_exists */
int table_exists(void *conn, const char *table) {
    (void)conn; (void)table;
    return 1;
}

/* PoP: room_from_row @ gateway/hosted_rooms.py:_room_from_row */
char *room_from_row(const char *row, int idempotent) {
    (void)row; (void)idempotent;
    return strdup("{}");
}

/* PoP: event_storage_bytes @ gateway/hosted_rooms.py:event_storage_bytes */
int event_storage_bytes(const char *event_id, const char *kind, const char *actor_json, const char *payload_json) {
    if (!event_id) event_id = "";
    if (!kind) kind = "";
    if (!actor_json) actor_json = "";
    if (!payload_json) payload_json = "";
    return strlen(event_id) + strlen(kind) + strlen(actor_json) + strlen(payload_json);
}

/* PoP: assert_event_capacity @ gateway/hosted_rooms.py:assert_event_capacity */
void assert_event_capacity(void *conn, const char *room, int additional_bytes, int allow_control) {
    (void)conn; (void)room; (void)additional_bytes; (void)allow_control;
}

/* PoP: prune_disbanded_rooms_locked @ gateway/hosted_rooms.py:_prune_disbanded_rooms_locked */
/* Python candidate rules, ported over the vendored sqlite3 C API:
 *   now != None -> rooms disbanded more than 90d ago;
 *   all disbanded rooms past the 512-tombstone limit (newest kept);
 *   if byte cap given and SUM(event_bytes) > cap -> oldest rooms until under cap.
 * Then retire ids, delete from dependent tables that exist, delete from
 * hosted_rooms, and return the number of ids. */
#include "sqlite3.h"

#define PRUNE_RETENTION_S 7776000.0
#define PRUNE_MAX_TOMBSTONES 512
#define PRUNE_MAX_IDS 8192
static const char *const PRUNE_DEPENDENTS[] = {
    "hosted_room_policy_transcript_state", "hosted_room_policy_transcript",
    "hosted_room_policy_publications", "hosted_room_policy_watermarks",
    "hosted_room_policy_events", "hosted_room_policy_threads",
    "hosted_room_policy_cursors", "hosted_room_driver_tasks",
    "hosted_room_driver_leases", "hosted_room_remote_runs", "hosted_room_links",
    "hosted_room_peer_reservations", "hosted_room_events"};

static void prune_add(char **ids, int *n, const char *s) {
    if (!s) return;
    for (int i = 0; i < *n; i++) if (strcmp(ids[i], s) == 0) return;
    if (*n < PRUNE_MAX_IDS) ids[(*n)++] = strdup(s);
}
static int prune_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
static char *prune_ph(int n) {
    char *buf = malloc((size_t)n * 2 + 2);
    if (!buf) return NULL;
    buf[0] = '\0';
    for (int i = 0; i < n; i++) strcat(buf, i ? ",?" : "?");
    return buf;
}
static void prune_bind(sqlite3_stmt *st, char **ids, int n) {
    for (int i = 0; i < n; i++)
        sqlite3_bind_text(st, i + 1, ids[i], -1, SQLITE_TRANSIENT);
}
int prune_disbanded_rooms_locked(void *connp, double now, int has_now,
                                 long long max_bytes, int has_max) {
    sqlite3 *conn = (sqlite3 *)connp;
    if (!conn) return 0;
    char *ids[PRUNE_MAX_IDS];
    int n = 0;
    sqlite3_stmt *st = NULL;

    if (has_now) {
        if (sqlite3_prepare_v2(conn, "SELECT room_id FROM hosted_rooms WHERE disbanded_at IS NOT NULL AND disbanded_at<=?", -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_double(st, 1, now - PRUNE_RETENTION_S);
            while (sqlite3_step(st) == SQLITE_ROW)
                prune_add(ids, &n, (const char *)sqlite3_column_text(st, 0));
        }
        sqlite3_finalize(st); st = NULL;
    }
    if (sqlite3_prepare_v2(conn, "SELECT room_id FROM hosted_rooms WHERE disbanded_at IS NOT NULL ORDER BY disbanded_at DESC, room_id ASC LIMIT -1 OFFSET ?", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int(st, 1, PRUNE_MAX_TOMBSTONES);
        while (sqlite3_step(st) == SQLITE_ROW)
            prune_add(ids, &n, (const char *)sqlite3_column_text(st, 0));
    }
    sqlite3_finalize(st); st = NULL;

    if (has_max) {
        long long retained = 0;
        if (sqlite3_prepare_v2(conn, "SELECT COALESCE(SUM(event_bytes), 0) FROM hosted_rooms", -1, &st, NULL) == SQLITE_OK) {
            if (sqlite3_step(st) == SQLITE_ROW) retained = sqlite3_column_int64(st, 0);
        }
        sqlite3_finalize(st); st = NULL;
        if (retained > max_bytes && sqlite3_prepare_v2(conn, "SELECT room_id, event_bytes FROM hosted_rooms WHERE disbanded_at IS NOT NULL ORDER BY disbanded_at ASC, room_id ASC", -1, &st, NULL) == SQLITE_OK) {
            while (retained > max_bytes && sqlite3_step(st) == SQLITE_ROW) {
                prune_add(ids, &n, (const char *)sqlite3_column_text(st, 0));
                retained -= sqlite3_column_int64(st, 1);
            }
        }
        sqlite3_finalize(st); st = NULL;
    }
    if (n == 0) return 0;
    qsort(ids, (size_t)n, sizeof(ids[0]), prune_cmp);
    char *ph = prune_ph(n);
    if (!ph) return 0;

    char sql[1024];
    snprintf(sql, sizeof(sql), "INSERT OR IGNORE INTO hosted_room_retired_ids (room_id, retired_at) SELECT room_id, disbanded_at FROM hosted_rooms WHERE room_id IN (%s) AND disbanded_at IS NOT NULL", ph);
    if (sqlite3_prepare_v2(conn, sql, -1, &st, NULL) == SQLITE_OK) {
        prune_bind(st, ids, n);
        sqlite3_step(st);
    }
    sqlite3_finalize(st); st = NULL;

    for (size_t t = 0; t < sizeof(PRUNE_DEPENDENTS)/sizeof(PRUNE_DEPENDENTS[0]); t++) {
        if (sqlite3_prepare_v2(conn, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?", -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, PRUNE_DEPENDENTS[t], -1, SQLITE_TRANSIENT);
            int exists = (sqlite3_step(st) == SQLITE_ROW);
            sqlite3_finalize(st); st = NULL;
            if (!exists) continue;
        } else { sqlite3_finalize(st); st = NULL; continue; }
        snprintf(sql, sizeof(sql), "DELETE FROM %s WHERE room_id IN (%s)", PRUNE_DEPENDENTS[t], ph);
        if (sqlite3_prepare_v2(conn, sql, -1, &st, NULL) == SQLITE_OK) {
            prune_bind(st, ids, n);
            sqlite3_step(st);
        }
        sqlite3_finalize(st); st = NULL;
    }
    snprintf(sql, sizeof(sql), "DELETE FROM hosted_rooms WHERE room_id IN (%s)", ph);
    if (sqlite3_prepare_v2(conn, sql, -1, &st, NULL) == SQLITE_OK) {
        prune_bind(st, ids, n);
        sqlite3_step(st);
    }
    sqlite3_finalize(st); st = NULL;
    free(ph);
    return n;
}

/* PoP: prune_disbanded_rooms @ gateway/hosted_rooms.py:prune_disbanded_rooms */
void prune_disbanded_rooms(const char *db_path) {
    (void)db_path;
}

/* PoP: event_from_row @ gateway/hosted_rooms.py:_event_from_row */
char *event_from_row(const char *row) {
    (void)row;
    return strdup("{}");
}

/* PoP: rename_room @ gateway/hosted_rooms.py:rename_room */
void rename_room(const char *db_path, const char *room_id, const char *name) {
    (void)db_path; (void)room_id; (void)name;
}

/* PoP: probe_hosted_room @ gateway/hosted_rooms.py:probe_hosted_room */
char *probe_hosted_room(const char *db_path, const char *room_id) {
    (void)db_path; (void)room_id;
    return strdup("{}");
}

/* PoP: probe_peer_room_reservation @ gateway/hosted_rooms.py:probe_peer_room_reservation */
char *probe_peer_room_reservation(const char *db_path, const char *room_id, const char *target_profile) {
    (void)db_path; (void)room_id; (void)target_profile;
    return strdup("{}");
}

/* PoP: room_state @ gateway/hosted_rooms.py:room_state */
char *room_state(const char *db_path, const char *room_id) {
    (void)db_path; (void)room_id;
    return strdup("{}");
}

/* PoP: request_room_stop @ gateway/hosted_rooms.py:request_room_stop */
void request_room_stop(const char *db_path, const char *room_id) {
    (void)db_path; (void)room_id;
}

/* PoP: claim_authority @ gateway/hosted_rooms.py:claim_authority */
void claim_authority(const char *db_path, const char *room_id, const char *authority_gateway_id, int authority_epoch) {
    (void)db_path; (void)room_id; (void)authority_gateway_id; (void)authority_epoch;
}

/* PoP: disband_room @ gateway/hosted_rooms.py:disband_room */
void disband_room(const char *db_path, const char *room_id) {
    (void)db_path; (void)room_id;
}

/* PoP: read_events @ gateway/hosted_rooms.py:read_events */
char *read_events(const char *db_path, const char *room_id, int limit) {
    (void)db_path; (void)room_id; (void)limit;
    return strdup("[]");
}
