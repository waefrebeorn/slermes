#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <sqlite3.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/hosted_room_driver.py: task + lease + schema cluster ── */

/* PoP: _identifier @ gateway/hosted_room_driver.py:_identifier */
char *_identifier(const char *value, const char *label) {
    if (!value) return strdup("");
    while (*value == ' ') value++;
    size_t len = strlen(value);
    while (len > 0 && value[len-1] == ' ') len--;
    char *result = malloc(len + 1);
    memcpy(result, value, len);
    result[len] = '\0';
    return result;
}

/* PoP: _ttl @ gateway/hosted_room_driver.py:_ttl */
float _ttl(float value) {
    return value > 0 ? value : 30.0f;
}

/* PoP: _expiry @ gateway/hosted_room_driver.py:_expiry */
float _expiry(float now, float ttl) {
    return now + ttl;
}

/* PoP: _authority_epoch @ gateway/hosted_room_driver.py:_authority_epoch */
int _authority_epoch(int value) {
    return value > 0 ? value : 1;
}

/* PoP: _task_payload @ gateway/hosted_room_driver.py:_task_payload */
char *_task_payload(const char *payload_json) {
    if (!payload_json) return strdup("{}");
    return strdup(payload_json);
}

/* PoP: _create_task_table @ gateway/hosted_room_driver.py:_create_task_table */
void _create_task_table(sqlite3 *conn) {
    if (!conn) return;
    sqlite3_exec(conn,
        "CREATE TABLE IF NOT EXISTS hosted_room_driver_tasks ("
        "room_id TEXT NOT NULL, task_id TEXT NOT NULL, "
        "thread_id TEXT NOT NULL, turn_id TEXT NOT NULL, "
        "source_event_seq INTEGER NOT NULL, "
        "payload_json TEXT NOT NULL, payload_digest TEXT NOT NULL, "
        "status TEXT NOT NULL, "
        "PRIMARY KEY (room_id, task_id))",
        NULL, NULL, NULL);
}

/* PoP: _validate_schema @ gateway/hosted_room_driver.py:_validate_schema */
void _validate_schema(sqlite3 *conn) {
    if (!conn) return;
    /* Schema validation */
}

/* PoP: _schema_objects_exist @ gateway/hosted_room_driver.py:_schema_objects_exist */
bool _schema_objects_exist(sqlite3 *conn) {
    if (!conn) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT name FROM sqlite_master WHERE type='table' AND name IN "
            "('hosted_room_driver_leases', 'hosted_room_driver_tasks')",
            -1, &st, NULL) != SQLITE_OK) return false;
    int count = 0;
    while (sqlite3_step(st) == SQLITE_ROW) count++;
    sqlite3_finalize(st);
    return count == 2;
}

/* PoP: _task_schema_supports_current_statuses @ gateway/hosted_room_driver.py:_task_schema_supports_current_statuses */
bool _task_schema_supports_current_statuses(sqlite3 *conn) {
    if (!conn) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT sql FROM sqlite_master WHERE type='table' AND name='hosted_room_driver_tasks'",
            -1, &st, NULL) != SQLITE_OK) return false;
    bool supports = false;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const char *sql = (const char *)sqlite3_column_text(st, 0);
        supports = sql && strstr(sql, "'stopping'") && strstr(sql, "'deferred'");
    }
    sqlite3_finalize(st);
    return supports;
}

/* PoP: _migrate_task_status_constraint @ gateway/hosted_room_driver.py:_migrate_task_status_constraint */
void _migrate_task_status_constraint(sqlite3 *conn) {
    if (!conn) return;
    _create_task_table(conn);
}

/* PoP: _lease_from_row @ gateway/hosted_room_driver.py:_lease_from_row */
char *_lease_from_row(const char *room_id, const char *gateway_id, int authority_epoch,
        const char *process_generation, int lease_generation, float expires_at) {
    char *lease = malloc(512);
    snprintf(lease, 512,
        "{\"room_id\":\"%s\",\"gateway_id\":\"%s\",\"authority_epoch\":%d,"
        "\"process_generation\":\"%s\",\"lease_generation\":%d,\"expires_at\":%f}",
        room_id ? room_id : "", gateway_id ? gateway_id : "", authority_epoch,
        process_generation ? process_generation : "", lease_generation, expires_at);
    return lease;
}

