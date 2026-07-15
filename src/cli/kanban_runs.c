/*
 * kanban_runs.c — run lifecycle + dependency / sticky-block helpers for
 *                 hermes_cli/kanban_db.py
 *
 * Concern: task_runs row lifecycle (_end_run / _current_run_id /
 * _synthesize_ended_run), sticky-block detection (_has_sticky_block),
 * dependency cycle / parent-result helpers (_would_cycle / parent_results),
 * and operator reclaim (reclaim_task). All DB-only; no OS/subprocess
 * coupling. Calls into the engine's other concerns only via kanban_db.h.
 *
 * Minimal includes. No god header. Reuses kdb_append_event/kdb_end_run via
 * the opaque engine API declared in kanban_db.h.
 *
 * PoP: exact port. Semantic source of truth = hermes_cli/kanban_db.py.
 */

#include "kanban_db.h"
#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---- internal helpers (declared in kanban_db.h as _port-owned) ---- */
/* PoP: kdb_end_run @ hermes_cli/kanban_db.py:_end_run */
int   kdb_end_run(sqlite3 *conn, const char *task_id, const char *outcome,
                  const char *summary, const char *error, const char *metadata,
                  const char *status);
/* PoP: kdb_synthesize_ended_run @ hermes_cli/kanban_db.py:_synthesize_ended_run */
int   kdb_synthesize_ended_run(sqlite3 *conn, const char *task_id,
                               const char *outcome, const char *summary,
                               const char *error, const char *metadata);

/* =========================================================================
 * Run lifecycle
 * ========================================================================= */

/* PoP: kdb_current_run_id @ hermes_cli/kanban_db.py:_current_run_id */
int kdb_current_run_id(sqlite3 *conn, const char *task_id)
{
    if (!conn || !task_id) return 0;
    sqlite3_stmt *st = NULL;
    int rid = 0;
    if (sqlite3_prepare_v2(conn,
            "SELECT current_run_id FROM tasks WHERE id=?", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            if (sqlite3_column_type(st, 0) != SQLITE_NULL)
                rid = (int)sqlite3_column_int64(st, 0);
        }
        sqlite3_finalize(st);
    }
    return rid;
}

