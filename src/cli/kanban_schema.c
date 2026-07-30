/*
 * kanban_schema.c — connection + schema engine for hermes_cli/kanban_db.py
 *
 * Concern: open/init the kanban SQLite DB, run the schema + additive
 * migrations, and provide the write-transaction primitives. Reuses
 * sqlite_util (add_column_if_missing, write_txn) and the path/config
 * helpers already ported in src/cli/port_kanban_db.c (kanban_db_path,
 * get_current_board, board_exists, kanban_board_exists, normalize_board_slug).
 *
 * Minimal includes: only what this TU needs. No god header.
 *
 * PoP: exact port. Semantic source of truth = hermes_cli/kanban_db.py
 *      (connect / init_db / _migrate_add_optional_columns / write_txn).
 * PoP: kdb_connect @ hermes_cli/kanban_db.py:connect
 * PoP: kdb_connect_path @ hermes_cli/kanban_db.py:connect_closing
 * PoP: kdb_init_db @ hermes_cli/kanban_db.py:init_db
 * PoP: kdb_write_begin @ hermes_cli/kanban_db.py:write_txn
 * PoP: kdb_write_end @ hermes_cli/kanban_db.py:write_txn
 * PoP: kdb_now @ hermes_cli/kanban_db.py:_resolve_busy_timeout_ms
 */

#include "kanban_db.h"
#include "hermes_json.h"
#include "hermes_cli/sqlite_util.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

/* ---- declarations of helpers owned by port_kanban_db.c (already ported) ---- */
char *kanban_db_path(const char *board);
char *get_current_board(void);
int   board_exists(const char *board);
char *normalize_board_slug(const char *slug);
extern int resolve_busy_timeout_ms(void);   /* defined in port_kanban_db.c */