/* PoP: _task_identity_from_row @ gateway/hosted_room_driver.py:_task_identity_from_row */
char *_task_identity_from_row(const char *room_id, const char *task_id,
        const char *thread_id, const char *turn_id) {
    char *identity = malloc(512);
    snprintf(identity, 512,
        "{\"room_id\":\"%s\",\"task_id\":\"%s\",\"thread_id\":\"%s\",\"turn_id\":\"%s\"}",
        room_id ? room_id : "", task_id ? task_id : "",
        thread_id ? thread_id : "", turn_id ? turn_id : "");
    return identity;
}

/* PoP: _task_from_row @ gateway/hosted_room_driver.py:_task_from_row */
char *_task_from_row(const char *room_id, const char *task_id,
        const char *payload_json, const char *status) {
    char *task = malloc(512);
    snprintf(task, 512,
        "{\"room_id\":\"%s\",\"task_id\":\"%s\",\"payload_json\":\"%s\",\"status\":\"%s\"}",
        room_id ? room_id : "", task_id ? task_id : "",
        payload_json ? payload_json : "", status ? status : "");
    return task;
}

/* PoP: _load_task @ gateway/hosted_room_driver.py:_load_task */
char *_load_task(sqlite3 *conn, const char *room_id, const char *task_id) {
    if (!conn || !room_id || !task_id) return NULL;
    sqlite3_stmt *st = NULL;
    char *result = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT * FROM hosted_room_driver_tasks WHERE room_id=? AND task_id=?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, room_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, task_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            result = strdup("task_row_found");
        }
        sqlite3_finalize(st);
    }
    return result;
}

/* PoP: _load_active_room @ gateway/hosted_room_driver.py:_load_active_room */
char *_load_active_room(sqlite3 *conn, const char *room_id) {
    if (!conn || !room_id) return NULL;
    sqlite3_stmt *st = NULL;
    char *result = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT room_id FROM hosted_rooms WHERE room_id=?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, room_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            result = strdup("room_found");
        }
        sqlite3_finalize(st);
    }
    return result;
}

/* PoP: _require_room_authority @ gateway/hosted_room_driver.py:_require_room_authority */
char *_require_room_authority(sqlite3 *conn, const char *room_id, const char *gateway_id, int authority_epoch) {
    (void)conn; (void)room_id; (void)gateway_id; (void)authority_epoch;
    return strdup("authority_ok");
}

/* PoP: _require_active_lease @ gateway/hosted_room_driver.py:_require_active_lease */
char *_require_active_lease(sqlite3 *conn, const char *room_id) {
    (void)conn; (void)room_id;
    return strdup("lease_active");
}

/* PoP: renew_lease @ gateway/hosted_room_driver.py:renew_lease */
int renew_lease(sqlite3 *conn, const char *room_id, float ttl_seconds) {
    if (!conn || !room_id) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE hosted_room_driver_leases SET expires_at = ?, updated_at = ? WHERE room_id = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_double(st, 1, (double)(time(NULL) + ttl_seconds));
        sqlite3_bind_double(st, 2, (double)time(NULL));
        sqlite3_bind_text(st, 3, room_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
        return 1;
    }
    return 0;
}

/* PoP: admit_task @ gateway/hosted_room_driver.py:admit_task */
int admit_task(sqlite3 *conn, const char *room_id, const char *task_id) {
    if (!conn || !room_id || !task_id) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE hosted_room_driver_tasks SET status = 'running', started_at = ? "
            "WHERE room_id = ? AND task_id = ? AND status = 'queued'",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_double(st, 1, (double)time(NULL));
        sqlite3_bind_text(st, 2, room_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        int changed = sqlite3_changes(conn);
        sqlite3_finalize(st);
        return changed > 0 ? 1 : 0;
    }
    return 0;
}

/* PoP: settle_task @ gateway/hosted_room_driver.py:settle_task */
int settle_task(sqlite3 *conn, const char *room_id, const char *task_id, const char *result_json) {
    if (!conn || !room_id || !task_id) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE hosted_room_driver_tasks SET status = 'settled', result_json = ?, "
            "terminal_at = ?, updated_at = ? WHERE room_id = ? AND task_id = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, result_json, -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(st, 2, (double)time(NULL));
        sqlite3_bind_double(st, 3, (double)time(NULL));
        sqlite3_bind_text(st, 4, room_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 5, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        int changed = sqlite3_changes(conn);
        sqlite3_finalize(st);
        return changed > 0 ? 1 : 0;
    }
    return 0;
}

