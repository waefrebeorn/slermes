/*
 * hosted_room_driver.c - C11 port of upstream
 * persistence/sqlite/gateway/hosted_room_driver.py.
 *
 * Three differential gates share this one file:
 *   w29_hostedroom_8927a5_p1  statement text, schema DDL and the schema verdicts
 *   w29_hostedroom_8927a5_p2  the fenced lease lifecycle and the row predicates
 *   w29_hostedroom_8927a5_p3  the task state machine, its replays and the task view
 *
 * PoP: w29_hostedroom_8927a5_p1_task_update @ persistence/sqlite/gateway/hosted_room_driver.py:_task_update
 * PoP: w29_hostedroom_8927a5_p1_generation_update @ persistence/sqlite/gateway/hosted_room_driver.py:_generation_update
 * PoP: w29_hostedroom_8927a5_p1_named_sql @ persistence/sqlite/gateway/hosted_room_driver.py:<fenced statement constants>
 * PoP: w29_hostedroom_8927a5_p1_tasks_in_order_sql @ persistence/sqlite/gateway/hosted_room_driver.py:_tasks_in_order
 * PoP: w29_hostedroom_8927a5_p1_task_index_sql @ persistence/sqlite/gateway/hosted_room_driver.py:_TASK_INDEX_SQL
 * PoP: w29_hostedroom_8927a5_p1_task_columns @ persistence/sqlite/gateway/hosted_room_driver.py:_TASK_COLUMN_ORDER
 * PoP: w29_hostedroom_8927a5_p1_transition @ persistence/sqlite/gateway/hosted_room_driver.py:_GENERATION_TRANSITIONS
 * PoP: w29_hostedroom_8927a5_p1_task_table_ddl @ persistence/sqlite/gateway/hosted_room_driver.py:_create_task_table
 * PoP: w29_hostedroom_8927a5_p1_leases_table_ddl @ persistence/sqlite/gateway/hosted_room_driver.py:_initialize_schema
 * PoP: w29_hostedroom_8927a5_p1_schema_sql @ persistence/sqlite/gateway/hosted_room_driver.py:_initialize_store
 * PoP: w29_hostedroom_8927a5_p1_step_mode @ persistence/sqlite/gateway/hosted_room_driver.py:_initialize_store
 * PoP: w29_hostedroom_8927a5_p1_schema_objects_exist @ persistence/sqlite/gateway/hosted_room_driver.py:_schema_objects_exist
 * PoP: w29_hostedroom_8927a5_p1_validate_schema @ persistence/sqlite/gateway/hosted_room_driver.py:_validate_schema
 * PoP: w29_hostedroom_8927a5_p1_schema_ready @ persistence/sqlite/gateway/hosted_room_driver.py:_schema_ready
 * PoP: w29_hostedroom_8927a5_p2_run_fence @ persistence/sqlite/gateway/hosted_room_driver.py:_run_fence
 * PoP: w29_hostedroom_8927a5_p2_lease_from_row @ persistence/sqlite/gateway/hosted_room_driver.py:_lease_from_row
 * PoP: w29_hostedroom_8927a5_p2_lease_row_matches @ persistence/sqlite/gateway/hosted_room_driver.py:_lease_row_matches
 * PoP: w29_hostedroom_8927a5_p2_task_identity @ persistence/sqlite/gateway/hosted_room_driver.py:_task_identity_from_row
 * PoP: w29_hostedroom_8927a5_p2_load_task @ persistence/sqlite/gateway/hosted_room_driver.py:_load_task
 * PoP: w29_hostedroom_8927a5_p2_active_room @ persistence/sqlite/gateway/hosted_room_driver.py:_load_active_room
 * PoP: w29_hostedroom_8927a5_p2_room_authority @ persistence/sqlite/gateway/hosted_room_driver.py:_require_room_authority
 * PoP: w29_hostedroom_8927a5_p2_lease_gate @ persistence/sqlite/gateway/hosted_room_driver.py:_require_active_lease
 * PoP: w29_hostedroom_8927a5_p2_generations_match @ persistence/sqlite/gateway/hosted_room_driver.py:_generations_match
 * PoP: w29_hostedroom_8927a5_p2_cancel_generation_guard @ persistence/sqlite/gateway/hosted_room_driver.py:_require_cancel_generation
 * PoP: w29_hostedroom_8927a5_p2_acquire @ persistence/sqlite/gateway/hosted_room_driver.py:acquire_lease
 * PoP: w29_hostedroom_8927a5_p2_renew @ persistence/sqlite/gateway/hosted_room_driver.py:renew_lease
 * PoP: w29_hostedroom_8927a5_p2_release @ persistence/sqlite/gateway/hosted_room_driver.py:release_lease
 * PoP: w29_hostedroom_8927a5_p3_task_view @ persistence/sqlite/gateway/hosted_room_driver.py:_task_from_row
 * PoP: w29_hostedroom_8927a5_p3_settlement_params @ persistence/sqlite/gateway/hosted_room_driver.py:_settlement
 * PoP: w29_hostedroom_8927a5_p3_settlement_replay @ persistence/sqlite/gateway/hosted_room_driver.py:_settlement.replay
 * PoP: w29_hostedroom_8927a5_p3_cancel_replay @ persistence/sqlite/gateway/hosted_room_driver.py:_cancel_replay
 * PoP: w29_hostedroom_8927a5_p3_defer_result @ persistence/sqlite/gateway/hosted_room_driver.py:_defer
 * PoP: w29_hostedroom_8927a5_p3_admit @ persistence/sqlite/gateway/hosted_room_driver.py:admit_task
 * PoP: w29_hostedroom_8927a5_p3_start @ persistence/sqlite/gateway/hosted_room_driver.py:start_task
 * PoP: w29_hostedroom_8927a5_p3_cancel @ persistence/sqlite/gateway/hosted_room_driver.py:cancel_task
 * PoP: w29_hostedroom_8927a5_p3_settle @ persistence/sqlite/gateway/hosted_room_driver.py:settle_task
 * PoP: w29_hostedroom_8927a5_p3_generation @ persistence/sqlite/gateway/hosted_room_driver.py:<_GENERATION_TRANSITIONS verbs>
 * PoP: w29_hostedroom_8927a5_p3_prune @ persistence/sqlite/gateway/hosted_room_driver.py:prune_published_terminal_tasks
 * PoP: w29_hostedroom_8927a5_p3_mark_indeterminate @ persistence/sqlite/gateway/hosted_room_driver.py:mark_attempt_indeterminate
 * PoP: w29_hostedroom_8927a5_p3_settle_stopping @ persistence/sqlite/gateway/hosted_room_driver.py:settle_stopping_task
 * PoP: w29_hostedroom_8927a5_p3_begin_cancel @ persistence/sqlite/gateway/hosted_room_driver.py:begin_task_cancel
 * PoP: w29_hostedroom_8927a5_p3_complete_cancel @ persistence/sqlite/gateway/hosted_room_driver.py:complete_task_cancel
 * PoP: w29_hostedroom_8927a5_p3_requeue_not_admitted @ persistence/sqlite/gateway/hosted_room_driver.py:requeue_not_admitted_task
 * PoP: w29_hostedroom_8927a5_p2_recover @ persistence/sqlite/gateway/hosted_room_driver.py:recover_room
 *
 * SEAMS (fixed, never decisions): the SQLite connection and its transaction
 * (`BEGIN IMMEDIATE`), `fenced_update`'s rowcount read, the caller-side
 * `normalize_task_payload`/digest check before `admit_task` and in the stored
 * payload, the work-record capture after every write, `clock(None)`, the
 * status aggregation SQL (`room_task_summary` counts, `room_change_rows`'
 * ordering of retained rows), and the placement/file plumbing.  What is ported
 * here is exactly what those values decide: which statement text a verb emits,
 * whether a fence or a lease holds, whether a write is an idempotent replay,
 * and how a stored row is shaped for the caller.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_hostedroom_8927a5_p1.h"
#include "noc_w29_hostedroom_8927a5_p2.h"
#include "noc_w29_hostedroom_8927a5_p3.h"

/* ── exact statement text (captured from the upstream module) ─────────────── */

static const char *const SQL_INITIALIZE[] = {
    "SELECT name FROM sqlite_master WHERE type='table'\n           AND name IN ('hosted_room_driver_leases', 'hosted_room_driver_tasks')",
    "CREATE TABLE IF NOT EXISTS hosted_room_driver_leases (\n            room_id TEXT PRIMARY KEY, gateway_id TEXT NOT NULL,\n            authority_epoch INTEGER NOT NULL CHECK (authority_epoch >= 1), process_generation TEXT NOT NULL,\n            lease_generation INTEGER NOT NULL CHECK (lease_generation >= 1),\n            expires_at REAL NOT NULL, acquired_at REAL NOT NULL, updated_at REAL NOT NULL, released_at REAL,\n            FOREIGN KEY (room_id) REFERENCES hosted_rooms(room_id))",
    "CREATE TABLE IF NOT EXISTS hosted_room_driver_tasks (\n            room_id TEXT NOT NULL, task_id TEXT NOT NULL, thread_id TEXT NOT NULL, turn_id TEXT NOT NULL,\n            source_event_seq INTEGER NOT NULL CHECK (source_event_seq >= 1),\n            payload_json TEXT NOT NULL, payload_digest TEXT NOT NULL,\n            status TEXT NOT NULL CHECK (status IN (\n                'queued', 'running', 'settled', 'failed', 'cancelled', 'indeterminate', 'deferred', 'stopping')),\n            execution_generation INTEGER NOT NULL DEFAULT 0 CHECK (execution_generation >= 0),\n            cancel_generation INTEGER NOT NULL DEFAULT 0 CHECK (cancel_generation >= 0),\n            run_gateway_id TEXT, run_process_generation TEXT, run_lease_generation INTEGER, cancel_id TEXT,\n            settlement_id TEXT, settlement_status TEXT, result_json TEXT, created_at REAL NOT NULL,\n            updated_at REAL NOT NULL, started_at REAL, terminal_at REAL, indeterminate_at REAL,\n            PRIMARY KEY (room_id, task_id), UNIQUE (room_id, thread_id, turn_id),\n            FOREIGN KEY (room_id) REFERENCES hosted_rooms(room_id))",
    "PRAGMA table_info(hosted_room_driver_leases)",
    "PRAGMA table_info(hosted_room_driver_tasks)",
    "PRAGMA foreign_key_list(hosted_room_driver_leases)",
    "PRAGMA foreign_key_list(hosted_room_driver_tasks)",
    "CREATE INDEX IF NOT EXISTS idx_hosted_room_driver_tasks_status\n           ON hosted_room_driver_tasks(room_id, status, source_event_seq, created_at, task_id)",
};
#define SQL_INITIALIZE_N (sizeof(SQL_INITIALIZE) / sizeof(SQL_INITIALIZE[0]))