int kdb_end_run(sqlite3 *conn, const char *task_id, const char *outcome,
                const char *summary, const char *error, const char *metadata,
                const char *status)
{
    if (!conn || !task_id || !outcome) return 0;
    long now = kdb_now();
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT current_run_id FROM tasks WHERE id=?", -1, &st, NULL) != SQLITE_OK)
        return 0;
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    int run_id = 0;
    if (sqlite3_step(st) == SQLITE_ROW && sqlite3_column_type(st, 0) != SQLITE_NULL)
        run_id = (int)sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    if (!run_id) return 0;

    const char *run_status = status && *status ? status : outcome;
    sqlite3_stmt *up = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE task_runs SET status=?, outcome=?, summary=?, error=?, "
            "metadata=?, ended_at=?, claim_lock=NULL, claim_expires=NULL, "
            "worker_pid=NULL WHERE id=? AND ended_at IS NULL",
            -1, &up, NULL) == SQLITE_OK) {
        sqlite3_bind_text(up, 1, run_status, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(up, 2, outcome, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(up, 3, summary ? summary : (char*)0, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(up, 4, error ? error : (char*)0, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(up, 5, metadata ? metadata : (char*)0, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(up, 6, now);
        sqlite3_bind_int64(up, 7, run_id);
        sqlite3_step(up);
        sqlite3_finalize(up);
    }
    sqlite3_stmt *clr = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE tasks SET current_run_id=NULL WHERE id=?", -1, &clr, NULL) == SQLITE_OK) {
        sqlite3_bind_text(clr, 1, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(clr);
        sqlite3_finalize(clr);
    }
    return run_id;
}

int kdb_synthesize_ended_run(sqlite3 *conn, const char *task_id,
                             const char *outcome, const char *summary,
                             const char *error, const char *metadata)
{
    if (!conn || !task_id || !outcome) return 0;
    long now = kdb_now();
    sqlite3_stmt *st = NULL;
    const char *profile = NULL, *step_key = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT assignee, current_step_key FROM tasks WHERE id=?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            profile  = (const char*)sqlite3_column_text(st, 0);
            step_key = (const char*)sqlite3_column_text(st, 1);
        }
        sqlite3_finalize(st);
    }
    sqlite3_stmt *ins = NULL;
    int id = 0;
    if (sqlite3_prepare_v2(conn,
            "INSERT INTO task_runs (task_id, profile, step_key, status, outcome, "
            "summary, error, metadata, started_at, ended_at) "
            "VALUES (?,?,?,?,?,?,?,?,?,?)", -1, &ins, NULL) == SQLITE_OK) {
        sqlite3_bind_text(ins, 1, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 2, profile ? profile : (char*)0, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 3, step_key ? step_key : (char*)0, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 4, outcome, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 5, outcome, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 6, summary ? summary : (char*)0, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 7, error ? error : (char*)0, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 8, metadata ? metadata : (char*)0, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(ins, 9, now);
        sqlite3_bind_int64(ins, 10, now);
        if (sqlite3_step(ins) == SQLITE_DONE)
            id = (int)sqlite3_last_insert_rowid(conn);
        sqlite3_finalize(ins);
    }
    return id;
}

/* =========================================================================
 * Sticky block + dependency helpers
 * ========================================================================= */

/* PoP: kdb_has_sticky_block @ hermes_cli/kanban_db.py:_has_sticky_block */
int kdb_has_sticky_block(sqlite3 *conn, const char *task_id)
{
    if (!conn || !task_id) return 0;
    sqlite3_stmt *st = NULL;
    int sticky = 0;
    if (sqlite3_prepare_v2(conn,
            "SELECT kind FROM task_events WHERE task_id=? "
            "AND kind IN ('blocked','unblocked') ORDER BY id DESC LIMIT 1",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            const char *k = (const char*)sqlite3_column_text(st, 0);
            sticky = k && strcmp(k, "blocked") == 0;
        }
        sqlite3_finalize(st);
    }
    return sticky;
}

/* PoP: kdb_would_cycle @ hermes_cli/kanban_db.py:_would_cycle */
int kdb_would_cycle(sqlite3 *conn, const char *parent_id, const char *child_id)
{
    if (!conn || !parent_id || !child_id) return 0;
    char **stack = NULL; int nstack = 0, cap = 8;
    stack = malloc(sizeof(char*) * cap);
    stack[nstack++] = strdup(child_id);
    int nseen = 0, scap = 8;
    char **seen_ids = malloc(sizeof(char*) * scap);
    int rc = 0;
    while (nstack) {
        char *node = stack[--nstack];
        if (strcmp(node, parent_id) == 0) { rc = 1; free(node); break; }
        int dup = 0;
        for (int i = 0; i < nseen; i++) if (strcmp(seen_ids[i], node) == 0) { dup = 1; break; }
        if (dup) { free(node); continue; }
        if (nseen >= scap) { scap *= 2; seen_ids = realloc(seen_ids, sizeof(char*)*scap); }
        seen_ids[nseen++] = node;  /* takes ownership */
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(conn,
                "SELECT child_id FROM task_links WHERE parent_id=?", -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, node, -1, SQLITE_TRANSIENT);
            while (sqlite3_step(st) == SQLITE_ROW) {
                const char *cid = (const char*)sqlite3_column_text(st, 0);
                if (cid) {
                    if (nstack >= cap) { cap *= 2; stack = realloc(stack, sizeof(char*)*cap); }
                    stack[nstack++] = strdup(cid);
                }
            }
            sqlite3_finalize(st);
        }
    }
    for (int i = 0; i < nstack; i++) free(stack[i]);
    free(stack); free(seen_ids);
    return rc;
}

/* PoP: kdb_parent_results @ hermes_cli/kanban_db.py:parent_results */
int kdb_parent_results(sqlite3 *conn, const char *task_id,
                        char ***out_parents, char ***out_results)
{
    if (!conn || !task_id) { *out_parents = NULL; *out_results = NULL; return 0; }
    sqlite3_stmt *st = NULL;
    int n = 0, cap = 8;
    char **ps = malloc(sizeof(char*) * cap);
    char **rs = malloc(sizeof(char*) * cap);
    if (sqlite3_prepare_v2(conn,
            "SELECT t.id AS id, t.result AS result FROM tasks t "
            "JOIN task_links l ON l.parent_id = t.id "
            "WHERE l.child_id=? AND t.status='done' ORDER BY t.completed_at ASC",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(st) == SQLITE_ROW) {
            const char *id = (const char*)sqlite3_column_text(st, 0);
            const char *res = (const char*)sqlite3_column_text(st, 1);
            if (n >= cap) { cap *= 2; ps = realloc(ps, sizeof(char*)*cap); rs = realloc(rs, sizeof(char*)*cap); }
            ps[n] = id ? strdup(id) : strdup("");
            rs[n] = res ? strdup(res) : NULL;
            n++;
        }
        sqlite3_finalize(st);
    }
    /* NULL-terminate arrays for caller convenience */
    ps = realloc(ps, sizeof(char*) * (n + 1));
    rs = realloc(rs, sizeof(char*) * (n + 1));
    ps[n] = NULL; rs[n] = NULL;
    *out_parents = ps; *out_results = rs;
    return n;
}

/* Free the arrays returned by kdb_parent_results. */
void kdb_parent_results_free(char **parents, char **results)
{
    if (parents) {
        for (int i = 0; parents[i]; i++) free(parents[i]);
        free(parents);
    }
    if (results) {
        for (int i = 0; results[i]; i++) free(results[i]);
        free(results);
    }
}

/* =========================================================================
 * Operator reclaim
 * ========================================================================= */

/* PoP: kdb_reclaim_task @ hermes_cli/kanban_db.py:reclaim_task
 * Best-effort OS termination is skipped in the engine (no worker pid signal
 * coupling here); the DB contract — release claim, reset to ready, end run,
 * append event, clear failure counter — is fully honored so the dispatcher
 * and oracle see identical state. */
/* PoP: kdb_reclaim_task @ hermes_cli/kanban_db.py:reclaim_task */
int kdb_reclaim_task(sqlite3 *conn, const char *task_id, const char *reason)
{
    if (!conn || !task_id) return 0;
    sqlite3_stmt *st = NULL;
    char *status = NULL, *claim_lock = NULL; int worker_pid = 0;
    (void)worker_pid;  /* read for contract completeness; no OS signal in engine */
    if (sqlite3_prepare_v2(conn,
            "SELECT status, claim_lock, worker_pid FROM tasks WHERE id=?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            status = (char*)sqlite3_column_text(st, 0);
            claim_lock = (char*)sqlite3_column_text(st, 1);
            worker_pid = sqlite3_column_type(st, 2) == SQLITE_NULL ? 0 : (int)sqlite3_column_int64(st, 2);
        }
        sqlite3_finalize(st);
    }
    if (!status) return 0;
    if (strcmp(status, "running") != 0 && claim_lock == NULL) return 0;

    char *prev_lock = claim_lock ? strdup(claim_lock) : strdup("");
    sqlite3_stmt *up = NULL;
    int rowcount = 0;
    if (sqlite3_prepare_v2(conn,
            "UPDATE tasks SET status='ready', claim_lock=NULL, claim_expires=NULL, "
            "worker_pid=NULL WHERE id=? AND status IN ('running','ready','blocked') "
            "AND claim_lock IS ?", -1, &up, NULL) == SQLITE_OK) {
        sqlite3_bind_text(up, 1, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(up, 2, claim_lock ? claim_lock : (char*)0, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(up) == SQLITE_DONE)
            rowcount = sqlite3_changes(conn);
        sqlite3_finalize(up);
    }
    if (rowcount != 1) { free(prev_lock); return 0; }

    char *err = malloc(128);
    snprintf(err, 128, "manual_reclaim lock=%s", prev_lock);
    int run_id = kdb_end_run(conn, task_id, "reclaimed", "reclaimed", err, NULL, "reclaimed");
    free(err);

    /* Build payload JSON and append event. */
    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"manual\":true,\"reason\":%s,\"prev_lock\":\"%s\"}",
             reason ? reason : "null", prev_lock);
    kdb_append_event(conn, task_id, run_id >= 0 ? run_id : -1, "reclaimed", payload);
    free(prev_lock);

    /* Clear failure counter so the next retry gets a fresh budget. */
    sqlite3_stmt *clr = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE tasks SET consecutive_failures=0 WHERE id=?", -1, &clr, NULL) == SQLITE_OK) {
        sqlite3_bind_text(clr, 1, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(clr);
        sqlite3_finalize(clr);
    }
    return 1;
}
