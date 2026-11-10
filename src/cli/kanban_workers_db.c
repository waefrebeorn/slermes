/*
 * kanban_workers_db.c — kanban worker DB/bookkeeping helpers.
 *
 * Split from kanban_workers.c (soul.md: self-contained modules). Worker
 * bookkeeping, drift detection, and failure recording against the kanban
 * SQLite DB. No file-scope state.
 */
#include "kanban_workers_db.h"
#include "kanban_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KB_DEFAULT_FAILURE_LIMIT 2
#define KB_RECLAIM_DEFER_GRACE_SECONDS 120


int kdb_record_task_failure(sqlite3 *conn, const char *task_id,
                             const char *error, const char *outcome,
                             int failure_limit, int release_claim,
                             int end_run, const char *event_payload_extra_json)
{
    if (!conn || !task_id) return 0;
    if (failure_limit < 0) failure_limit = KB_DEFAULT_FAILURE_LIMIT;
    int blocked = 0;
    char errbuf[512];
    snprintf(errbuf, sizeof(errbuf), "%s", error ? error : "");
    errbuf[sizeof(errbuf) - 1] = '\0';

    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT consecutive_failures, status, max_retries "
            "FROM tasks WHERE id = ?",
            -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) != SQLITE_ROW) { sqlite3_finalize(st); return 0; }
    int failures0     = sqlite3_column_int(st, 0);
    const char *status = (const char *)sqlite3_column_text(st, 1);
    const char *maxret = (const char *)sqlite3_column_text(st, 2);
    sqlite3_finalize(st);

    int failures = failures0 + 1;
    int effective_limit = failure_limit;
    const char *limit_source = "dispatcher";
    if (maxret && maxret[0]) {
        char *end = NULL;
        long mr = strtol(maxret, &end, 10);
        if (end != maxret && mr >= 0) { effective_limit = (int)mr; limit_source = "task"; }
    }

    if (failures >= effective_limit) {
        /* trip the breaker */
        char q[512];
        if (release_claim) {
            snprintf(q, sizeof(q),
                "UPDATE tasks SET status = 'blocked', claim_lock = NULL, "
                "claim_expires = NULL, worker_pid = NULL, "
                "consecutive_failures = ?, last_failure_error = ? "
                "WHERE id = ? AND status IN ('running','ready')");
        } else {
            snprintf(q, sizeof(q),
                "UPDATE tasks SET status = 'blocked', "
                "consecutive_failures = ?, last_failure_error = ? "
                "WHERE id = ? AND status IN ('ready','running')");
        }
        sqlite3_stmt *up = NULL;
        if (sqlite3_prepare_v2(conn, q, -1, &up, NULL) == SQLITE_OK) {
            sqlite3_bind_int(up, 1, failures);
            sqlite3_bind_text(up, 2, errbuf, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(up, 3, task_id, -1, SQLITE_TRANSIENT);
            sqlite3_step(up);
            sqlite3_finalize(up);
        }
        long run_id = -1;
        if (end_run) {
            char meta[512];
            snprintf(meta, sizeof(meta),
                     "{\"failures\":%d,\"trigger_outcome\":\"%s\","
                     "\"effective_limit\":%d,\"limit_source\":\"%s\"}",
                     failures, outcome ? outcome : "", effective_limit, limit_source);
            run_id = kdb_end_run(conn, task_id, "gave_up", "", errbuf, meta, "gave_up");
        }
        /* gab_up event */
        char pl[1024];
        snprintf(pl, sizeof(pl),
                 "{\"failures\":%d,\"effective_limit\":%d,\"limit_source\":\"%s\","
                 "\"error\":\"%s\",\"trigger_outcome\":\"%s\"",
                 failures, effective_limit, limit_source, errbuf,
                 outcome ? outcome : "");
        if (event_payload_extra_json && event_payload_extra_json[0]) {
            size_t L = strlen(pl);
            snprintf(pl + L, sizeof(pl) - L, ",%s", event_payload_extra_json);
        }
        size_t L = strlen(pl);
        snprintf(pl + L, sizeof(pl) - L, "}");
        kdb_append_event(conn, task_id, run_id, "gave_up", pl);
        blocked = 1;
    } else {
        char q[512];
        if (release_claim) {
            snprintf(q, sizeof(q),
                "UPDATE tasks SET status = 'ready', claim_lock = NULL, "
                "claim_expires = NULL, worker_pid = NULL, "
                "consecutive_failures = ?, last_failure_error = ? "
                "WHERE id = ? AND status = 'running'");
        } else {
            snprintf(q, sizeof(q),
                "UPDATE tasks SET consecutive_failures = ?, "
                "last_failure_error = ? WHERE id = ?");
        }
        sqlite3_stmt *up = NULL;
        if (sqlite3_prepare_v2(conn, q, -1, &up, NULL) == SQLITE_OK) {
            sqlite3_bind_int(up, 1, failures);
            sqlite3_bind_text(up, 2, errbuf, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(up, 3, task_id, -1, SQLITE_TRANSIENT);
            sqlite3_step(up);
            sqlite3_finalize(up);
        }
        if (end_run) {
            char meta[256];
            snprintf(meta, sizeof(meta), "{\"failures\":%d}", failures);
            long run_id = kdb_end_run(conn, task_id, outcome, "", errbuf, meta, outcome);
            kdb_append_event(conn, task_id, run_id, outcome ? outcome : "",
                             meta);
        }
    }
    return blocked;
}

int kb_is_managed_scratch(const char *path)
{
    /* reuse kanban_util.c's logical path guard */
    return kdb_is_managed_scratch_path(path) != 0;
}

int kdb_try_cleanup_parent_workspaces(sqlite3 *conn, const char *task_id)
{
    if (!conn || !task_id) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT parent_id FROM task_links WHERE child_id = ?",
            -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    char parents[256][64];
    int np = 0;
    while (sqlite3_step(st) == SQLITE_ROW && np < 256) {
        const char *p = (const char *)sqlite3_column_text(st, 0);
        if (p) { snprintf(parents[np], 64, "%s", p); np++; }
    }
    sqlite3_finalize(st);
    for (int i = 0; i < np; i++) {
        sqlite3_stmt *r = NULL;
        if (sqlite3_prepare_v2(conn,
                "SELECT workspace_kind, workspace_path FROM tasks WHERE id = ?",
                -1, &r, NULL) != SQLITE_OK) continue;
        sqlite3_bind_text(r, 1, parents[i], -1, SQLITE_TRANSIENT);
        if (sqlite3_step(r) != SQLITE_ROW) { sqlite3_finalize(r); continue; }
        const char *pk = (const char *)sqlite3_column_text(r, 0);
        const char *pp = (const char *)sqlite3_column_text(r, 1);
        char pkb[64], ppb[PATH_MAX];
        snprintf(pkb, sizeof(pkb), "%s", pk ? pk : "");
        snprintf(ppb, sizeof(ppb), "%s", pp ? pp : "");
        sqlite3_finalize(r);
        if (strcmp(pkb, "scratch") != 0 || !ppb[0]) continue;
        sqlite3_stmt *a = NULL;
        if (sqlite3_prepare_v2(conn,
                "SELECT 1 FROM task_links l JOIN tasks t ON t.id = l.child_id "
                "WHERE l.parent_id = ? AND t.status NOT IN "
                "('done','archived','failed','cancelled') LIMIT 1",
                -1, &a, NULL) == SQLITE_OK) {
            sqlite3_bind_text(a, 1, parents[i], -1, SQLITE_TRANSIENT);
            int active = (sqlite3_step(a) == SQLITE_ROW);
            sqlite3_finalize(a);
            if (active) continue;
        }
        if (kb_is_managed_scratch(ppb)) {
            char rm[PATH_MAX + 32];
            snprintf(rm, sizeof(rm), "rm -rf '%s' 2>/dev/null", ppb);
            system(rm);
        }
    }
    return 1;
}

int kb_table_has_drifted(sqlite3 *conn, const char *table)
{
    sqlite3_stmt *st = NULL;
    char pbuf[256];
    snprintf(pbuf, sizeof(pbuf), "PRAGMA table_info(%s)", table);
    if (sqlite3_prepare_v2(conn, pbuf, -1, &st, NULL) != SQLITE_OK)
        return 0;
    int has_id = 0, id_is_int = 0, id_is_pk = 0, has_lei = 0, lei_is_int = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *name = (const char *)sqlite3_column_text(st, 1);
        const char *type = (const char *)sqlite3_column_text(st, 2);
        int pk = sqlite3_column_int(st, 5);
        if (name && strcmp(name, "id") == 0) {
            has_id = 1;
            if (type && strcasecmp(type, "INTEGER") == 0) id_is_int = 1;
            if (pk) id_is_pk = 1;
        }
        if (name && strcmp(name, "last_event_id") == 0) {
            has_lei = 1;
            if (type && strcasecmp(type, "INTEGER") == 0) lei_is_int = 1;
        }
    }
    sqlite3_finalize(st);
    if (strcmp(table, "kanban_notify_subs") == 0)
        return (has_lei && !lei_is_int);
    if (!has_id) return 0;
    return !(id_is_int && id_is_pk);
}