/* The full v1 schema (mirrors SCHEMA_SQL in the Python module). */
static const char *KANBAN_SCHEMA_SQL =
    "CREATE TABLE IF NOT EXISTS tasks (\n"
    "    id                   TEXT PRIMARY KEY,\n"
    "    title                TEXT NOT NULL,\n"
    "    body                 TEXT,\n"
    "    assignee             TEXT,\n"
    "    status               TEXT NOT NULL,\n"
    "    priority             INTEGER DEFAULT 0,\n"
    "    created_by           TEXT,\n"
    "    created_at           INTEGER NOT NULL,\n"
    "    started_at           INTEGER,\n"
    "    completed_at         INTEGER,\n"
    "    workspace_kind       TEXT NOT NULL DEFAULT 'scratch',\n"
    "    workspace_path       TEXT,\n"
    "    branch_name          TEXT,\n"
    "    project_id           TEXT,\n"
    "    claim_lock           TEXT,\n"
    "    claim_expires        INTEGER,\n"
    "    tenant               TEXT,\n"
    "    result               TEXT,\n"
    "    idempotency_key      TEXT,\n"
    "    consecutive_failures INTEGER NOT NULL DEFAULT 0,\n"
    "    worker_pid           INTEGER,\n"
    "    last_failure_error   TEXT,\n"
    "    max_runtime_seconds  INTEGER,\n"
    "    last_heartbeat_at    INTEGER,\n"
    "    current_run_id       INTEGER,\n"
    "    workflow_template_id TEXT,\n"
    "    current_step_key     TEXT,\n"
    "    skills               TEXT,\n"
    "    model_override       TEXT,\n"
    "    max_retries          INTEGER,\n"
    "    goal_mode            INTEGER NOT NULL DEFAULT 0,\n"
    "    goal_max_turns       INTEGER,\n"
    "    session_id           TEXT,\n"
    "    block_kind           TEXT,\n"
    "    block_recurrences    INTEGER NOT NULL DEFAULT 0\n"
    ");\n"
    "CREATE TABLE IF NOT EXISTS task_links (\n"
    "    parent_id  TEXT NOT NULL,\n"
    "    child_id   TEXT NOT NULL,\n"
    "    PRIMARY KEY (parent_id, child_id)\n"
    ");\n"
    "CREATE TABLE IF NOT EXISTS task_comments (\n"
    "    id         INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    task_id    TEXT NOT NULL,\n"
    "    author     TEXT NOT NULL,\n"
    "    body       TEXT NOT NULL,\n"
    "    created_at INTEGER NOT NULL\n"
    ");\n"
    "CREATE TABLE IF NOT EXISTS task_events (\n"
    "    id         INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    task_id    TEXT NOT NULL,\n"
    "    run_id     INTEGER,\n"
    "    kind       TEXT NOT NULL,\n"
    "    payload    TEXT,\n"
    "    created_at INTEGER NOT NULL\n"
    ");\n"
    "CREATE TABLE IF NOT EXISTS task_runs (\n"
    "    id                  INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    task_id             TEXT NOT NULL,\n"
    "    profile             TEXT,\n"
    "    step_key            TEXT,\n"
    "    status              TEXT NOT NULL,\n"
    "    claim_lock          TEXT,\n"
    "    claim_expires       INTEGER,\n"
    "    worker_pid          INTEGER,\n"
    "    max_runtime_seconds INTEGER,\n"
    "    last_heartbeat_at   INTEGER,\n"
    "    started_at          INTEGER NOT NULL,\n"
    "    ended_at            INTEGER,\n"
    "    outcome             TEXT,\n"
    "    summary             TEXT,\n"
    "    metadata            TEXT,\n"
    "    error               TEXT\n"
    ");\n"
    "CREATE TABLE IF NOT EXISTS task_attachments (\n"
    "    id           INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    task_id      TEXT NOT NULL,\n"
    "    filename     TEXT NOT NULL,\n"
    "    stored_path  TEXT NOT NULL,\n"
    "    content_type TEXT,\n"
    "    size         INTEGER NOT NULL DEFAULT 0,\n"
    "    uploaded_by  TEXT,\n"
    "    created_at   INTEGER NOT NULL\n"
    ");\n"
    "CREATE TABLE IF NOT EXISTS kanban_notify_subs (\n"
    "    task_id         TEXT NOT NULL,\n"
    "    platform        TEXT NOT NULL,\n"
    "    chat_id         TEXT NOT NULL,\n"
    "    thread_id       TEXT NOT NULL DEFAULT '',\n"
    "    user_id         TEXT,\n"
    "    notifier_profile TEXT,\n"
    "    created_at      INTEGER NOT NULL,\n"
    "    last_event_id   INTEGER NOT NULL DEFAULT 0,\n"
    "    PRIMARY KEY (task_id, platform, chat_id, thread_id)\n"
    ");\n"
    "CREATE INDEX IF NOT EXISTS idx_tasks_assignee_status ON tasks(assignee, status);\n"
    "CREATE INDEX IF NOT EXISTS idx_tasks_status          ON tasks(status);\n"
    "CREATE INDEX IF NOT EXISTS idx_links_child           ON task_links(child_id);\n"
    "CREATE INDEX IF NOT EXISTS idx_links_parent          ON task_links(parent_id);\n"
    "CREATE INDEX IF NOT EXISTS idx_comments_task         ON task_comments(task_id, created_at);\n"
    "CREATE INDEX IF NOT EXISTS idx_events_task           ON task_events(task_id, created_at);\n"
    "CREATE INDEX IF NOT EXISTS idx_runs_task             ON task_runs(task_id, started_at);\n"
    "CREATE INDEX IF NOT EXISTS idx_runs_status           ON task_runs(status);\n"
    "CREATE INDEX IF NOT EXISTS idx_attachments_task      ON task_attachments(task_id, created_at);\n"
    "CREATE INDEX IF NOT EXISTS idx_notify_task           ON kanban_notify_subs(task_id);\n";

/* Additive migrations for columns introduced after v1. */
static int kanban_migrate_optional_columns(sqlite3 *conn)
{
    /* Collect current column names. */
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, "PRAGMA table_info(tasks)", -1, &st, NULL) != SQLITE_OK)
        return -1;
    int have_tenant = 0, have_result = 0, have_branch = 0, have_project = 0,
        have_idem = 0, have_confail = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *name = (const char *)sqlite3_column_text(st, 1);
        if (!name) continue;
        if (!strcmp(name, "tenant"))            have_tenant = 1;
        else if (!strcmp(name, "result"))       have_result = 1;
        else if (!strcmp(name, "branch_name"))  have_branch = 1;
        else if (!strcmp(name, "project_id"))   have_project = 1;
        else if (!strcmp(name, "idempotency_key")) have_idem = 1;
        else if (!strcmp(name, "consecutive_failures")) have_confail = 1;
    }
    sqlite3_finalize(st);

    if (!have_tenant)
        sqlite_util_add_column_if_missing(conn, "tasks", "tenant", "tenant TEXT");
    if (!have_result)
        sqlite_util_add_column_if_missing(conn, "tasks", "result", "result TEXT");
    if (!have_branch)
        sqlite_util_add_column_if_missing(conn, "tasks", "branch_name", "branch_name TEXT");
    if (!have_project)
        sqlite_util_add_column_if_missing(conn, "tasks", "project_id", "project_id TEXT");
    if (!have_idem)
        sqlite_util_add_column_if_missing(conn, "tasks", "idempotency_key", "idempotency_key TEXT");
    if (!have_confail)
        sqlite_util_add_column_if_missing(conn, "tasks", "consecutive_failures",
                                          "consecutive_failures INTEGER NOT NULL DEFAULT 0");

    /* Index for idempotency lookups (additive, safe to repeat). */
    sqlite3_exec(conn,
        "CREATE INDEX IF NOT EXISTS idx_tasks_idempotency ON tasks(idempotency_key)",
        NULL, NULL, NULL);
    return 0;
}