static const char *const SQL_MIGRATE[] = {
    "SELECT name FROM sqlite_master WHERE type='table'\n           AND name IN ('hosted_room_driver_leases', 'hosted_room_driver_tasks')",
    "SELECT 1 FROM sqlite_master WHERE type='index' AND name='idx_hosted_room_driver_tasks_status'",
    "DROP INDEX IF EXISTS idx_hosted_room_driver_tasks_status",
    "CREATE TABLE IF NOT EXISTS hosted_room_driver_tasks_next (\n            room_id TEXT NOT NULL, task_id TEXT NOT NULL, thread_id TEXT NOT NULL, turn_id TEXT NOT NULL,\n            source_event_seq INTEGER NOT NULL CHECK (source_event_seq >= 1),\n            payload_json TEXT NOT NULL, payload_digest TEXT NOT NULL,\n            status TEXT NOT NULL CHECK (status IN (\n                'queued', 'running', 'settled', 'failed', 'cancelled', 'indeterminate', 'deferred', 'stopping')),\n            execution_generation INTEGER NOT NULL DEFAULT 0 CHECK (execution_generation >= 0),\n            cancel_generation INTEGER NOT NULL DEFAULT 0 CHECK (cancel_generation >= 0),\n            run_gateway_id TEXT, run_process_generation TEXT, run_lease_generation INTEGER, cancel_id TEXT,\n            settlement_id TEXT, settlement_status TEXT, result_json TEXT, created_at REAL NOT NULL,\n            updated_at REAL NOT NULL, started_at REAL, terminal_at REAL, indeterminate_at REAL,\n            PRIMARY KEY (room_id, task_id), UNIQUE (room_id, thread_id, turn_id),\n            FOREIGN KEY (room_id) REFERENCES hosted_rooms(room_id))",
    "INSERT INTO hosted_room_driver_tasks_next (room_id, task_id, thread_id, turn_id, source_event_seq, payload_json, payload_digest, status, execution_generation, cancel_generation, run_gateway_id, run_process_generation, run_lease_generation, cancel_id, settlement_id, settlement_status, result_json, created_at, updated_at, started_at, terminal_at, indeterminate_at) SELECT room_id, task_id, thread_id, turn_id, source_event_seq, payload_json, payload_digest, status, execution_generation, cancel_generation, run_gateway_id, run_process_generation, run_lease_generation, cancel_id, settlement_id, settlement_status, result_json, created_at, updated_at, started_at, terminal_at, indeterminate_at FROM hosted_room_driver_tasks",
    "DROP TABLE hosted_room_driver_tasks",
    "ALTER TABLE hosted_room_driver_tasks_next RENAME TO hosted_room_driver_tasks",
    "CREATE INDEX idx_hosted_room_driver_tasks_status\n           ON hosted_room_driver_tasks(room_id, status, source_event_seq, created_at, task_id)",
    "PRAGMA table_info(hosted_room_driver_leases)",
    "PRAGMA table_info(hosted_room_driver_tasks)",
    "PRAGMA foreign_key_list(hosted_room_driver_leases)",
    "PRAGMA foreign_key_list(hosted_room_driver_tasks)",
};
#define SQL_MIGRATE_N (sizeof(SQL_MIGRATE) / sizeof(SQL_MIGRATE[0]))

static const char *const SQL_NAMED[][2] = {
    { "select_lease", "SELECT * FROM hosted_room_driver_leases WHERE room_id=?" },
    { "select_task", "SELECT * FROM hosted_room_driver_tasks WHERE room_id=? AND task_id=?" },
    { "settle_running_sql", "UPDATE hosted_room_driver_tasks SET status=?, settlement_id=?, settlement_status=?, result_json=?, terminal_at=?, updated_at=? WHERE room_id=? AND task_id=? AND status='running' AND execution_generation=? AND cancel_generation=? AND run_gateway_id=? AND run_process_generation=? AND run_lease_generation=?" },
    { "settle_stopping_sql", "UPDATE hosted_room_driver_tasks SET status=?, settlement_id=?, settlement_status=?, result_json=?, terminal_at=?, updated_at=? WHERE room_id=? AND task_id=? AND status='stopping' AND execution_generation=? AND cancel_generation=?" },
    { "requeue_running_sql", "UPDATE hosted_room_driver_tasks SET status='queued', run_gateway_id=NULL, run_process_generation=NULL, run_lease_generation=NULL, started_at=NULL, updated_at=? WHERE room_id=? AND task_id=? AND status='running' AND execution_generation=? AND cancel_generation=? AND run_gateway_id=? AND run_process_generation=? AND run_lease_generation=?" },
    { "cancel_queued_sql", "UPDATE hosted_room_driver_tasks SET status='cancelled', cancel_generation=?, cancel_id=?, terminal_at=?, updated_at=? WHERE room_id=? AND task_id=? AND status IN ('queued', 'deferred') AND cancel_generation=?" },
    { "begin_stop_sql", "UPDATE hosted_room_driver_tasks SET status='stopping', cancel_generation=?, cancel_id=?, updated_at=? WHERE room_id=? AND task_id=? AND status IN ('running', 'indeterminate') AND cancel_generation=?" },
    { "complete_stop_sql", "UPDATE hosted_room_driver_tasks SET status='cancelled', terminal_at=?, updated_at=? WHERE room_id=? AND task_id=? AND status='stopping' AND cancel_id=? AND cancel_generation=?" },
};

static const char *const SQL_TASK_ORDER = "ORDER BY source_event_seq, created_at, task_id";
static const char *const SQL_TASK_INDEX = "CREATE INDEX {if_not_exists}idx_hosted_room_driver_tasks_status\n           ON hosted_room_driver_tasks(room_id, status, source_event_seq, created_at, task_id)";

static const char *const TASK_COLUMNS[] = {
    "room_id", "task_id", "thread_id", "turn_id", "source_event_seq", "payload_json", "payload_digest", "status",
    "execution_generation", "cancel_generation", "run_gateway_id", "run_process_generation", "run_lease_generation",
    "cancel_id", "settlement_id", "settlement_status", "result_json", "created_at", "updated_at", "started_at",
    "terminal_at", "indeterminate_at",
};
#define TASK_COLUMNS_N (sizeof(TASK_COLUMNS) / sizeof(TASK_COLUMNS[0]))

static const char *const LEASE_COLUMNS[] = {
    "acquired_at", "authority_epoch", "expires_at", "gateway_id", "lease_generation", "process_generation",
    "released_at", "room_id", "updated_at",
};
#define LEASE_COLUMNS_N (sizeof(LEASE_COLUMNS) / sizeof(LEASE_COLUMNS[0]))

/* name, fenced status, SET clause, generation-stale message, row-stale message */
static const char *const TRANSITIONS[][5] = {
    { "resolve", "indeterminate", "status=?, settlement_id=?, settlement_status=?, result_json=?, terminal_at=?, updated_at=?", "indeterminate task generation changed", "indeterminate task changed during reconciliation" },
    { "resolve_cancel", "indeterminate", "status='cancelled', cancel_generation=?, cancel_id=?, terminal_at=?, updated_at=?", "indeterminate cancellation proof is stale", "indeterminate cancellation proof lost its fence" },
    { "requeue", "indeterminate", "status='queued', run_gateway_id=NULL, run_process_generation=NULL, run_lease_generation=NULL, started_at=NULL, indeterminate_at=NULL, updated_at=?", "indeterminate task generation changed", "indeterminate task changed during requeue" },
    { "defer", "indeterminate", "status='deferred', result_json=?, terminal_at=?, updated_at=?", "indeterminate task generation changed", "indeterminate task changed during deferral" },
    { "defer_stopping", "stopping", "status='deferred', result_json=?, terminal_at=?, updated_at=?", "stopping task generation changed", "stopping task changed during deferral" },
    { "reopen_deferred", "deferred", "status='indeterminate', result_json=NULL, terminal_at=NULL, started_at=?, indeterminate_at=?, updated_at=?", "deferred task generation changed", "deferred task changed during reopen" },
    { "requeue_deferred", "deferred", "status='queued', run_gateway_id=NULL, run_process_generation=NULL, run_lease_generation=NULL, result_json=NULL, started_at=NULL, terminal_at=NULL, indeterminate_at=NULL, updated_at=?", "deferred task generation changed", "deferred task changed during requeue" },
};
#define TRANSITIONS_N (sizeof(TRANSITIONS) / sizeof(TRANSITIONS[0]))

#define MAX_TASK_PRUNE_BATCH 1000

/* ── small helpers ───────────────────────────────────────────────────────── */

typedef struct {
    char *p;
    size_t len, cap;
    int oom;
} sb;

static void sb_init(sb *b) { b->p = NULL; b->len = 0; b->cap = 0; b->oom = 0; }

static void sb_grow(sb *b, size_t need) {
    size_t cap;
    char *q;
    if (b->oom) return;
    cap = b->cap ? b->cap : 128;
    while (cap < need) cap *= 2;
    if (cap == b->cap) return;
    q = (char *)realloc(b->p, cap);
    if (!q) { b->oom = 1; return; }
    b->p = q;
    b->cap = cap;
}

static void sb_addn(sb *b, const char *s, size_t n) {
    if (b->oom || !s) return;
    sb_grow(b, b->len + n + 1);
    if (b->oom) return;
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}

static void sb_add(sb *b, const char *s) { if (s) sb_addn(b, s, strlen(s)); }
static void sb_addc(sb *b, char c) { sb_addn(b, &c, 1); }

static char *sb_take(sb *b) {
    if (!b->p) {
        b->p = (char *)malloc(1);
        if (!b->p) { b->oom = 1; return NULL; }
        b->p[0] = '\0';
    }
    return b->p;
}

static json_t *sb_json(sb *b) {
    char *s = sb_take(b);
    json_t *v = json_string(s ? s : "");
    free(s);
    return v;
}

static const json_t *jget(const json_t *o, const char *k) {
    return o && o->type == JSON_OBJECT ? json_obj_get(o, k) : NULL;
}

static const char *jstr(const json_t *o, const char *k) {
    const json_t *v = jget(o, k);
    return v && v->type == JSON_STRING ? v->str_val : NULL;
}

/* Python None for a missing key or an explicit null. */
static int jnone(const json_t *v) { return !v || v->type == JSON_NULL; }

static int jtruthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

static double jnum(const json_t *v, double d) {
    if (!v) return d;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return d;
}

static double jgetnum(const json_t *o, const char *k, double d) { return jnum(jget(o, k), d); }

static long long jint(const json_t *v, long long d) { return (long long)jnum(v, (double)d); }
static long long jgetint(const json_t *o, const char *k, long long d) { return jint(jget(o, k), d); }

static int jgetbool(const json_t *o, const char *k, int d) {
    const json_t *v = jget(o, k);
    return v ? jtruthy(v) : d;
}

/* Python '==' over the shapes a stored column can hold. */
static int py_eq(const json_t *a, const json_t *b) {
    size_t i, n;
    if (jnone(a)) return jnone(b);
    if (jnone(b)) return 0;
    if ((a->type == JSON_NUMBER || a->type == JSON_BOOL) &&
        (b->type == JSON_NUMBER || b->type == JSON_BOOL))
        return jnum(a, 0.0) == jnum(b, 0.0);
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type == JSON_ARRAY && b->type == JSON_ARRAY) {
        n = json_len(a);
        if (n != json_len(b)) return 0;
        for (i = 0; i < n; i++)
            if (!py_eq(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    }
    if (a->type == JSON_OBJECT && b->type == JSON_OBJECT) {
        n = json_object_size(a);
        if (n != json_object_size(b)) return 0;
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(a, i);
            if (!k || !py_eq(json_object_get_at(a, i), jget(b, k))) return 0;
        }
        return 1;
    }
    return 0;
}

static int py_eq_key(const json_t *o, const char *k, const json_t *other) {
    return py_eq(jget(o, k), other);
}

static json_t *err_obj(const char *error, const char *message) {
    json_t *o = json_object();
    json_set(o, "error", error ? json_string(error) : json_null());
    json_set(o, "message", message ? json_string(message) : json_null());
    return o;
}

static json_t *json_from_long(long long v) { return json_number((double)v); }

/* Python repr of a JSON value under `compact_json` (sorted keys, no spaces,
 * ensure_ascii).  Integral numbers print as integers, as json.dumps does. */
static void compact_value(sb *b, const json_t *v);