int kb_worker_survived_termination(const char *term_json)
{
    if (!term_json) return 0;
    /* parse the three booleans we care about (tiny, bounded JSON) */
    int attempted = strstr(term_json, "\"termination_attempted\":true") != NULL;
    int host_local = strstr(term_json, "\"host_local\":true") != NULL;
    int terminated = strstr(term_json, "\"terminated\":true") != NULL;
    return attempted && host_local && !terminated;
}

void kb_defer_reclaim_for_live_worker(sqlite3 *conn, const char *task_id,
                                              const char *claim_lock, long now,
                                              const char *term_json, const char *reason)
{
    long grace = now + KB_RECLAIM_DEFER_GRACE_SECONDS;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE tasks SET claim_expires = ? "
            "WHERE id = ? AND status = 'running' AND claim_lock IS ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(st, 1, grace);
        sqlite3_bind_text(st, 2, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, claim_lock ? claim_lock : (char*)0, -1, SQLITE_TRANSIENT);
        int rc = sqlite3_step(st);
        (void)rc;
        sqlite3_finalize(st);
    }
    long run_id = kdb_current_run_id(conn, task_id);
    if (run_id >= 0) {
        sqlite3_stmt *rs = NULL;
        if (sqlite3_prepare_v2(conn,
                "UPDATE task_runs SET claim_expires = ? WHERE id = ?",
                -1, &rs, NULL) == SQLITE_OK) {
            sqlite3_bind_int64(rs, 1, grace);
            sqlite3_bind_int64(rs, 2, run_id);
            sqlite3_step(rs);
            sqlite3_finalize(rs);
        }
    }
    char pl[1024];
    snprintf(pl, sizeof(pl),
             "{\"reason\":\"%s\",\"claim_lock\":\"%s\",\"claim_expires_now\":%ld",
             reason ? reason : "", claim_lock ? claim_lock : "", grace);
    size_t L = strlen(pl);
    if (term_json) snprintf(pl + L, sizeof(pl) - L, ",%s", term_json);
    L = strlen(pl);
    snprintf(pl + L, sizeof(pl) - L, "}");
    kdb_append_event(conn, task_id, run_id, "reclaim_deferred", pl);
}

void kb_vec_push(char ***vec, char *s)
{
    int n = 0;
    while ((*vec)[n]) n++;
    if (n + 2 >= (int)(sizeof(char *) == 8 ? 1 : 1) && 0) { } /* noop */
    /* grow by powers of two-ish: realloc +8 */
    char **nv = realloc(*vec, (size_t)(n + 8) * sizeof(char *));
    if (!nv) { free(s); return; }
    *vec = nv;
    (*vec)[n] = s;
    (*vec)[n + 1] = NULL;
}