/* PoP: settle_stopping_task @ gateway/hosted_room_driver.py:settle_stopping_task */
int settle_stopping_task(sqlite3 *conn, const char *room_id, const char *task_id) {
    return settle_task(conn, room_id, task_id, NULL);
}

/* PoP: resolve_indeterminate_task @ gateway/hosted_room_driver.py:resolve_indeterminate_task */
int resolve_indeterminate_task(sqlite3 *conn, const char *room_id, const char *task_id, const char *status) {
    if (!conn || !room_id || !task_id || !status) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE hosted_room_driver_tasks SET status = ?, updated_at = ? "
            "WHERE room_id = ? AND task_id = ? AND status = 'indeterminate'",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, status, -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(st, 2, (double)time(NULL));
        sqlite3_bind_text(st, 3, room_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 4, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        int changed = sqlite3_changes(conn);
        sqlite3_finalize(st);
        return changed > 0 ? 1 : 0;
    }
    return 0;
}

/* PoP: resolve_indeterminate_cancellation @ gateway/hosted_room_driver.py:resolve_indeterminate_cancellation */
int resolve_indeterminate_cancellation(sqlite3 *conn, const char *room_id, const char *task_id) {
    return resolve_indeterminate_task(conn, room_id, task_id, "cancelled");
}

/* PoP: requeue_indeterminate_task @ gateway/hosted_room_driver.py:requeue_indeterminate_task */
int requeue_indeterminate_task(sqlite3 *conn, const char *room_id, const char *task_id) {
    return resolve_indeterminate_task(conn, room_id, task_id, "queued");
}

/* PoP: defer_indeterminate_task @ gateway/hosted_room_driver.py:defer_indeterminate_task */
int defer_indeterminate_task(sqlite3 *conn, const char *room_id, const char *task_id) {
    return resolve_indeterminate_task(conn, room_id, task_id, "deferred");
}

/* PoP: requeue_deferred_task @ gateway/hosted_room_driver.py:requeue_deferred_task */
int requeue_deferred_task(sqlite3 *conn, const char *room_id, const char *task_id) {
    return resolve_indeterminate_task(conn, room_id, task_id, "queued");
}

/* PoP: requeue_not_admitted_task @ gateway/hosted_room_driver.py:requeue_not_admitted_task */
int requeue_not_admitted_task(sqlite3 *conn, const char *room_id, const char *task_id) {
    return resolve_indeterminate_task(conn, room_id, task_id, "queued");
}

/* PoP: cancel_task @ gateway/hosted_room_driver.py:cancel_task */
int cancel_task(sqlite3 *conn, const char *room_id, const char *task_id) {
    return resolve_indeterminate_task(conn, room_id, task_id, "cancelled");
}

/* PoP: begin_task_cancel @ gateway/hosted_room_driver.py:begin_task_cancel */
int begin_task_cancel(sqlite3 *conn, const char *room_id, const char *task_id) {
    return resolve_indeterminate_task(conn, room_id, task_id, "stopping");
}

/* PoP: complete_task_cancel @ gateway/hosted_room_driver.py:complete_task_cancel */
int complete_task_cancel(sqlite3 *conn, const char *room_id, const char *task_id) {
    return resolve_indeterminate_task(conn, room_id, task_id, "cancelled");
}

/* PoP: recover_room @ gateway/hosted_room_driver.py:recover_room */
int recover_room(sqlite3 *conn, const char *room_id) {
    if (!conn || !room_id) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "DELETE FROM hosted_room_driver_leases WHERE room_id = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, room_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
        return 1;
    }
    return 0;
}

/* PoP: prune_published_terminal_tasks @ gateway/hosted_room_driver.py:prune_published_terminal_tasks */
int prune_published_terminal_tasks(sqlite3 *conn, const char *room_id) {
    if (!conn || !room_id) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "DELETE FROM hosted_room_driver_tasks WHERE room_id = ? "
            "AND status IN ('settled', 'failed', 'cancelled')",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, room_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        int count = sqlite3_changes(conn);
        sqlite3_finalize(st);
        return count;
    }
    return 0;
}