static void compact_number(sb *b, double x) {
    char buf[40];
    if (x == (double)(long long)x && x > -1e15 && x < 1e15)
        snprintf(buf, sizeof buf, "%lld", (long long)x);
    else
        snprintf(buf, sizeof buf, "%.17g", x);
    sb_add(b, buf);
}

static void compact_string(sb *b, const char *s) {
    const unsigned char *q = (const unsigned char *)(s ? s : "");
    sb_addc(b, '"');
    for (; *q; q++) {
        switch (*q) {
        case '"':  sb_add(b, "\\\""); break;
        case '\\': sb_add(b, "\\\\"); break;
        case '\n': sb_add(b, "\\n"); break;
        case '\r': sb_add(b, "\\r"); break;
        case '\t': sb_add(b, "\\t"); break;
        case '\b': sb_add(b, "\\b"); break;
        case '\f': sb_add(b, "\\f"); break;
        default:
            if (*q < 0x20) {
                char u[8];
                snprintf(u, sizeof u, "\\u%04x", (unsigned)*q);
                sb_add(b, u);
            } else if (*q < 0x80) {
                sb_addc(b, (char)*q);
            } else {
                unsigned cp = 0;
                int extra = 0, i;
                if ((*q & 0xe0) == 0xc0) { cp = *q & 0x1fu; extra = 1; }
                else if ((*q & 0xf0) == 0xe0) { cp = *q & 0x0fu; extra = 2; }
                else if ((*q & 0xf8) == 0xf0) { cp = *q & 0x07u; extra = 3; }
                else { cp = *q; }
                for (i = 0; i < extra && q[1]; i++) { q++; cp = (cp << 6) | (*q & 0x3fu); }
                if (cp >= 0x10000u) {
                    char u[16];
                    unsigned v2 = cp - 0x10000u;
                    snprintf(u, sizeof u, "\\u%04x\\u%04x", 0xd800u + (v2 >> 10), 0xdc00u + (v2 & 0x3ffu));
                    sb_add(b, u);
                } else {
                    char u[8];
                    snprintf(u, sizeof u, "\\u%04x", cp);
                    sb_add(b, u);
                }
            }
        }
    }
    sb_addc(b, '"');
}

static void compact_value(sb *b, const json_t *v) {
    size_t i, j, n;
    if (!v) { sb_add(b, "null"); return; }
    switch (v->type) {
    case JSON_NULL:   sb_add(b, "null"); break;
    case JSON_BOOL:   sb_add(b, v->bool_val ? "true" : "false"); break;
    case JSON_NUMBER: compact_number(b, v->num_val); break;
    case JSON_STRING: compact_string(b, v->str_val); break;
    case JSON_ARRAY:
        sb_addc(b, '[');
        n = json_len(v);
        for (i = 0; i < n; i++) {
            if (i) sb_addc(b, ',');
            compact_value(b, json_get(v, i));
        }
        sb_addc(b, ']');
        break;
    case JSON_OBJECT: {
        size_t *order;
        n = json_object_size(v);
        order = (size_t *)malloc((n ? n : 1) * sizeof(size_t));
        if (!order) { sb_add(b, "{}"); return; }
        for (i = 0; i < n; i++) order[i] = i;
        for (i = 1; i < n; i++) {          /* insertion sort by key, like sort_keys */
            size_t cur = order[i];
            const char *ck = json_object_get_key_at(v, cur);
            j = i;
            while (j > 0) {
                const char *pk = json_object_get_key_at(v, order[j - 1]);
                if (strcmp(pk ? pk : "", ck ? ck : "") <= 0) break;
                order[j] = order[j - 1];
                j--;
            }
            order[j] = cur;
        }
        sb_addc(b, '{');
        for (i = 0; i < n; i++) {
            if (i) sb_addc(b, ',');
            compact_string(b, json_object_get_key_at(v, order[i]));
            sb_addc(b, ':');
            compact_value(b, json_object_get_at(v, order[i]));
        }
        sb_addc(b, '}');
        free(order);
        break;
    }
    }
}

static json_t *compact_json(const json_t *v) {
    sb b;
    json_t *out;
    sb_init(&b);
    compact_value(&b, v);
    out = sb_json(&b);
    return out;
}