/* Open + lazily init. Faithful to connect(): resolve path, mkdir, run schema
 * + migrations under a write txn. Path resolution precedence:
 *   board arg -> kanban_db_path(board) (which honours HERMES_KANBAN_DB env,
 *   current-board file, default). */
/* PoP: kdb_connect @ hermes_cli/kanban_db.py:connect */
sqlite3 *kdb_connect(const char *board)
{
    char *path = kanban_db_path(board);
    if (!path) return NULL;
    sqlite3 *conn = NULL;
    int rc = sqlite3_open_v2(path, &conn,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    free(path);
    if (rc != SQLITE_OK) {
        if (conn) sqlite3_close(conn);
        return NULL;
    }

    char *dir = NULL;
    /* Ensure parent dir exists (mirrors path.parent.mkdir). */
    {
        char *p2 = kanban_db_path(board);
        const char *slash = strrchr(p2 ? p2 : "", '/');
        if (slash) {
            size_t n = (size_t)(slash - p2);
            dir = malloc(n + 1);
            if (dir) { memcpy(dir, p2, n); dir[n] = '\0'; }
        }
        free(p2);
    }
    if (dir) { mkdir(dir, 0755); free(dir); }

    /* busy_timeout (mirrors _sqlite_connect + _resolve_busy_timeout_ms). */
    int bt = resolve_busy_timeout_ms();
    char pragma[64];
    snprintf(pragma, sizeof(pragma), "PRAGMA busy_timeout=%d", bt);
    sqlite3_exec(conn, pragma, NULL, NULL, NULL);

    if (kdb_init_db(conn) != 0) {
        sqlite3_close(conn);
        return NULL;
    }
    return conn;
}

/* PoP: kdb_connect_path @ hermes_cli/kanban_db.py:connect_closing */
sqlite3 *kdb_connect_path(const char *db_path)
{
    if (!db_path) return NULL;
    sqlite3 *conn = NULL;
    int rc = sqlite3_open_v2(db_path, &conn,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK) {
        if (conn) sqlite3_close(conn);
        return NULL;
    }
    int bt = resolve_busy_timeout_ms();
    char pragma[64];
    snprintf(pragma, sizeof(pragma), "PRAGMA busy_timeout=%d", bt);
    sqlite3_exec(conn, pragma, NULL, NULL, NULL);
    if (kdb_init_db(conn) != 0) {
        sqlite3_close(conn);
        return NULL;
    }
    return conn;
}

void kdb_close(sqlite3 *conn)
{
    if (conn) sqlite3_close(conn);
}

/* PoP: kdb_now @ hermes_cli/kanban_db.py:_resolve_busy_timeout_ms */
long kdb_now(void)
{
    static int frozen = -1;
    static long frozen_val = 0;
    if (frozen < 0) {
        const char *e = getenv("HERMES_KANBAN_NOW");
        if (e && *e) {
            char *end = NULL;
            long v = strtol(e, &end, 10);
            if (end != e && *end == '\0') { frozen = 1; frozen_val = v; return v; }
        }
        frozen = 0;
    }
    if (frozen) return frozen_val;
    return (long)time(NULL);
}

/* PoP: kdb_init_db @ hermes_cli/kanban_db.py:init_db */
int kdb_init_db(sqlite3 *conn)
{
    if (!conn) return -1;
    int rc = sqlite3_exec(conn, KANBAN_SCHEMA_SQL, NULL, NULL, NULL);
    if (rc != SQLITE_OK) return -1;
    if (kanban_migrate_optional_columns(conn) != 0) return -1;
    return 0;
}

/* PoP: kdb_write_begin @ hermes_cli/kanban_db.py:write_txn */
int kdb_write_begin(sqlite3 *conn)
{
    return sqlite_util_write_txn_begin(conn);
}

/* PoP: kdb_write_end @ hermes_cli/kanban_db.py:write_txn */
int kdb_write_end(sqlite3 *conn, int committed)
{
    return sqlite_util_write_txn_end(conn, committed);
}