/* Set equality of a JSON array of strings against a fixed column list. */
static int set_equals(const json_t *arr, const char *const *want, size_t n) {
    size_t i, j, m;
    if (!arr || arr->type != JSON_ARRAY) {
        return n == 0;
    }
    m = json_len(arr);
    if (m != n) return 0;
    for (i = 0; i < n; i++) {
        int found = 0;
        for (j = 0; j < m; j++) {
            const json_t *v = json_get(arr, j);
            if (v && v->type == JSON_STRING && v->str_val && strcmp(v->str_val, want[i]) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) return 0;
    }
    return 1;
}

/* ══ gate p1: statement text and schema verdicts ═══════════════════════════ */

/* PoP: w29_hostedroom_8927a5_p1_task_update @ hosted_room_driver.py:_task_update */
json_t *w29_hostedroom_8927a5_p1_task_update(const char *set_clause, const char *fence) {
    sb b;
    sb_init(&b);
    sb_add(&b, "UPDATE hosted_room_driver_tasks SET ");
    sb_add(&b, set_clause ? set_clause : "");
    sb_add(&b, " WHERE room_id=? AND task_id=? AND ");
    sb_add(&b, fence ? fence : "");
    return sb_json(&b);
}

/* PoP: w29_hostedroom_8927a5_p1_generation_update @ hosted_room_driver.py:_generation_update */
json_t *w29_hostedroom_8927a5_p1_generation_update(const char *set_clause, const char *status) {
    sb fence;
    json_t *out;
    sb_init(&fence);
    sb_add(&fence, "status='");
    sb_add(&fence, status ? status : "");
    sb_add(&fence, "' AND execution_generation=? AND cancel_generation=?");
    {
        char *f = sb_take(&fence);
        out = w29_hostedroom_8927a5_p1_task_update(set_clause, f);
        free(f);
    }
    return out;
}

/* PoP: w29_hostedroom_8927a5_p1_named_sql @ hosted_room_driver.py:<fenced statement constants> */
json_t *w29_hostedroom_8927a5_p1_named_sql(const char *name) {
    size_t i;
    if (!name) return json_null();
    for (i = 0; i < sizeof(SQL_NAMED) / sizeof(SQL_NAMED[0]); i++)
        if (!strcmp(SQL_NAMED[i][0], name)) return json_string(SQL_NAMED[i][1]);
    return json_null();
}

/* PoP: w29_hostedroom_8927a5_p1_tasks_in_order_sql @ hosted_room_driver.py:_tasks_in_order */
json_t *w29_hostedroom_8927a5_p1_tasks_in_order_sql(const char *status) {
    sb b;
    sb_init(&b);
    sb_add(&b, "SELECT * FROM hosted_room_driver_tasks WHERE room_id=?");
    if (status) sb_add(&b, " AND status=?");
    sb_addc(&b, ' ');
    sb_add(&b, SQL_TASK_ORDER);
    return sb_json(&b);
}

/* PoP: w29_hostedroom_8927a5_p1_task_index_sql @ hosted_room_driver.py:_TASK_INDEX_SQL */
json_t *w29_hostedroom_8927a5_p1_task_index_sql(int if_not_exists) {
    const char *token = "{if_not_exists}";
    const char *hit;
    sb b;
    size_t head;
    sb_init(&b);
    hit = strstr(SQL_TASK_INDEX, token);
    if (!hit) return json_string(SQL_TASK_INDEX);
    head = (size_t)(hit - SQL_TASK_INDEX);
    sb_addn(&b, SQL_TASK_INDEX, head);
    sb_add(&b, if_not_exists ? "IF NOT EXISTS " : "");
    sb_add(&b, hit + strlen(token));
    return sb_json(&b);
}

/* PoP: w29_hostedroom_8927a5_p1_task_columns @ hosted_room_driver.py:_TASK_COLUMN_ORDER */
json_t *w29_hostedroom_8927a5_p1_task_columns(void) {
    json_t *a = json_array();
    size_t i;
    for (i = 0; i < TASK_COLUMNS_N; i++) json_append(a, json_string(TASK_COLUMNS[i]));
    return a;
}

/* PoP: w29_hostedroom_8927a5_p1_transition @ hosted_room_driver.py:_GENERATION_TRANSITIONS */
json_t *w29_hostedroom_8927a5_p1_transition(const char *name) {
    size_t i;
    for (i = 0; i < TRANSITIONS_N; i++) {
        if (name && !strcmp(TRANSITIONS[i][0], name)) {
            json_t *o = json_object();
            json_t *sql = w29_hostedroom_8927a5_p1_generation_update(TRANSITIONS[i][2], TRANSITIONS[i][1]);
            json_set(o, "status", json_string(TRANSITIONS[i][1]));
            json_set(o, "set", json_string(TRANSITIONS[i][2]));
            json_set(o, "generation_stale", json_string(TRANSITIONS[i][3]));
            json_set(o, "stale", json_string(TRANSITIONS[i][4]));
            json_set(o, "sql", sql);
            return o;
        }
    }
    return json_null();
}

/* PoP: w29_hostedroom_8927a5_p1_task_table_ddl @ hosted_room_driver.py:_create_task_table */
json_t *w29_hostedroom_8927a5_p1_task_table_ddl(const char *table) {
    const char *base = SQL_INITIALIZE[2];
    const char *paren = strchr(base, '(');
    sb b;
    if (!table) table = "hosted_room_driver_tasks";
    sb_init(&b);
    sb_add(&b, "CREATE TABLE IF NOT EXISTS ");
    sb_add(&b, table);
    sb_add(&b, " ");
    sb_add(&b, paren ? paren : "(");
    return sb_json(&b);
}

/* PoP: w29_hostedroom_8927a5_p1_leases_table_ddl @ hosted_room_driver.py:_initialize_schema */
json_t *w29_hostedroom_8927a5_p1_leases_table_ddl(void) { return json_string(SQL_INITIALIZE[1]); }

/* PoP: w29_hostedroom_8927a5_p1_schema_sql @ hosted_room_driver.py:_initialize_store */
json_t *w29_hostedroom_8927a5_p1_schema_sql(int objects_exist) {
    const char *const *list = objects_exist ? SQL_MIGRATE : SQL_INITIALIZE;
    size_t n = objects_exist ? SQL_MIGRATE_N : SQL_INITIALIZE_N;
    size_t i;
    json_t *a = json_array();
    for (i = 0; i < n; i++) json_append(a, json_string(list[i]));
    return a;
}

/* PoP: w29_hostedroom_8927a5_p1_step_mode @ hosted_room_driver.py:_initialize_store */
json_t *w29_hostedroom_8927a5_p1_step_mode(int objects_exist) {
    return json_string(objects_exist ? "migrate" : "initialize");
}

/* PoP: w29_hostedroom_8927a5_p1_schema_objects_exist @ hosted_room_driver.py:_schema_objects_exist */
json_t *w29_hostedroom_8927a5_p1_schema_objects_exist(const json_t *tables, int has_index) {
    static const char *const want[] = { "hosted_room_driver_leases", "hosted_room_driver_tasks" };
    int both = set_equals(tables, want, 2);
    return json_bool(both && has_index);
}

/* PoP: w29_hostedroom_8927a5_p1_validate_schema @ hosted_room_driver.py:_validate_schema */
json_t *w29_hostedroom_8927a5_p1_validate_schema(const json_t *lease_columns, const json_t *task_columns,
                                                 int lease_fk, int task_fk) {
    if (!set_equals(lease_columns, LEASE_COLUMNS, LEASE_COLUMNS_N) ||
        !set_equals(task_columns, TASK_COLUMNS, TASK_COLUMNS_N))
        return err_obj("DriverStateError",
                       "unsupported unpublished hosted-room driver schema; "
                       "recreate the driver tables before starting the driver");
    if (!lease_fk) return err_obj("DriverStateError", "hosted_room_driver_leases is missing its hosted_rooms foreign key");
    if (!task_fk) return err_obj("DriverStateError", "hosted_room_driver_tasks is missing its hosted_rooms foreign key");
    return json_null();
}

/* PoP: w29_hostedroom_8927a5_p1_schema_ready @ hosted_room_driver.py:_schema_ready */
json_t *w29_hostedroom_8927a5_p1_schema_ready(const json_t *req) {
    const json_t *tables = jget(req, "tables");
    const json_t *task_sql = jget(req, "task_sql");
    int has_index = jgetbool(req, "has_index", 0);
    json_t *schema_err;
    json_t *out;
    const char *sql;
    char *lower;
    size_t i;

    out = json_object();
    if (!jtruthy(w29_hostedroom_8927a5_p1_schema_objects_exist(tables, has_index))) {
        json_set(out, "ready", json_bool(0));
        json_set(out, "error", json_null());
        json_set(out, "message", json_null());
        return out;
    }
    sql = task_sql && task_sql->type == JSON_STRING ? task_sql->str_val : "";
    if (!sql) sql = "";
    lower = (char *)malloc(strlen(sql) + 1);
    if (!lower) { json_free(out); return json_null(); }
    for (i = 0; sql[i]; i++) {
        char c = sql[i];
        lower[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    lower[i] = '\0';
    if (!strstr(lower, "'stopping'") || !strstr(lower, "'deferred'")) {
        free(lower);
        json_set(out, "ready", json_bool(0));
        json_set(out, "error", json_null());
        json_set(out, "message", json_null());
        return out;
    }
    free(lower);
    schema_err = w29_hostedroom_8927a5_p1_validate_schema(
        jget(req, "lease_columns"), jget(req, "task_columns"), jgetbool(req, "lease_fk", 0),
        jgetbool(req, "task_fk", 0));
    if (!jnone(schema_err)) {
        json_set(out, "ready", json_bool(0));
        json_set(out, "error", json_copy(jget(schema_err, "error")));
        json_set(out, "message", json_copy(jget(schema_err, "message")));
        json_free(schema_err);
        return out;
    }
    json_set(out, "ready", json_bool(1));
    json_set(out, "error", json_null());
    json_set(out, "message", json_null());
    return out;
}

/* ══ gate p2: lease lifecycle and row predicates ═══════════════════════════ */

/* PoP: w29_hostedroom_8927a5_p2_run_fence @ hosted_room_driver.py:_run_fence */
json_t *w29_hostedroom_8927a5_p2_run_fence(const json_t *lease) {
    json_t *a = json_array();
    const json_t *gateway = jget(lease, "gateway_id");
    const json_t *process = jget(lease, "process_generation");
    json_append(a, gateway ? json_copy(gateway) : json_null());
    json_append(a, process ? json_copy(process) : json_null());
    json_append(a, json_from_long(jgetint(lease, "lease_generation", 0)));
    return a;
}

/* PoP: w29_hostedroom_8927a5_p2_lease_from_row @ hosted_room_driver.py:_lease_from_row */
json_t *w29_hostedroom_8927a5_p2_lease_from_row(const json_t *row, int reclaimed) {
    json_t *o = json_object();
    json_set(o, "room_id", jget(row, "room_id") ? json_copy(jget(row, "room_id")) : json_null());
    json_set(o, "gateway_id", jget(row, "gateway_id") ? json_copy(jget(row, "gateway_id")) : json_null());
    json_set(o, "authority_epoch", json_from_long(jgetint(row, "authority_epoch", 0)));
    json_set(o, "process_generation",
             jget(row, "process_generation") ? json_copy(jget(row, "process_generation")) : json_null());
    json_set(o, "lease_generation", json_from_long(jgetint(row, "lease_generation", 0)));
    json_set(o, "expires_at", json_number(jgetnum(row, "expires_at", 0.0)));
    json_set(o, "reclaimed", json_bool(reclaimed ? 1 : 0));
    return o;
}

/* PoP: w29_hostedroom_8927a5_p2_lease_row_matches @ hosted_room_driver.py:_lease_row_matches */
int w29_hostedroom_8927a5_p2_lease_row_matches(const json_t *row, const json_t *lease) {
    if (jnone(row)) return 0;
    return py_eq_key(row, "gateway_id", jget(lease, "gateway_id")) &&
           jgetint(row, "authority_epoch", 0) == jgetint(lease, "authority_epoch", 0) &&
           py_eq_key(row, "process_generation", jget(lease, "process_generation")) &&
           jgetint(row, "lease_generation", 0) == jgetint(lease, "lease_generation", 0);
}

/* PoP: w29_hostedroom_8927a5_p2_task_identity @ hosted_room_driver.py:_task_identity_from_row */
json_t *w29_hostedroom_8927a5_p2_task_identity(const json_t *row) {
    json_t *o = json_object();
    json_set(o, "room_id", jget(row, "room_id") ? json_copy(jget(row, "room_id")) : json_null());
    json_set(o, "task_id", jget(row, "task_id") ? json_copy(jget(row, "task_id")) : json_null());
    json_set(o, "thread_id", jget(row, "thread_id") ? json_copy(jget(row, "thread_id")) : json_null());
    json_set(o, "turn_id", jget(row, "turn_id") ? json_copy(jget(row, "turn_id")) : json_null());
    return o;
}

static int identity_matches(const json_t *row, const json_t *identity) {
    return py_eq_key(row, "room_id", jget(identity, "room_id")) &&
           py_eq_key(row, "task_id", jget(identity, "task_id")) &&
           py_eq_key(row, "thread_id", jget(identity, "thread_id")) &&
           py_eq_key(row, "turn_id", jget(identity, "turn_id"));
}

/* PoP: w29_hostedroom_8927a5_p2_load_task @ hosted_room_driver.py:_load_task */
json_t *w29_hostedroom_8927a5_p2_load_task(const json_t *row, const json_t *identity, int required) {
    if (jnone(row)) {
        if (required) return err_obj("TaskConflictError", "task does not exist");
        return json_null();
    }
    if (!identity_matches(row, identity))
        return err_obj("TaskConflictError", "task_id is already bound to a different turn");
    return json_null();
}

/* PoP: w29_hostedroom_8927a5_p2_active_room @ hosted_room_driver.py:_load_active_room */
json_t *w29_hostedroom_8927a5_p2_active_room(const json_t *room, int no_table) {
    if (no_table || jnone(room)) return err_obj("RoomUnavailableError", "hosted room does not exist");
    /* row["disbanded_at"] is an INDEX, not .get(): a row without the key raises
     * KeyError rather than reading as "not disbanded". */
    if (room && room->type == JSON_OBJECT && !json_has(room, "disbanded_at"))
        return err_obj("KeyError", "'disbanded_at'");
    if (!jnone(jget(room, "disbanded_at"))) return err_obj("RoomUnavailableError", "hosted room is disbanded");
    return json_null();
}

/* PoP: w29_hostedroom_8927a5_p2_room_authority @ hosted_room_driver.py:_require_room_authority */
json_t *w29_hostedroom_8927a5_p2_room_authority(const json_t *room, const char *gateway_id, long long epoch) {
    json_t *err = w29_hostedroom_8927a5_p2_active_room(room, 0);
    const char *room_gateway;
    if (!jnone(err)) return err;
    room_gateway = jstr(room, "authority_gateway_id");
    if (!room_gateway || !gateway_id || strcmp(room_gateway, gateway_id) != 0 ||
        jgetint(room, "authority_epoch", -1) != epoch)
        return err_obj("StaleLeaseError", "hosted room authority changed");
    return json_null();
}

/* PoP: w29_hostedroom_8927a5_p2_lease_gate @ hosted_room_driver.py:_require_active_lease */
json_t *w29_hostedroom_8927a5_p2_lease_gate(const json_t *room, const json_t *lease_row, const json_t *lease,
                                            double now, int allow_expired) {
    json_t *err = w29_hostedroom_8927a5_p2_room_authority(
        room, jstr(lease, "gateway_id"), jgetint(lease, "authority_epoch", 0));
    if (!jnone(err)) return err;
    if (!w29_hostedroom_8927a5_p2_lease_row_matches(lease_row, lease) ||
        !jnone(jget(lease_row, "released_at")) ||
        (!allow_expired && jgetnum(lease_row, "expires_at", 0.0) <= now))
        return err_obj("StaleLeaseError", "driver lease is stale or expired");
    return json_null();
}

/* PoP: w29_hostedroom_8927a5_p2_generations_match @ hosted_room_driver.py:_generations_match */
int w29_hostedroom_8927a5_p2_generations_match(const json_t *row, const char *status, long long execution_generation,
                                               long long cancel_generation) {
    const char *row_status = jstr(row, "status");
    return row_status && status && !strcmp(row_status, status) &&
           jgetint(row, "execution_generation", 0) == execution_generation &&
           jgetint(row, "cancel_generation", 0) == cancel_generation;
}

/* PoP: w29_hostedroom_8927a5_p2_cancel_generation_guard @ hosted_room_driver.py:_require_cancel_generation */
json_t *w29_hostedroom_8927a5_p2_cancel_generation_guard(const json_t *row, long long expected) {
    if (jgetint(row, "cancel_generation", 0) != expected)
        return err_obj("StaleTaskError", "task cancellation generation changed");
    return json_null();
}

/* PoP: w29_hostedroom_8927a5_p2_acquire @ hosted_room_driver.py:acquire_lease */
json_t *w29_hostedroom_8927a5_p2_acquire(const json_t *req) {
    const json_t *room = jget(req, "room");
    const json_t *row = jget(req, "lease_row");
    const char *gateway_id = jstr(req, "gateway_id");
    const char *process_generation = jstr(req, "process_generation");
    long long authority_epoch = jgetint(req, "authority_epoch", 0);
    double now = jgetnum(req, "now", 0.0);
    double expires_at = now + jgetnum(req, "ttl_seconds", 0.0);
    json_t *out, *params, *err;

    out = json_object();
    err = w29_hostedroom_8927a5_p2_room_authority(room, gateway_id, authority_epoch);
    if (!jnone(err)) {
        json_set(out, "outcome", json_string("error"));
        json_set(out, "error", json_copy(jget(err, "error")));
        json_set(out, "message", json_copy(jget(err, "message")));
        json_set(out, "params", json_null());
        json_free(err);
        return out;
    }
    if (jnone(row)) {
        params = json_array();
        json_append(params, json_copy(jget(req, "room_id")));
        json_append(params, json_string(gateway_id ? gateway_id : ""));
        json_append(params, json_from_long(authority_epoch));
        json_append(params, json_string(process_generation ? process_generation : ""));
        json_append(params, json_number(expires_at));
        json_append(params, json_number(now));
        json_append(params, json_number(now));
        json_set(out, "outcome", json_string("insert"));
        json_set(out, "error", json_null());
        json_set(out, "message", json_null());
        json_set(out, "params", params);
        return out;
    }
    {
        const char *row_gateway = jstr(row, "gateway_id");
        int same_authority = row_gateway && gateway_id && !strcmp(row_gateway, gateway_id) &&
                             jgetint(row, "authority_epoch", -1) == authority_epoch;
        int live = jnone(jget(row, "released_at")) && jgetnum(row, "expires_at", 0.0) > now;
        const char *row_pg = jstr(row, "process_generation");
        if (same_authority && row_pg && process_generation && !strcmp(row_pg, process_generation) && live) {
            double renewed = jgetnum(row, "expires_at", 0.0) > expires_at ? jgetnum(row, "expires_at", 0.0)
                                                                          : expires_at;
            params = json_array();
            json_append(params, json_number(renewed));
            json_append(params, json_number(now));
            json_append(params, json_copy(jget(req, "room_id")));
            json_append(params, json_from_long(jgetint(row, "lease_generation", 0)));
            json_set(out, "outcome", json_string("renew"));
            json_set(out, "error", json_null());
            json_set(out, "message", json_null());
            json_set(out, "params", params);
            return out;
        }
        if (same_authority && live) {
            json_set(out, "outcome", json_string("held"));
            json_set(out, "error", json_string("LeaseHeldError"));
            json_set(out, "message", json_string("room driver lease is held by another generation"));
            json_set(out, "params", json_null());
            return out;
        }
        if (jgetint(req, "rowcount", 1) != 1) {
            json_set(out, "outcome", json_string("held"));
            json_set(out, "error", json_string("LeaseHeldError"));
            json_set(out, "message", json_string("room driver lease changed during acquisition"));
            json_set(out, "params", json_null());
            return out;
        }
        params = json_array();
        json_append(params, json_string(gateway_id ? gateway_id : ""));
        json_append(params, json_from_long(authority_epoch));
        json_append(params, json_string(process_generation ? process_generation : ""));
        json_append(params, json_number(expires_at));
        json_append(params, json_number(now));
        json_append(params, json_number(now));
        json_append(params, json_copy(jget(req, "room_id")));
        json_append(params, json_from_long(jgetint(row, "lease_generation", 0)));
        json_append(params, json_string(gateway_id ? gateway_id : ""));
        json_append(params, json_from_long(authority_epoch));
        json_append(params, json_number(now));
        json_set(out, "outcome", json_string("reclaim"));
        json_set(out, "error", json_null());
        json_set(out, "message", json_null());
        json_set(out, "params", params);
        return out;
    }
}

/* PoP: w29_hostedroom_8927a5_p2_renew @ hosted_room_driver.py:renew_lease */
json_t *w29_hostedroom_8927a5_p2_renew(const json_t *req) {
    const json_t *lease = jget(req, "lease");
    const json_t *current = jget(req, "lease_row");
    double now = jgetnum(req, "now", 0.0);
    double requested = now + jgetnum(req, "ttl_seconds", 0.0);
    double expires_at;
    json_t *out = json_object();
    json_t *err = w29_hostedroom_8927a5_p2_lease_gate(jget(req, "room"), current, lease, now, 0);
    json_t *params;
    if (!jnone(err)) {
        json_set(out, "outcome", json_string("error"));
        json_set(out, "error", json_copy(jget(err, "error")));
        json_set(out, "message", json_copy(jget(err, "message")));
        json_set(out, "params", json_null());
        json_free(err);
        return out;
    }
    expires_at = jgetnum(current, "expires_at", 0.0) > requested ? jgetnum(current, "expires_at", 0.0) : requested;
    if (jgetint(req, "rowcount", 1) != 1) {
        json_set(out, "outcome", json_string("error"));
        json_set(out, "error", json_string("StaleLeaseError"));
        json_set(out, "message", json_string("driver lease changed during renewal"));
        json_set(out, "params", json_null());
        return out;
    }
    params = json_array();
    json_append(params, json_number(expires_at));
    json_append(params, json_number(now));
    json_append(params, json_copy(jget(lease, "room_id")));
    json_append(params, json_copy(jget(lease, "gateway_id")));
    json_append(params, json_copy(jget(lease, "process_generation")));
    json_append(params, json_from_long(jgetint(lease, "lease_generation", 0)));
    json_append(params, json_number(now));
    json_set(out, "outcome", json_string("renewed"));
    json_set(out, "error", json_null());
    json_set(out, "message", json_null());
    json_set(out, "params", params);
    return out;
}

/* PoP: w29_hostedroom_8927a5_p2_release @ hosted_room_driver.py:release_lease */
json_t *w29_hostedroom_8927a5_p2_release(const json_t *req) {
    const json_t *lease = jget(req, "lease");
    const json_t *room = jget(req, "room");
    const json_t *row = jget(req, "lease_row");
    double now = jgetnum(req, "now", 0.0);
    json_t *out = json_object();
    json_t *err = w29_hostedroom_8927a5_p2_room_authority(
        room, jstr(lease, "gateway_id"), jgetint(lease, "authority_epoch", 0));
    json_t *params;

    if (!jnone(err)) {
        json_set(out, "outcome", json_string("error"));
        json_set(out, "error", json_copy(jget(err, "error")));
        json_set(out, "message", json_copy(jget(err, "message")));
        json_set(out, "params", json_null());
        json_free(err);
        return out;
    }
    if (!w29_hostedroom_8927a5_p2_lease_row_matches(row, lease)) {
        json_set(out, "outcome", json_string("error"));
        json_set(out, "error", json_string("StaleLeaseError"));
        json_set(out, "message", json_string("driver lease is stale"));
        json_set(out, "params", json_null());
        return out;
    }
    if (!jnone(jget(row, "released_at"))) {
        json_set(out, "outcome", json_string("idempotent"));
        json_set(out, "error", json_null());
        json_set(out, "message", json_null());
        json_set(out, "params", json_null());
        return out;
    }
    if (jgetnum(row, "expires_at", 0.0) <= now) {
        json_set(out, "outcome", json_string("error"));
        json_set(out, "error", json_string("StaleLeaseError"));
        json_set(out, "message", json_string("driver lease expired before release"));
        json_set(out, "params", json_null());
        return out;
    }
    if (jgetbool(req, "has_running", 0)) {
        json_set(out, "outcome", json_string("error"));
        json_set(out, "error", json_string("InvalidTaskTransitionError"));
        json_set(out, "message", json_string("cannot release a room lease while tasks are running"));
        json_set(out, "params", json_null());
        return out;
    }
    params = json_array();
    json_append(params, json_number(now));
    json_append(params, json_number(now));
    json_append(params, json_number(now));
    json_append(params, json_copy(jget(lease, "room_id")));
    json_append(params, json_from_long(jgetint(lease, "lease_generation", 0)));
    json_set(out, "outcome", json_string("released"));
    json_set(out, "error", json_null());
    json_set(out, "message", json_null());
    json_set(out, "params", params);
    return out;
}

/* ══ gate p3: task state machine, replays and the task view ════════════════ */

/* PoP: w29_hostedroom_8927a5_p3_task_view @ hosted_room_driver.py:_task_from_row */
json_t *w29_hostedroom_8927a5_p3_task_view(const json_t *row, int idempotent) {
    const char *payload_json = jstr(row, "payload_json");
    json_t *payload = payload_json ? json_parse(payload_json, NULL) : NULL;
    json_t *task;
    size_t i;
    static const char *const cast_int[] = { "execution_generation", "cancel_generation" };
    static const char *const cast_opt_int[] = { "run_lease_generation" };
    static const char *const cast_float[] = { "created_at", "updated_at" };
    static const char *const cast_opt_float[] = { "started_at", "terminal_at", "indeterminate_at" };

    if (!payload) return err_obj("TaskConflictError", "stored task payload is invalid");
    task = json_object();
    json_set(task, "identity", w29_hostedroom_8927a5_p2_task_identity(row));
    json_set(task, "payload", payload);
    for (i = 6; i < TASK_COLUMNS_N; i++) {   /* columns from payload_digest onward */
        const char *column = TASK_COLUMNS[i];
        const json_t *value = jget(row, column);
        size_t j;
        if (!strcmp(column, "result_json")) {
            json_set(task, "result", jnone(value) ? json_null()
                                                  : (value->type == JSON_STRING ? json_parse(value->str_val, NULL)
                                                                                : json_copy(value)));
            continue;
        }
        for (j = 0; j < sizeof cast_int / sizeof cast_int[0]; j++) {
            if (!strcmp(column, cast_int[j])) { json_set(task, column, json_from_long(jgetint(row, column, 0))); break; }
        }
        if (j < sizeof cast_int / sizeof cast_int[0]) continue;
        for (j = 0; j < sizeof cast_opt_int / sizeof cast_opt_int[0]; j++) {
            if (!strcmp(column, cast_opt_int[j])) {
                json_set(task, column, jnone(value) ? json_null() : json_from_long(jgetint(row, column, 0)));
                break;
            }
        }
        if (j < sizeof cast_opt_int / sizeof cast_opt_int[0]) continue;
        for (j = 0; j < sizeof cast_float / sizeof cast_float[0]; j++) {
            if (!strcmp(column, cast_float[j])) { json_set(task, column, json_number(jgetnum(row, column, 0.0))); break; }
        }
        if (j < sizeof cast_float / sizeof cast_float[0]) continue;
        for (j = 0; j < sizeof cast_opt_float / sizeof cast_opt_float[0]; j++) {
            if (!strcmp(column, cast_opt_float[j])) {
                json_set(task, column, jnone(value) ? json_null() : json_number(jgetnum(row, column, 0.0)));
                break;
            }
        }
        if (j < sizeof cast_opt_float / sizeof cast_opt_float[0]) continue;
        json_set(task, column, jnone(value) ? json_null() : json_copy(value));
    }
    json_set(task, "idempotent", json_bool(idempotent ? 1 : 0));
    return task;
}

/* PoP: w29_hostedroom_8927a5_p3_settlement_params @ hosted_room_driver.py:_settlement */
json_t *w29_hostedroom_8927a5_p3_settlement_params(const char *settlement_id, const char *status,
                                                   const json_t *result, double now) {
    json_t *a = json_array();
    json_t *result_json = compact_json(result ? result : json_null());
    json_append(a, json_string(status ? status : ""));
    json_append(a, json_string(settlement_id ? settlement_id : ""));
    json_append(a, json_string(status ? status : ""));
    json_append(a, result_json);
    json_append(a, json_number(now));
    json_append(a, json_number(now));
    return a;
}

/* PoP: w29_hostedroom_8927a5_p3_settlement_replay @ hosted_room_driver.py:_settlement.replay */
json_t *w29_hostedroom_8927a5_p3_settlement_replay(const json_t *row, const char *settlement_id, const char *status,
                                                   const json_t *result, double now) {
    json_t *out = json_object();
    json_t *result_json;
    (void)now;
    if (jnone(jget(row, "settlement_id"))) {
        json_set(out, "outcome", json_string("none"));
        return out;
    }
    result_json = compact_json(result ? result : json_null());
    if (py_eq_key(row, "settlement_id", json_string(settlement_id ? settlement_id : "")) &&
        py_eq_key(row, "settlement_status", json_string(status ? status : "")) &&
        py_eq_key(row, "result_json", result_json)) {
        json_set(out, "outcome", json_string("idempotent"));
        json_set(out, "task", w29_hostedroom_8927a5_p3_task_view(row, 1));
        json_free(result_json);
        return out;
    }
    json_free(result_json);
    json_set(out, "outcome", json_string("conflict"));
    json_set(out, "error", json_string("TaskConflictError"));
    json_set(out, "message", json_string("task already has a different terminal settlement"));
    return out;
}

/* PoP: w29_hostedroom_8927a5_p3_cancel_replay @ hosted_room_driver.py:_cancel_replay */
json_t *w29_hostedroom_8927a5_p3_cancel_replay(const json_t *row, const char *cancel_id, const char *status) {
    json_t *out = json_object();
    const char *row_status = jstr(row, "status");
    const char *row_cancel = jstr(row, "cancel_id");
    if (row_status && status && !strcmp(row_status, status) && row_cancel && cancel_id &&
        !strcmp(row_cancel, cancel_id)) {
        json_set(out, "outcome", json_string("idempotent"));
        json_set(out, "task", w29_hostedroom_8927a5_p3_task_view(row, 1));
        return out;
    }
    json_set(out, "outcome", json_string("none"));
    return out;
}

/* PoP: w29_hostedroom_8927a5_p3_defer_result @ hosted_room_driver.py:_defer */
json_t *w29_hostedroom_8927a5_p3_defer_result(const char *reason) {
    json_t *o = json_object();
    json_t *out;
    json_set(o, "reason", json_string(reason ? reason : ""));
    json_set(o, "retryable", json_bool(1));
    out = compact_json(o);
    json_free(o);
    return out;
}

/* PoP: w29_hostedroom_8927a5_p3_admit @ hosted_room_driver.py:admit_task */
json_t *w29_hostedroom_8927a5_p3_admit(const json_t *req) {
    const json_t *existing = jget(req, "existing");
    const json_t *thread_row = jget(req, "thread_row");
    const char *payload_json = jstr(req, "payload_json");
    const char *payload_digest = jstr(req, "payload_digest");
    const json_t *identity = jget(req, "identity");
    json_t *out = json_object();
    json_t *err = w29_hostedroom_8927a5_p2_active_room(jget(req, "room"), jgetbool(req, "no_room_table", 0));

    if (!jnone(err)) {
        json_set(out, "outcome", json_string("error"));
        json_set(out, "error", json_copy(jget(err, "error")));
        json_set(out, "message", json_copy(jget(err, "message")));
        json_free(err);
        return out;
    }
    if (!jnone(existing)) {
        const char *row_digest = jstr(existing, "payload_digest");
        const char *row_json = jstr(existing, "payload_json");
        if (!row_digest || !payload_digest || strcmp(row_digest, payload_digest) != 0 || !row_json ||
            !payload_json || strcmp(row_json, payload_json) != 0) {
            json_set(out, "outcome", json_string("error"));
            json_set(out, "error", json_string("TaskConflictError"));
            json_set(out, "message", json_string("task_id is already bound to a different payload"));
            return out;
        }
        json_set(out, "outcome", json_string("replay"));
        json_set(out, "task", w29_hostedroom_8927a5_p3_task_view(existing, 1));
        return out;
    }
    if (!jnone(thread_row)) {
        json_set(out, "outcome", json_string("error"));
        json_set(out, "error", json_string("TaskConflictError"));
        json_set(out, "message", json_string("thread_id and turn_id are already bound to a task"));
        return out;
    }
    {
        json_t *params = json_array();
        double now = jgetnum(req, "now", 0.0);
        json_append(params, json_copy(jget(identity, "room_id")));
        json_append(params, json_copy(jget(identity, "task_id")));
        json_append(params, json_copy(jget(identity, "thread_id")));
        json_append(params, json_copy(jget(identity, "turn_id")));
        json_append(params, json_from_long(jgetint(req, "source_event_seq", 0)));
        json_append(params, json_string(payload_json ? payload_json : ""));
        json_append(params, json_string(payload_digest ? payload_digest : ""));
        json_append(params, json_number(now));
        json_append(params, json_number(now));
        json_set(out, "outcome", json_string("insert"));
        json_set(out, "params", params);
        return out;
    }
}

/* PoP: w29_hostedroom_8927a5_p3_start @ hosted_room_driver.py:start_task */
json_t *w29_hostedroom_8927a5_p3_start(const json_t *req) {
    const json_t *row = jget(req, "task_row");
    const json_t *lease = jget(req, "lease");
    const json_t *identity = jget(req, "identity");
    double now = jgetnum(req, "now", 0.0);
    long long expected_cancel_generation = jgetint(req, "expected_cancel_generation", 0);
    json_t *out = json_object();
    json_t *err;

    err = w29_hostedroom_8927a5_p2_lease_gate(jget(req, "room"), jget(req, "lease_row"), lease, now, 0);
    if (!jnone(err)) goto error;
    err = w29_hostedroom_8927a5_p2_load_task(row, identity, 1);
    if (!jnone(err)) goto error;
    err = w29_hostedroom_8927a5_p2_cancel_generation_guard(row, expected_cancel_generation);
    if (!jnone(err)) goto error;
    {
        const char *status = jstr(row, "status");
        if (!status || strcmp(status, "queued") != 0) {
            sb b;
            char *msg;
            sb_init(&b);
            sb_add(&b, "cannot start task in state '");
            sb_add(&b, status ? status : "");
            sb_add(&b, "'");
            msg = sb_take(&b);
            err = err_obj("InvalidTaskTransitionError", msg);
            free(msg);
            goto error;
        }
    }
    if (jgetbool(req, "has_blocker", 0)) {
        err = err_obj("InvalidTaskTransitionError",
                      "room recovery must resolve the prior task before starting new work");
        goto error;
    }
    {
        const char *next_id = jstr(req, "next_task_id");
        const char *task_id = jstr(identity, "task_id");
        if (!next_id || !task_id || strcmp(next_id, task_id) != 0) {
            err = err_obj("InvalidTaskTransitionError", "task is not next in the hosted room event order");
            goto error;
        }
    }
    if (jgetint(req, "rowcount", 1) != 1) {
        err = err_obj("StaleTaskError", "task changed during start");
        goto error;
    }
    {
        long long execution_generation = jgetint(row, "execution_generation", 0) + 1;
        json_t *params = json_array();
        json_append(params, json_from_long(execution_generation));
        json_append(params, json_copy(jget(lease, "gateway_id")));
        json_append(params, json_copy(jget(lease, "process_generation")));
        json_append(params, json_from_long(jgetint(lease, "lease_generation", 0)));
        json_append(params, json_number(now));
        json_append(params, json_number(now));
        json_append(params, json_copy(jget(identity, "room_id")));
        json_append(params, json_copy(jget(identity, "task_id")));
        json_append(params, json_from_long(expected_cancel_generation));
        json_set(out, "outcome", json_string("started"));
        json_set(out, "execution_generation", json_from_long(execution_generation));
        json_set(out, "params", params);
        return out;
    }
error:
    json_set(out, "outcome", json_string("error"));
    json_set(out, "error", json_copy(jget(err, "error")));
    json_set(out, "message", json_copy(jget(err, "message")));
    json_free(err);
    return out;
}

/* PoP: w29_hostedroom_8927a5_p3_cancel @ hosted_room_driver.py:cancel_task */
json_t *w29_hostedroom_8927a5_p3_cancel(const json_t *req) {
    const json_t *row = jget(req, "task_row");
    const json_t *identity = jget(req, "identity");
    const char *cancel_id = jstr(req, "cancel_id");
    long long expected = jgetint(req, "expected_cancel_generation", 0);
    double now = jgetnum(req, "now", 0.0);
    json_t *out = json_object();
    json_t *err;
    const char *status;

    err = w29_hostedroom_8927a5_p2_load_task(row, identity, 1);
    if (!jnone(err)) goto error;
    {
        json_t *replay = w29_hostedroom_8927a5_p3_cancel_replay(row, cancel_id, "cancelled");
        if (!strcmp(jstr(replay, "outcome") ? jstr(replay, "outcome") : "", "idempotent")) {
            json_set(out, "outcome", json_string("replayed"));
            json_set(out, "task", json_copy(jget(replay, "task")));
            json_free(replay);
            return out;
        }
        json_free(replay);
    }
    status = jstr(row, "status");
    if (status && (!strcmp(status, "settled") || !strcmp(status, "failed") || !strcmp(status, "cancelled"))) {
        sb b;
        char *msg;
        sb_init(&b);
        sb_add(&b, "cannot cancel task in state '");
        sb_add(&b, status);
        sb_add(&b, "'");
        msg = sb_take(&b);
        err = err_obj("InvalidTaskTransitionError", msg);
        free(msg);
        goto error;
    }
    if (!status || (strcmp(status, "queued") != 0 && strcmp(status, "deferred") != 0)) {
        err = err_obj("InvalidTaskTransitionError", "running work requires acknowledged two-phase cancellation");
        goto error;
    }
    err = w29_hostedroom_8927a5_p2_cancel_generation_guard(row, expected);
    if (!jnone(err)) goto error;
    if (jgetint(req, "rowcount", 1) != 1) {
        err = err_obj("StaleTaskError", "task changed during cancellation");
        goto error;
    }
    {
        json_t *params = json_array();
        json_append(params, json_from_long(expected + 1));
        json_append(params, json_string(cancel_id ? cancel_id : ""));
        json_append(params, json_number(now));
        json_append(params, json_number(now));
        json_append(params, json_copy(jget(identity, "room_id")));
        json_append(params, json_copy(jget(identity, "task_id")));
        json_append(params, json_from_long(expected));
        json_set(out, "outcome", json_string("cancelled"));
        json_set(out, "params", params);
        return out;
    }
error:
    json_set(out, "outcome", json_string("error"));
    json_set(out, "error", json_copy(jget(err, "error")));
    json_set(out, "message", json_copy(jget(err, "message")));
    json_free(err);
    return out;
}

/* PoP: w29_hostedroom_8927a5_p3_settle @ hosted_room_driver.py:settle_task */
json_t *w29_hostedroom_8927a5_p3_settle(const json_t *req) {
    const json_t *row = jget(req, "task_row");
    const json_t *attempt = jget(req, "attempt");
    const json_t *lease = jget(req, "lease");
    const json_t *identity = jget(attempt, "identity");
    const char *settlement_id = jstr(req, "settlement_id");
    const char *status = jstr(req, "status");
    double now = jgetnum(req, "now", 0.0);
    long long execution_generation = jgetint(attempt, "execution_generation", 0);
    long long cancel_generation = jgetint(attempt, "cancel_generation", 0);
    json_t *out = json_object();
    json_t *err;

    err = w29_hostedroom_8927a5_p2_load_task(row, identity, 1);
    if (!jnone(err)) goto error;
    {
        json_t *replay = w29_hostedroom_8927a5_p3_settlement_replay(row, settlement_id, status,
                                                                    jget(req, "result"), now);
        const char *outcome = jstr(replay, "outcome");
        if (outcome && !strcmp(outcome, "idempotent")) {
            json_set(out, "outcome", json_string("replayed"));
            json_set(out, "task", json_copy(jget(replay, "task")));
            json_free(replay);
            return out;
        }
        if (outcome && !strcmp(outcome, "conflict")) {
            json_set(out, "outcome", json_string("error"));
            json_set(out, "error", json_copy(jget(replay, "error")));
            json_set(out, "message", json_copy(jget(replay, "message")));
            json_free(replay);
            return out;
        }
        json_free(replay);
    }
    err = w29_hostedroom_8927a5_p2_lease_gate(jget(req, "room"), jget(req, "lease_row"), lease, now,
                                              jgetbool(req, "allow_expired", 0));
    if (!jnone(err)) goto error;
    {
        const char *run_gateway = jstr(row, "run_gateway_id");
        const char *run_process = jstr(row, "run_process_generation");
        const char *lease_gateway = jstr(lease, "gateway_id");
        const char *lease_process = jstr(lease, "process_generation");
        int fence_ok = run_gateway && lease_gateway && !strcmp(run_gateway, lease_gateway) && run_process &&
                       lease_process && !strcmp(run_process, lease_process) &&
                       jgetint(row, "run_lease_generation", 0) == jgetint(lease, "lease_generation", 0);
        if (!w29_hostedroom_8927a5_p2_generations_match(row, "running", execution_generation, cancel_generation) ||
            !fence_ok) {
            err = err_obj("StaleTaskError", "task attempt is stale or cancelled");
            goto error;
        }
    }
    if (jgetint(req, "rowcount", 1) != 1) {
        err = err_obj("StaleTaskError", "task changed during settlement");
        goto error;
    }
    {
        json_t *params = w29_hostedroom_8927a5_p3_settlement_params(settlement_id, status, jget(req, "result"), now);
        json_append(params, json_copy(jget(identity, "room_id")));
        json_append(params, json_copy(jget(identity, "task_id")));
        json_append(params, json_from_long(execution_generation));
        json_append(params, json_from_long(cancel_generation));
        json_append(params, json_copy(jget(lease, "gateway_id")));
        json_append(params, json_copy(jget(lease, "process_generation")));
        json_append(params, json_from_long(jgetint(lease, "lease_generation", 0)));
        json_set(out, "outcome", json_string("settled"));
        json_set(out, "params", params);
        return out;
    }
error:
    json_set(out, "outcome", json_string("error"));
    json_set(out, "error", json_copy(jget(err, "error")));
    json_set(out, "message", json_copy(jget(err, "message")));
    json_free(err);
    return out;
}

/* PoP: w29_hostedroom_8927a5_p3_generation @ hosted_room_driver.py:<_GENERATION_TRANSITIONS verbs> */
json_t *w29_hostedroom_8927a5_p3_generation(const json_t *req) {
    const char *name = jstr(req, "name");
    const json_t *row = jget(req, "task_row");
    const json_t *lease = jget(req, "lease");
    const json_t *identity = jget(req, "identity");
    double now = jgetnum(req, "now", 0.0);
    long long execution_generation = jgetint(req, "execution_generation", 0);
    long long cancel_generation = jgetint(req, "cancel_generation", 0);
    json_t *out = json_object();
    json_t *err;
    size_t i;
    const char *fenced_status = NULL, *generation_stale = NULL, *row_stale = NULL;

    for (i = 0; i < TRANSITIONS_N; i++) {
        if (name && !strcmp(TRANSITIONS[i][0], name)) {
            fenced_status = TRANSITIONS[i][1];
            generation_stale = TRANSITIONS[i][3];
            row_stale = TRANSITIONS[i][4];
            break;
        }
    }
    if (!fenced_status) {
        json_set(out, "outcome", json_string("error"));
        json_set(out, "error", json_string("KeyError"));
        json_set(out, "message", json_string("unknown generation transition"));
        return out;
    }
    err = w29_hostedroom_8927a5_p2_lease_gate(jget(req, "room"), jget(req, "lease_row"), lease, now, 0);
    if (!jnone(err)) goto error;
    err = w29_hostedroom_8927a5_p2_load_task(row, identity, 1);
    if (!jnone(err)) goto error;
    /* replay, when the verb has one */
    if (!strcmp(name, "resolve")) {
        json_t *replay = w29_hostedroom_8927a5_p3_settlement_replay(row, jstr(req, "settlement_id"),
                                                                    jstr(req, "status"), jget(req, "result"), now);
        const char *outcome = jstr(replay, "outcome");
        if (outcome && !strcmp(outcome, "idempotent")) {
            json_set(out, "outcome", json_string("replayed"));
            json_set(out, "task", json_copy(jget(replay, "task")));
            json_free(replay);
            return out;
        }
        if (outcome && !strcmp(outcome, "conflict")) {
            json_set(out, "outcome", json_string("error"));
            json_set(out, "error", json_copy(jget(replay, "error")));
            json_set(out, "message", json_copy(jget(replay, "message")));
            json_free(replay);
            return out;
        }
        json_free(replay);
    } else if (!strcmp(name, "resolve_cancel")) {
        json_t *replay = w29_hostedroom_8927a5_p3_cancel_replay(row, jstr(req, "cancel_id"), "cancelled");
        const char *outcome = jstr(replay, "outcome");
        if (outcome && !strcmp(outcome, "idempotent")) {
            json_set(out, "outcome", json_string("replayed"));
            json_set(out, "task", json_copy(jget(replay, "task")));
            json_free(replay);
            return out;
        }
        json_free(replay);
    } else if (!strcmp(name, "defer") || !strcmp(name, "defer_stopping")) {
        json_t *result_json = w29_hostedroom_8927a5_p3_defer_result(jstr(req, "reason"));
        const char *row_result = jstr(row, "result_json");
        int deferred = w29_hostedroom_8927a5_p2_generations_match(row, "deferred", execution_generation,
                                                                  cancel_generation);
        int same = row_result && result_json && !strcmp(row_result, result_json->str_val ? result_json->str_val : "");
        json_free(result_json);
        if (deferred && same) {
            json_set(out, "outcome", json_string("replayed"));
            json_set(out, "task", w29_hostedroom_8927a5_p3_task_view(row, 1));
            return out;
        }
    }
    if (!w29_hostedroom_8927a5_p2_generations_match(row, fenced_status, execution_generation, cancel_generation)) {
        err = err_obj("StaleTaskError", generation_stale);
        goto error;
    }
    if (jgetint(req, "rowcount", 1) != 1) {
        err = err_obj("StaleTaskError", row_stale);
        goto error;
    }
    {
        json_t *params = json_array();
        if (!strcmp(name, "resolve")) {
            json_t *base = w29_hostedroom_8927a5_p3_settlement_params(jstr(req, "settlement_id"), jstr(req, "status"),
                                                                      jget(req, "result"), now);
            size_t k;
            for (k = 0; k < json_len(base); k++) json_append(params, json_copy(json_get(base, k)));
            json_free(base);
        } else if (!strcmp(name, "resolve_cancel")) {
            json_append(params, json_from_long(cancel_generation + 1));
            json_append(params, json_copy(jget(req, "cancel_id")));
            json_append(params, json_number(now));
            json_append(params, json_number(now));
        } else if (!strcmp(name, "requeue") || !strcmp(name, "requeue_deferred")) {
            json_append(params, json_number(now));
        } else if (!strcmp(name, "defer") || !strcmp(name, "defer_stopping")) {
            json_append(params, w29_hostedroom_8927a5_p3_defer_result(jstr(req, "reason")));
            json_append(params, json_number(now));
            json_append(params, json_number(now));
        } else if (!strcmp(name, "reopen_deferred")) {
            json_append(params, json_number(now));
            json_append(params, json_number(now));
            json_append(params, json_number(now));
        }
        json_append(params, json_copy(jget(identity, "room_id")));
        json_append(params, json_copy(jget(identity, "task_id")));
        json_append(params, json_from_long(execution_generation));
        json_append(params, json_from_long(cancel_generation));
        json_set(out, "outcome", json_string("applied"));
        json_set(out, "params", params);
        return out;
    }
error:
    json_set(out, "outcome", json_string("error"));
    json_set(out, "error", json_copy(jget(err, "error")));
    json_set(out, "message", json_copy(jget(err, "message")));
    json_free(err);
    return out;
}

/* PoP: w29_hostedroom_8927a5_p3_prune @ hosted_room_driver.py:prune_published_terminal_tasks */
json_t *w29_hostedroom_8927a5_p3_prune(const json_t *req) {
    const json_t *rows = jget(req, "rows");
    double now = jgetnum(req, "now", 0.0);
    double cutoff = now - jgetnum(req, "retention_seconds", 0.0);
    long long retain = jgetint(req, "retain", 0);
    json_t *out = json_object();
    json_t *ids = json_array();
    size_t i, n, picked = 0;

    if (!jgetbool(req, "has_publications", 1)) {
        json_set(out, "count", json_from_long(0));
        json_set(out, "ids", ids);
        return out;
    }
    n = rows && rows->type == JSON_ARRAY ? json_len(rows) : 0;
    for (i = 0; i < n && picked < MAX_TASK_PRUNE_BATCH; i++) {
        const json_t *row = json_get(rows, i);
        const json_t *terminal_at = jget(row, "terminal_at");
        int stale_by_age = !jnone(terminal_at) && jnum(terminal_at, 0.0) <= cutoff;
        if ((long long)i >= retain || stale_by_age) {
            json_append(ids, json_copy(jget(row, "task_id")));
            picked++;
        }
    }
    json_set(out, "count", json_from_long((long long)picked));
    json_set(out, "ids", ids);
    return out;
}

/* PoP: w29_hostedroom_8927a5_p3_mark_indeterminate @ hosted_room_driver.py:mark_attempt_indeterminate */
json_t *w29_hostedroom_8927a5_p3_mark_indeterminate(const json_t *req) {
    const json_t *row = jget(req, "task_row");
    const json_t *attempt = jget(req, "attempt");
    const json_t *identity = jget(attempt, "identity");
    double now = jgetnum(req, "now", 0.0);
    long long execution_generation = jgetint(attempt, "execution_generation", 0);
    long long cancel_generation = jgetint(attempt, "cancel_generation", 0);
    json_t *out = json_object();
    json_t *err = w29_hostedroom_8927a5_p2_lease_gate(jget(req, "room"), jget(req, "lease_row"), jget(req, "lease"),
                                                      now, 0);

    if (!jnone(err)) {
        json_set(out, "outcome", json_string("error"));
        json_set(out, "error", json_copy(jget(err, "error")));
        json_set(out, "message", json_copy(jget(err, "message")));
        json_free(err);
        return out;
    }
    err = w29_hostedroom_8927a5_p2_load_task(row, identity, 1);
    if (!jnone(err)) {
        json_set(out, "outcome", json_string("error"));
        json_set(out, "error", json_copy(jget(err, "error")));
        json_set(out, "message", json_copy(jget(err, "message")));
        json_free(err);
        return out;
    }
    if (!w29_hostedroom_8927a5_p2_generations_match(row, "running", execution_generation, cancel_generation)) {
        json_set(out, "outcome", json_string("skipped"));
        return out;
    }
    {
        json_t *params = json_array();
        json_append(params, json_number(now));
        json_append(params, json_number(now));
        json_append(params, json_copy(jget(identity, "room_id")));
        json_append(params, json_copy(jget(identity, "task_id")));
        json_append(params, json_from_long(execution_generation));
        json_append(params, json_from_long(cancel_generation));
        json_set(out, "outcome", json_string("marked"));
        json_set(out, "params", params);
        return out;
    }
}

/* PoP: w29_hostedroom_8927a5_p3_settle_stopping @ hosted_room_driver.py:settle_stopping_task */
json_t *w29_hostedroom_8927a5_p3_settle_stopping(const json_t *req) {
    const json_t *row = jget(req, "task_row");
    const json_t *identity = jget(req, "identity");
    const json_t *lease = jget(req, "lease");
    const char *settlement_id = jstr(req, "settlement_id");
    const char *status = jstr(req, "status");
    double now = jgetnum(req, "now", 0.0);
    long long execution_generation = jgetint(req, "expected_execution_generation", 0);
    long long cancel_generation = jgetint(req, "expected_cancel_generation", 0);
    json_t *out = json_object();
    json_t *err;

    err = w29_hostedroom_8927a5_p2_load_task(row, identity, 1);
    if (!jnone(err)) goto error;
    {
        json_t *replay = w29_hostedroom_8927a5_p3_settlement_replay(row, settlement_id, status,
                                                                    jget(req, "result"), now);
        const char *outcome = jstr(replay, "outcome");
        if (outcome && !strcmp(outcome, "idempotent")) {
            json_set(out, "outcome", json_string("replayed"));
            json_set(out, "task", json_copy(jget(replay, "task")));
            json_free(replay);
            return out;
        }
        if (outcome && !strcmp(outcome, "conflict")) {
            json_set(out, "outcome", json_string("error"));
            json_set(out, "error", json_copy(jget(replay, "error")));
            json_set(out, "message", json_copy(jget(replay, "message")));
            json_free(replay);
            return out;
        }
        json_free(replay);
    }
    err = w29_hostedroom_8927a5_p2_lease_gate(jget(req, "room"), jget(req, "lease_row"), lease, now, 0);
    if (!jnone(err)) goto error;
    /* No row guard: settle_stopping_task's fence lives in the statement's
     * status='stopping' term, so only the UPDATE's rowcount can refuse it. */
    if (jgetint(req, "rowcount", 1) != 1) {
        err = err_obj("StaleTaskError", "task completion lost the stop race");
        goto error;
    }
    {
        json_t *params = w29_hostedroom_8927a5_p3_settlement_params(settlement_id, status, jget(req, "result"), now);
        json_append(params, json_copy(jget(identity, "room_id")));
        json_append(params, json_copy(jget(identity, "task_id")));
        json_append(params, json_from_long(execution_generation));
        json_append(params, json_from_long(cancel_generation));
        json_set(out, "outcome", json_string("settled"));
        json_set(out, "params", params);
        return out;
    }
error:
    json_set(out, "outcome", json_string("error"));
    json_set(out, "error", json_copy(jget(err, "error")));
    json_set(out, "message", json_copy(jget(err, "message")));
    json_free(err);
    return out;
}

/* PoP: w29_hostedroom_8927a5_p3_begin_cancel @ hosted_room_driver.py:begin_task_cancel */
json_t *w29_hostedroom_8927a5_p3_begin_cancel(const json_t *req) {
    const json_t *row = jget(req, "task_row");
    const json_t *identity = jget(req, "identity");
    const char *cancel_id = jstr(req, "cancel_id");
    long long expected = jgetint(req, "expected_cancel_generation", 0);
    double now = jgetnum(req, "now", 0.0);
    json_t *out = json_object();
    json_t *err;
    const char *status;

    err = w29_hostedroom_8927a5_p2_load_task(row, identity, 1);
    if (!jnone(err)) goto error;
    {
        json_t *replay = w29_hostedroom_8927a5_p3_cancel_replay(row, cancel_id, "stopping");
        const char *outcome = jstr(replay, "outcome");
        if (outcome && !strcmp(outcome, "idempotent")) {
            json_set(out, "outcome", json_string("replayed"));
            json_set(out, "task", json_copy(jget(replay, "task")));
            json_free(replay);
            return out;
        }
        json_free(replay);
    }
    status = jstr(row, "status");
    if (status && (!strcmp(status, "settled") || !strcmp(status, "failed") || !strcmp(status, "cancelled") ||
                   !strcmp(status, "queued"))) {
        sb b;
        char *msg;
        sb_init(&b);
        sb_add(&b, "cannot request remote stop in state '");
        sb_add(&b, status);
        sb_add(&b, "'");
        msg = sb_take(&b);
        err = err_obj("InvalidTaskTransitionError", msg);
        free(msg);
        goto error;
    }
    err = w29_hostedroom_8927a5_p2_cancel_generation_guard(row, expected);
    if (!jnone(err)) goto error;
    if (jgetint(req, "rowcount", 1) != 1) {
        err = err_obj("StaleTaskError", "task changed during stop request");
        goto error;
    }
    {
        json_t *params = json_array();
        json_append(params, json_from_long(expected + 1));
        json_append(params, json_string(cancel_id ? cancel_id : ""));
        json_append(params, json_number(now));
        json_append(params, json_copy(jget(identity, "room_id")));
        json_append(params, json_copy(jget(identity, "task_id")));
        json_append(params, json_from_long(expected));
        json_set(out, "outcome", json_string("stopping"));
        json_set(out, "params", params);
        return out;
    }
error:
    json_set(out, "outcome", json_string("error"));
    json_set(out, "error", json_copy(jget(err, "error")));
    json_set(out, "message", json_copy(jget(err, "message")));
    json_free(err);
    return out;
}

/* PoP: w29_hostedroom_8927a5_p3_complete_cancel @ hosted_room_driver.py:complete_task_cancel */
json_t *w29_hostedroom_8927a5_p3_complete_cancel(const json_t *req) {
    const json_t *row = jget(req, "task_row");
    const json_t *identity = jget(req, "identity");
    const char *cancel_id = jstr(req, "cancel_id");
    long long expected = jgetint(req, "expected_cancel_generation", 0);
    double now = jgetnum(req, "now", 0.0);
    json_t *out = json_object();
    json_t *err;
    const char *status;
    const char *row_cancel;

    err = w29_hostedroom_8927a5_p2_load_task(row, identity, 1);
    if (!jnone(err)) goto error;
    {
        json_t *replay = w29_hostedroom_8927a5_p3_cancel_replay(row, cancel_id, "cancelled");
        const char *outcome = jstr(replay, "outcome");
        if (outcome && !strcmp(outcome, "idempotent")) {
            json_set(out, "outcome", json_string("replayed"));
            json_set(out, "task", json_copy(jget(replay, "task")));
            json_free(replay);
            return out;
        }
        json_free(replay);
    }
    status = jstr(row, "status");
    row_cancel = jstr(row, "cancel_id");
    if (!status || strcmp(status, "stopping") != 0 || !row_cancel || !cancel_id || strcmp(row_cancel, cancel_id) != 0 ||
        jgetint(row, "cancel_generation", 0) != expected) {
        err = err_obj("StaleTaskError", "task stop acknowledgement is stale");
        goto error;
    }
    if (jgetint(req, "rowcount", 1) != 1) {
        err = err_obj("StaleTaskError", "task changed during stop acknowledgement");
        goto error;
    }
    {
        json_t *params = json_array();
        json_append(params, json_number(now));
        json_append(params, json_number(now));
        json_append(params, json_copy(jget(identity, "room_id")));
        json_append(params, json_copy(jget(identity, "task_id")));
        json_append(params, json_string(cancel_id ? cancel_id : ""));
        json_append(params, json_from_long(expected));
        json_set(out, "outcome", json_string("cancelled"));
        json_set(out, "params", params);
        return out;
    }
error:
    json_set(out, "outcome", json_string("error"));
    json_set(out, "error", json_copy(jget(err, "error")));
    json_set(out, "message", json_copy(jget(err, "message")));
    json_free(err);
    return out;
}

/* PoP: w29_hostedroom_8927a5_p3_requeue_not_admitted @ hosted_room_driver.py:requeue_not_admitted_task */
json_t *w29_hostedroom_8927a5_p3_requeue_not_admitted(const json_t *req) {
    const json_t *row = jget(req, "task_row");
    const json_t *attempt = jget(req, "attempt");
    const json_t *identity = jget(attempt, "identity");
    const json_t *lease = jget(req, "lease");
    double now = jgetnum(req, "now", 0.0);
    long long execution_generation = jgetint(attempt, "execution_generation", 0);
    long long cancel_generation = jgetint(attempt, "cancel_generation", 0);
    json_t *out = json_object();
    json_t *err;

    err = w29_hostedroom_8927a5_p2_lease_gate(jget(req, "room"), jget(req, "lease_row"), lease, now, 0);
    if (!jnone(err)) goto error;
    err = w29_hostedroom_8927a5_p2_load_task(row, identity, 1);
    if (!jnone(err)) goto error;
    {
        const char *status = jstr(row, "status");
        int requeued = status && !strcmp(status, "queued") &&
                       jgetint(row, "execution_generation", 0) == execution_generation &&
                       jgetint(row, "cancel_generation", 0) == cancel_generation &&
                       jnone(jget(row, "run_gateway_id")) && jnone(jget(row, "run_process_generation")) &&
                       jnone(jget(row, "run_lease_generation"));
        if (requeued) {
            json_set(out, "outcome", json_string("replayed"));
            json_set(out, "task", w29_hostedroom_8927a5_p3_task_view(row, 1));
            return out;
        }
    }
    {
        const char *run_gateway = jstr(row, "run_gateway_id");
        const char *run_process = jstr(row, "run_process_generation");
        const char *lease_gateway = jstr(lease, "gateway_id");
        const char *lease_process = jstr(lease, "process_generation");
        int fence_ok = run_gateway && lease_gateway && !strcmp(run_gateway, lease_gateway) && run_process &&
                       lease_process && !strcmp(run_process, lease_process) &&
                       (long long)jgetnum(row, "run_lease_generation", 0.0) == jgetint(lease, "lease_generation", 0);
        if (!w29_hostedroom_8927a5_p2_generations_match(row, "running", execution_generation, cancel_generation) ||
            !fence_ok) {
            err = err_obj("StaleTaskError", "not-admitted task attempt lost its fence");
            goto error;
        }
    }
    if (jgetint(req, "rowcount", 1) != 1) {
        err = err_obj("StaleTaskError", "not-admitted task changed during requeue");
        goto error;
    }
    {
        json_t *params = json_array();
        json_append(params, json_number(now));
        json_append(params, json_copy(jget(identity, "room_id")));
        json_append(params, json_copy(jget(identity, "task_id")));
        json_append(params, json_from_long(execution_generation));
        json_append(params, json_from_long(cancel_generation));
        json_append(params, json_copy(jget(lease, "gateway_id")));
        json_append(params, json_copy(jget(lease, "process_generation")));
        json_append(params, json_from_long(jgetint(lease, "lease_generation", 0)));
        json_set(out, "outcome", json_string("requeued"));
        json_set(out, "params", params);
        return out;
    }
error:
    json_set(out, "outcome", json_string("error"));
    json_set(out, "error", json_copy(jget(err, "error")));
    json_set(out, "message", json_copy(jget(err, "message")));
    json_free(err);
    return out;
}

/* PoP: w29_hostedroom_8927a5_p2_recover @ hosted_room_driver.py:recover_room */
json_t *w29_hostedroom_8927a5_p2_recover(const json_t *req) {
    const json_t *lease = jget(req, "lease");
    const json_t *stale = jget(req, "stale_rows");
    const json_t *queued = jget(req, "queued_rows");
    const json_t *indeterminate = jget(req, "indeterminate_rows");
    double now = jgetnum(req, "now", 0.0);
    json_t *out = json_object();
    json_t *err = w29_hostedroom_8927a5_p2_lease_gate(jget(req, "room"), jget(req, "lease_row"), lease, now, 0);
    json_t *ids, *params;
    size_t i;

    if (!jnone(err)) {
        json_set(out, "outcome", json_string("error"));
        json_set(out, "error", json_copy(jget(err, "error")));
        json_set(out, "message", json_copy(jget(err, "message")));
        json_free(err);
        return out;
    }
    ids = json_array();
    for (i = 0; stale && i < json_len(stale); i++) {
        const json_t *r = json_get(stale, i);
        json_append(ids, json_copy(jget(r, "task_id")));
    }
    if (json_len(ids) > 0) {
        params = json_array();
        json_append(params, json_number(now));
        json_append(params, json_number(now));
        json_append(params, json_copy(jget(lease, "room_id")));
        json_append(params, json_copy(jget(lease, "gateway_id")));
        json_append(params, json_copy(jget(lease, "process_generation")));
        json_append(params, json_from_long(jgetint(lease, "lease_generation", 0)));
        json_set(out, "update", json_bool(1));
        json_set(out, "params", params);
    } else {
        json_set(out, "update", json_bool(0));
        json_set(out, "params", json_null());
    }
    json_set(out, "stale_ids", ids);
    {
        json_t *q = json_array();
        json_t *ind = json_array();
        for (i = 0; queued && i < json_len(queued); i++)
            json_append(q, w29_hostedroom_8927a5_p2_task_identity(json_get(queued, i)));
        for (i = 0; indeterminate && i < json_len(indeterminate); i++)
            json_append(ind, w29_hostedroom_8927a5_p2_task_identity(json_get(indeterminate, i)));
        json_set(out, "queued", q);
        json_set(out, "indeterminate", ind);
    }
    json_set(out, "outcome", json_string("recovered"));
    return out;
}
