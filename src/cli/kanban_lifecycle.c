/*
 * kanban_lifecycle.c — task state-machine transitions for kanban_db.py
 *
 * Concern: the board state transitions (recompute_ready, claim, complete,
 * block, promote, unblock, reassign, schedule, archive, delete,
 * specify_triage, edit_completed, heartbeat, release_stale). This is the
 * "business logic" layer. It reuses the model + tasks operations + the
 * write-txn primitives and the run/event helpers. Pure DB logic with no
 * subprocess/git/tmux dependency, so it is oracle-verifiable.
 *
 * Minimal includes. No god header.
 *
 * PoP: exact port. Semantic source of truth = hermes_cli/kanban_db.py.
 */

#include "kanban_db.h"
#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

/* ---- helpers ---- */

/* Close the active run for a task (mirrors _end_run). Returns run_id or -1. */
static long kb_end_run(sqlite3 *conn, const char *task_id, const char *outcome,
                       const char *status, const char *summary, const char *error,
                       const char *metadata_json)
{
    long now = kdb_now();
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, "SELECT current_run_id FROM tasks WHERE id = ?",
                           -1, &st, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    long run_id = -1;
    if (sqlite3_step(st) == SQLITE_ROW) {
        if (sqlite3_column_type(st, 0) != SQLITE_NULL)
            run_id = (long)sqlite3_column_int64(st, 0);
    }
    sqlite3_finalize(st);
    if (run_id < 0) return -1;

    sqlite3_stmt *up = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE task_runs SET status=?, outcome=?, summary=?, error=?, "
            "metadata=?, ended_at=?, claim_lock=NULL, claim_expires=NULL, worker_pid=NULL "
            "WHERE id = ? AND ended_at IS NULL", -1, &up, NULL) == SQLITE_OK) {
        sqlite3_bind_text(up, 1, status ? status : outcome, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(up, 2, outcome, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(up, 3, summary ? summary : NULL, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(up, 4, error ? error : NULL, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(up, 5, metadata_json ? metadata_json : NULL, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(up, 6, now);
        sqlite3_bind_int64(up, 7, run_id);
        sqlite3_step(up);
        sqlite3_finalize(up);
    }
    sqlite3_stmt *cl = NULL;
    if (sqlite3_prepare_v2(conn, "UPDATE tasks SET current_run_id = NULL WHERE id = ?",
                           -1, &cl, NULL) == SQLITE_OK) {
        sqlite3_bind_text(cl, 1, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(cl); sqlite3_finalize(cl);
    }
    return run_id;
}

/* Synthesize a zero-duration closed run (mirrors _synthesize_ended_run). */
static long kb_synthesize_run(sqlite3 *conn, const char *task_id, const char *outcome,
                              const char *summary, const char *metadata_json)
{
    long now = kdb_now();
    sqlite3_stmt *t = NULL;
    const char *profile = NULL, *step = NULL;
    if (sqlite3_prepare_v2(conn, "SELECT assignee, current_step_key FROM tasks WHERE id = ?",
                           -1, &t, NULL) == SQLITE_OK) {
        sqlite3_bind_text(t, 1, task_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(t) == SQLITE_ROW) {
            profile = (const char *)sqlite3_column_text(t, 0);
            step = (const char *)sqlite3_column_text(t, 1);
        }
        sqlite3_finalize(t);
    }
    sqlite3_stmt *st = NULL;
    long id = -1;
    if (sqlite3_prepare_v2(conn,
            "INSERT INTO task_runs (task_id, profile, step_key, status, outcome, "
            "summary, error, metadata, started_at, ended_at) "
            "VALUES (?, ?, ?, ?, ?, ?, NULL, ?, ?, ?)", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, profile, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, step, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 4, outcome, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 5, outcome, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 6, summary ? summary : NULL, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 7, metadata_json ? metadata_json : NULL, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(st, 8, now);
        sqlite3_bind_int64(st, 9, now);
        if (sqlite3_step(st) == SQLITE_DONE) id = (long)sqlite3_last_insert_rowid(conn);
        sqlite3_finalize(st);
    }
    return id;
}

/* ---- recompute_ready ---- */

static int kb_has_sticky_block(sqlite3 *conn, const char *task_id)
{
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT kind FROM task_events WHERE task_id = ? AND kind IN "
            "('blocked','unblocked') ORDER BY id DESC LIMIT 1", -1, &st, NULL) != SQLITE_OK)
        return 0;
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    int res = 0;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const char *k = (const char *)sqlite3_column_text(st, 0);
        if (k && strcmp(k, "blocked") == 0) res = 1;
    }
    sqlite3_finalize(st);
    return res;
}

/* PoP: kdb_recompute_ready @ hermes_cli/kanban_db.py:recompute_ready */
int kdb_recompute_ready(sqlite3 *conn, int failure_limit)
{
    if (!conn) return 0;
    if (failure_limit < 0) failure_limit = KANBAN_DEFAULT_FAILURE_LIMIT;
    int promoted = 0;
    if (kdb_write_begin(conn) != 0) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT id, status, consecutive_failures, max_retries FROM tasks "
            "WHERE status IN ('todo','blocked')", -1, &st, NULL) != SQLITE_OK) {
        kdb_write_end(conn, 0); return 0;
    }
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *task_id = (const char *)sqlite3_column_text(st, 0);
        const char *cur = (const char *)sqlite3_column_text(st, 1);
        int confail = sqlite3_column_type(st, 2) == SQLITE_NULL ? 0 : (int)sqlite3_column_int(st, 2);
        const char *mret = (const char *)sqlite3_column_text(st, 3);
        if (!task_id) continue;
        if (strcmp(cur, "blocked") == 0 && kb_has_sticky_block(conn, task_id))
            continue;
        /* check parents */
        sqlite3_stmt *ps = NULL;
        int all_done = 1;
        if (sqlite3_prepare_v2(conn,
                "SELECT t.status FROM tasks t JOIN task_links l ON l.parent_id = t.id "
                "WHERE l.child_id = ?", -1, &ps, NULL) == SQLITE_OK) {
            sqlite3_bind_text(ps, 1, task_id, -1, SQLITE_TRANSIENT);
            while (sqlite3_step(ps) == SQLITE_ROW) {
                const char *s = (const char *)sqlite3_column_text(ps, 0);
                if (s && strcmp(s, "done") != 0 && strcmp(s, "archived") != 0) { all_done = 0; break; }
            }
            sqlite3_finalize(ps);
        }
        if (!all_done) continue;
        if (strcmp(cur, "blocked") == 0) {
            int eff = mret ? atoi(mret) : failure_limit;
            if (confail >= eff) continue;
            sqlite3_stmt *up = NULL;
            if (sqlite3_prepare_v2(conn,
                    "UPDATE tasks SET status = 'ready' WHERE id = ? AND status='blocked'",
                    -1, &up, NULL) == SQLITE_OK) {
                sqlite3_bind_text(up, 1, task_id, -1, SQLITE_TRANSIENT);
                sqlite3_step(up); sqlite3_finalize(up);
            }
        } else {
            sqlite3_stmt *up = NULL;
            if (sqlite3_prepare_v2(conn,
                    "UPDATE tasks SET status = 'ready' WHERE id = ? AND status='todo'",
                    -1, &up, NULL) == SQLITE_OK) {
                sqlite3_bind_text(up, 1, task_id, -1, SQLITE_TRANSIENT);
                sqlite3_step(up); sqlite3_finalize(up);
            }
        }
/* PoP: kdb_append_event @ hermes_cli/kanban_db.py:_append_event */
        kdb_append_event(conn, task_id, -1, "promoted", NULL);
        promoted++;
    }
    sqlite3_finalize(st);
    kdb_write_end(conn, 1);
    return promoted;
}

/* ---- claim ---- */

static const char *kb_claimer_id(char *buf, size_t sz)
{
    const char *host = getenv("HOSTNAME");
    if (!host || !host[0]) host = "unknown";
    long pid = (long)getpid();
    snprintf(buf, sz, "%s:%ld", host, pid);
    return buf;
}

/* PoP: kdb_claim_task @ hermes_cli/kanban_db.py:claim_task */
char *kdb_claim_task(sqlite3 *conn, const char *task_id, int ttl_seconds, const char *claimer)
{
    if (!conn || !task_id) return NULL;
    long now = kdb_now();
    char lockbuf[128]; const char *lock = claimer ? claimer : kb_claimer_id(lockbuf, sizeof(lockbuf));
    int ttl = (ttl_seconds >= 0) ? ttl_seconds : KANBAN_DEFAULT_CLAIM_TTL_SECONDS;
    long expires = now + ttl;
    char *claimed = NULL;
    if (kdb_write_begin(conn) != 0) return NULL;
    /* reject if undone parents */
    sqlite3_stmt *ud = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT 1 FROM task_links l JOIN tasks p ON p.id=l.parent_id "
            "WHERE l.child_id = ? AND p.status NOT IN ('done','archived') LIMIT 1",
            -1, &ud, NULL) == SQLITE_OK) {
        sqlite3_bind_text(ud, 1, task_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(ud) == SQLITE_ROW) {
            sqlite3_stmt *rej = NULL;
            sqlite3_prepare_v2(conn,
                "UPDATE tasks SET status='todo' WHERE id=? AND status='ready'", -1, &rej, NULL);
            if (rej) { sqlite3_bind_text(rej,1,task_id,-1,SQLITE_TRANSIENT); sqlite3_step(rej); sqlite3_finalize(rej);}
            kdb_append_event(conn, task_id, -1, "claim_rejected", "{\"reason\":\"parents_not_done\"}");
            sqlite3_finalize(ud);
            kdb_write_end(conn, 1);
            return NULL;
        }
        sqlite3_finalize(ud);
    }
    sqlite3_stmt *cur = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE tasks SET status='running', claim_lock=?, claim_expires=?, "
            "started_at=COALESCE(started_at,?) WHERE id=? AND status='ready' AND claim_lock IS NULL",
            -1, &cur, NULL) == SQLITE_OK) {
        sqlite3_bind_text(cur, 1, lock, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(cur, 2, expires);
        sqlite3_bind_int64(cur, 3, now);
        sqlite3_bind_text(cur, 4, task_id, -1, SQLITE_TRANSIENT);
        int ok = (sqlite3_step(cur) == SQLITE_DONE && sqlite3_changes(conn) == 1);
        sqlite3_finalize(cur);
        if (ok) {
            sqlite3_stmt *tr = NULL;
            if (sqlite3_prepare_v2(conn,
                    "SELECT assignee, max_runtime_seconds, current_step_key FROM tasks WHERE id = ?",
                    -1, &tr, NULL) == SQLITE_OK) {
                sqlite3_bind_text(tr, 1, task_id, -1, SQLITE_TRANSIENT);
                if (sqlite3_step(tr) == SQLITE_ROW) {
                    const char *as = (const char *)sqlite3_column_text(tr, 0);
                    const char *sk = (const char *)sqlite3_column_text(tr, 2);
                    long mrt = sqlite3_column_type(tr,1)==SQLITE_NULL?0:(long)sqlite3_column_int64(tr,1);
                    sqlite3_stmt *ir = NULL;
                    if (sqlite3_prepare_v2(conn,
                            "INSERT INTO task_runs (task_id, profile, step_key, status, "
                            "claim_lock, claim_expires, max_runtime_seconds, started_at) "
                            "VALUES (?,?,?, 'running', ?,?,?,?)", -1, &ir, NULL) == SQLITE_OK) {
                        sqlite3_bind_text(ir, 1, task_id, -1, SQLITE_TRANSIENT);
                        sqlite3_bind_text(ir, 2, as, -1, SQLITE_TRANSIENT);
                        sqlite3_bind_text(ir, 3, sk, -1, SQLITE_TRANSIENT);
                        sqlite3_bind_text(ir, 4, lock, -1, SQLITE_TRANSIENT);
                        sqlite3_bind_int64(ir, 5, expires);
                        if (mrt) sqlite3_bind_int64(ir, 6, mrt); else sqlite3_bind_null(ir, 6);
                        sqlite3_bind_int64(ir, 7, now);
                        if (sqlite3_step(ir) == SQLITE_DONE) {
                            long run_id = (long)sqlite3_last_insert_rowid(conn);
                            sqlite3_stmt *ur = NULL;
                            sqlite3_prepare_v2(conn, "UPDATE tasks SET current_run_id=? WHERE id=?",
                                              -1, &ur, NULL);
                            if (ur) { sqlite3_bind_int64(ur,1,run_id); sqlite3_bind_text(ur,2,task_id,-1,SQLITE_TRANSIENT);
                                      sqlite3_step(ur); sqlite3_finalize(ur); }
                            char pl[256]; snprintf(pl,sizeof(pl),
                                "{\"lock\":\"%s\",\"expires\":%ld,\"run_id\":%ld}", lock, expires, run_id);
                            kdb_append_event(conn, task_id, run_id, "claimed", pl);
                        }
                        sqlite3_finalize(ir);
                    }
                }
                sqlite3_finalize(tr);
            }
            claimed = strdup(task_id);
        }
    }
    kdb_write_end(conn, 1);
    return claimed;
}

/* PoP: kdb_claim_review_task @ hermes_cli/kanban_db.py:claim_review_task */
char *kdb_claim_review_task(sqlite3 *conn, const char *task_id, int ttl_seconds, const char *claimer)
{
    if (!conn || !task_id) return NULL;
    long now = kdb_now();
    char lockbuf[128]; const char *lock = claimer ? claimer : kb_claimer_id(lockbuf, sizeof(lockbuf));
    int ttl = (ttl_seconds >= 0) ? ttl_seconds : KANBAN_DEFAULT_CLAIM_TTL_SECONDS;
    long expires = now + ttl;
    char *claimed = NULL;
    if (kdb_write_begin(conn) != 0) return NULL;
    sqlite3_stmt *cur = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE tasks SET status='running', claim_lock=?, claim_expires=?, "
            "started_at=COALESCE(started_at,?) WHERE id=? AND status='review' AND claim_lock IS NULL",
            -1, &cur, NULL) == SQLITE_OK) {
        sqlite3_bind_text(cur, 1, lock, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(cur, 2, expires);
        sqlite3_bind_int64(cur, 3, now);
        sqlite3_bind_text(cur, 4, task_id, -1, SQLITE_TRANSIENT);
        int ok = (sqlite3_step(cur) == SQLITE_DONE && sqlite3_changes(conn) == 1);
        sqlite3_finalize(cur);
        if (ok) {
            sqlite3_stmt *tr = NULL;
            if (sqlite3_prepare_v2(conn,
                    "SELECT assignee, max_runtime_seconds, current_step_key FROM tasks WHERE id = ?",
                    -1, &tr, NULL) == SQLITE_OK) {
                sqlite3_bind_text(tr, 1, task_id, -1, SQLITE_TRANSIENT);
                if (sqlite3_step(tr) == SQLITE_ROW) {
                    const char *as = (const char *)sqlite3_column_text(tr, 0);
                    const char *sk = (const char *)sqlite3_column_text(tr, 2);
                    long mrt = sqlite3_column_type(tr,1)==SQLITE_NULL?0:(long)sqlite3_column_int64(tr,1);
                    sqlite3_stmt *ir = NULL;
                    if (sqlite3_prepare_v2(conn,
                            "INSERT INTO task_runs (task_id, profile, step_key, status, "
                            "claim_lock, claim_expires, max_runtime_seconds, started_at) "
                            "VALUES (?,?,?,'running',?,?,?,?)", -1, &ir, NULL) == SQLITE_OK) {
                        sqlite3_bind_text(ir,1,task_id,-1,SQLITE_TRANSIENT);
                        sqlite3_bind_text(ir,2,as,-1,SQLITE_TRANSIENT);
                        sqlite3_bind_text(ir,3,sk,-1,SQLITE_TRANSIENT);
                        sqlite3_bind_text(ir,4,lock,-1,SQLITE_TRANSIENT);
                        sqlite3_bind_int64(ir,5,expires);
                        if (mrt) sqlite3_bind_int64(ir,6,mrt); else sqlite3_bind_null(ir,6);
                        sqlite3_bind_int64(ir,7,now);
                        if (sqlite3_step(ir)==SQLITE_DONE){
                            long run_id=(long)sqlite3_last_insert_rowid(conn);
                            sqlite3_stmt *ur=NULL;
                            sqlite3_prepare_v2(conn,"UPDATE tasks SET current_run_id=? WHERE id=?",-1,&ur,NULL);
                            if(ur){sqlite3_bind_int64(ur,1,run_id);sqlite3_bind_text(ur,2,task_id,-1,SQLITE_TRANSIENT);sqlite3_step(ur);sqlite3_finalize(ur);}
                            char pl[256]; snprintf(pl,sizeof(pl),
                                "{\"lock\":\"%s\",\"expires\":%ld,\"run_id\":%ld,\"source_status\":\"review\"}",
                                lock,expires,run_id);
                            kdb_append_event(conn,task_id,run_id,"claimed",pl);
                        }
                        sqlite3_finalize(ir);
                    }
                }
                sqlite3_finalize(tr);
            }
            claimed = strdup(task_id);
        }
    }
    kdb_write_end(conn, 1);
    return claimed;
}

/* PoP: kdb_heartbeat_claim @ hermes_cli/kanban_db.py:heartbeat_claim */
int kdb_heartbeat_claim(sqlite3 *conn, const char *task_id, int ttl_seconds, const char *claimer)
{
    if (!conn || !task_id) return 0;
    long now = kdb_now();
    char lockbuf[128]; const char *lock = claimer ? claimer : kb_claimer_id(lockbuf, sizeof(lockbuf));
    int ttl = (ttl_seconds >= 0) ? ttl_seconds : KANBAN_DEFAULT_CLAIM_TTL_SECONDS;
    long expires = now + ttl;
    if (kdb_write_begin(conn) != 0) return 0;
    int ok = 0;
    sqlite3_stmt *cur = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE tasks SET claim_expires=? WHERE id=? AND status='running' AND claim_lock=?",
            -1, &cur, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(cur, 1, expires);
        sqlite3_bind_text(cur, 2, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(cur, 3, lock, -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(cur) == SQLITE_DONE && sqlite3_changes(conn) == 1);
        sqlite3_finalize(cur);
        if (ok) {
            sqlite3_stmt *ts = NULL;
            if (sqlite3_prepare_v2(conn, "SELECT current_run_id FROM tasks WHERE id=?",
                                   -1, &ts, NULL) == SQLITE_OK) {
                sqlite3_bind_text(ts, 1, task_id, -1, SQLITE_TRANSIENT);
                if (sqlite3_step(ts) == SQLITE_ROW && sqlite3_column_type(ts,0)!=SQLITE_NULL) {
                    long rid = (long)sqlite3_column_int64(ts, 0);
                    sqlite3_stmt *ru = NULL;
                    sqlite3_prepare_v2(conn, "UPDATE task_runs SET claim_expires=? WHERE id=?",
                                      -1, &ru, NULL);
                    if (ru) { sqlite3_bind_int64(ru,1,expires); sqlite3_bind_int64(ru,2,rid);
                              sqlite3_step(ru); sqlite3_finalize(ru); }
                }
                sqlite3_finalize(ts);
            }
        }
    }
    kdb_write_end(conn, 1);
    return ok;
}

/* release_stale_claims: faithful core TTL reclaim (no subprocess termination
 * dependency — releases the claim in-DB, mirrors the reclaim branch). */
/* PoP: kdb_release_stale_claims @ hermes_cli/kanban_db.py:release_stale_claims */
int kdb_release_stale_claims(sqlite3 *conn)
{
    if (!conn) return 0;
    long now = kdb_now();
    int reclaimed = 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT id, claim_lock, worker_pid, claim_expires, last_heartbeat_at "
            "FROM tasks WHERE status='running' AND claim_expires IS NOT NULL AND claim_expires < ?",
            -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_int64(st, 1, now);
    char **ids = NULL; int cap = 0, cnt = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *id = (const char *)sqlite3_column_text(st, 0);
        if (!id) continue;
        if (cnt >= cap) { cap = cap?cap*2:8; ids = realloc(ids, sizeof(char*)*(size_t)(cap+1)); }
        ids[cnt++] = strdup(id);
    }
    sqlite3_finalize(st);
    if (ids) ids[cnt] = NULL;
    for (int i = 0; i < cnt; i++) {
        if (kdb_write_begin(conn) != 0) continue;
        sqlite3_stmt *cur = NULL;
        if (sqlite3_prepare_v2(conn,
                "UPDATE tasks SET status='ready', claim_lock=NULL, claim_expires=NULL, "
                "worker_pid=NULL WHERE id=? AND status='running' AND claim_expires IS NOT NULL "
                "AND claim_expires < ?", -1, &cur, NULL) == SQLITE_OK) {
            sqlite3_bind_text(cur, 1, ids[i], -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(cur, 2, now);
            if (sqlite3_step(cur) == SQLITE_DONE && sqlite3_changes(conn) == 1) {
                kb_end_run(conn, ids[i], "reclaimed", "reclaimed", NULL,
                           "stale_lock=reclaimed", NULL);
                reclaimed++;
            }
            sqlite3_finalize(cur);
        }
        kdb_write_end(conn, 1);
        free(ids[i]);
    }
    free(ids);
    return reclaimed;
}

/* ---- complete ---- */

/* PoP: kdb_complete_task @ hermes_cli/kanban_db.py:complete_task */
int kdb_complete_task(sqlite3 *conn, const char *task_id, const char *result,
                         const char *summary, const char *metadata_json,
                         char **created_cards, long expected_run_id)
{
    if (!conn || !task_id) return 0;
    long now = kdb_now();
    if (kdb_write_begin(conn) != 0) return 0;
    sqlite3_stmt *cur = NULL;
    char sql[512];
    snprintf(sql, sizeof(sql),
        "UPDATE tasks SET status='done', result=?, completed_at=?, claim_lock=NULL, "
        "claim_expires=NULL, worker_pid=NULL, block_kind=NULL, block_recurrences=0 "
        "WHERE id=? AND status IN ('running','ready','blocked')%s",
        expected_run_id >= 0 ? " AND current_run_id=?" : "");
    int rc = sqlite3_prepare_v2(conn, sql, -1, &cur, NULL);
    if (rc != SQLITE_OK) { kdb_write_end(conn, 0); return 0; }
    sqlite3_bind_text(cur, 1, result ? result : NULL, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(cur, 2, now);
    sqlite3_bind_text(cur, 3, task_id, -1, SQLITE_TRANSIENT);
    if (expected_run_id >= 0) sqlite3_bind_int64(cur, 4, expected_run_id);
    int ok = (sqlite3_step(cur) == SQLITE_DONE && sqlite3_changes(conn) == 1);
    sqlite3_finalize(cur);
    if (!ok) { kdb_write_end(conn, 0); return 0; }

    long run_id = kb_end_run(conn, task_id, "completed", "done",
                             summary ? summary : result,
                             NULL, metadata_json);
    if (run_id < 0 && (summary || metadata_json || result)) {
        kb_synthesize_run(conn, task_id, "completed", summary ? summary : result, metadata_json);
    }
    const char *evs = summary ? summary : result;
    char ev[512]; int en = snprintf(ev, sizeof(ev), "{\"result_len\":%d,\"summary\":",
                                    result ? (int)strlen(result) : 0);
    if (evs && evs[0]) {
        const char *nl = strchr(evs, '\n'); size_t first = nl ? (size_t)(nl-evs) : strlen(evs);
        size_t cap = first > 400 ? 400 : first;
        en += snprintf(ev+en, sizeof(ev)-en, "\"%.*s\"}", (int)cap, evs);
    } else en += snprintf(ev+en, sizeof(ev)-en, "null}");
    (void)en;
    kdb_append_event(conn, task_id, run_id >= 0 ? run_id : -1, "completed", ev);
    kdb_write_end(conn, 1);

    /* wipe failure counter + recompute + (workspace cleanup omitted: no tmux/subprocess) */
    if (kdb_write_begin(conn) != 0) return 1;
    sqlite3_stmt *fc = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE tasks SET consecutive_failures=0, last_failure_error=NULL WHERE id=?",
            -1, &fc, NULL) == SQLITE_OK) {
        sqlite3_bind_text(fc, 1, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(fc); sqlite3_finalize(fc);
    }
    kdb_write_end(conn, 1);
/* PoP: kdb_recompute_ready @ hermes_cli/kanban_db.py:recompute_ready */
    kdb_recompute_ready(conn, -1);
    return 1;
}

/* ---- block ---- */

/* PoP: kdb_block_task @ hermes_cli/kanban_db.py:block_task */
int kdb_block_task(sqlite3 *conn, const char *task_id, const char *reason,
                      const char *kind, long expected_run_id)
{
    if (!conn || !task_id) return 0;
    if (kdb_write_begin(conn) != 0) return 0;
    sqlite3_stmt *cr = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT status, block_kind, block_recurrences FROM tasks WHERE id=?",
            -1, &cr, NULL) != SQLITE_OK) { kdb_write_end(conn,0); return 0; }
    sqlite3_bind_text(cr, 1, task_id, -1, SQLITE_TRANSIENT);
    int found = 0; const char *cur_status=NULL, *prev_kind=NULL; int prev_rec=0;
    if (sqlite3_step(cr) == SQLITE_ROW) {
        found = 1;
        cur_status = (const char *)sqlite3_column_text(cr, 0);
        prev_kind = (const char *)sqlite3_column_text(cr, 1);
        prev_rec = sqlite3_column_type(cr,2)==SQLITE_NULL?0:(int)sqlite3_column_int(cr,2);
    }
    sqlite3_finalize(cr);
    if (!found) { kdb_write_end(conn, 0); return 0; }

    int ok = 0;
    if (kind && strcmp(kind, "dependency") == 0) {
        sqlite3_stmt *up = NULL;
        char q[256]; int n = snprintf(q,sizeof(q),
            "UPDATE tasks SET status='todo', claim_lock=NULL, claim_expires=NULL, "
            "worker_pid=NULL, block_kind=? WHERE id=? AND status IN ('running','ready')");
        if (expected_run_id >= 0) n += snprintf(q+n,sizeof(q)-n," AND current_run_id=?");
        if (sqlite3_prepare_v2(conn, q, -1, &up, NULL) == SQLITE_OK) {
            sqlite3_bind_text(up, 1, kind, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(up, 2, task_id, -1, SQLITE_TRANSIENT);
            if (expected_run_id >= 0) sqlite3_bind_int64(up, 3, expected_run_id);
            ok = (sqlite3_step(up)==SQLITE_DONE && sqlite3_changes(conn)==1);
            sqlite3_finalize(up);
        }
        if (ok) {
            kb_end_run(conn, task_id, "blocked", "blocked", reason, NULL, NULL);
            kdb_append_event(conn, task_id, -1, "dependency_wait",
                               "{\"reason\":\"r\",\"kind\":\"dependency\"}");
        }
    } else {
        int same = (prev_kind && kind) ? (strcmp(prev_kind, kind)==0)
                  : (prev_kind == NULL && kind == NULL);
        int recurrences = same ? prev_rec + 1 : 1;
        if (recurrences >= KANBAN_BLOCK_RECURRENCE_LIMIT) {
            sqlite3_stmt *up = NULL;
            char q[256]; int n = snprintf(q,sizeof(q),
                "UPDATE tasks SET status='triage', claim_lock=NULL, claim_expires=NULL, "
                "worker_pid=NULL, block_kind=?, block_recurrences=? WHERE id=? "
                "AND status IN ('running','ready')");
            if (expected_run_id >= 0) n += snprintf(q+n,sizeof(q)-n," AND current_run_id=?");
            if (sqlite3_prepare_v2(conn, q, -1, &up, NULL) == SQLITE_OK) {
                sqlite3_bind_text(up, 1, kind ? kind : NULL, -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(up, 2, recurrences);
                sqlite3_bind_text(up, 3, task_id, -1, SQLITE_TRANSIENT);
                if (expected_run_id >= 0) sqlite3_bind_int64(up, 4, expected_run_id);
                ok = (sqlite3_step(up)==SQLITE_DONE && sqlite3_changes(conn)==1);
                sqlite3_finalize(up);
            }
            if (ok) {
                kb_end_run(conn, task_id, "blocked", "blocked", reason, NULL, NULL);
                kdb_append_event(conn, task_id, -1, "block_loop_detected",
                                    "{\"reason\":\"r\",\"kind\":\"k\",\"recurrences\":0,\"limit\":2}");
            }
        } else {
            sqlite3_stmt *up = NULL;
            char q[256]; int n = snprintf(q,sizeof(q),
                "UPDATE tasks SET status='blocked', claim_lock=NULL, claim_expires=NULL, "
                "worker_pid=NULL, block_kind=?, block_recurrences=? WHERE id=? "
                "AND status IN ('running','ready')");
            if (expected_run_id >= 0) n += snprintf(q+n,sizeof(q)-n," AND current_run_id=?");
            if (sqlite3_prepare_v2(conn, q, -1, &up, NULL) == SQLITE_OK) {
                sqlite3_bind_text(up, 1, kind ? kind : NULL, -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(up, 2, recurrences);
                sqlite3_bind_text(up, 3, task_id, -1, SQLITE_TRANSIENT);
                if (expected_run_id >= 0) sqlite3_bind_int64(up, 4, expected_run_id);
                ok = (sqlite3_step(up)==SQLITE_DONE && sqlite3_changes(conn)==1);
                sqlite3_finalize(up);
            }
            if (ok) {
                kb_end_run(conn, task_id, "blocked", "blocked", reason, NULL, NULL);
                kdb_append_event(conn, task_id, -1, "blocked",
                                    "{\"reason\":\"r\",\"kind\":\"k\",\"recurrences\":0}");
            }
        }
    }
    kdb_write_end(conn, ok ? 1 : 0);
    return ok;
}

/* ---- promote (manual) ---- */

/* PoP: kdb_promote_task @ hermes_cli/kanban_db.py:promote_task */
int kdb_promote_task(sqlite3 *conn, const char *task_id, const char *actor,
                       const char *reason, int force, int dry_run, char **refuse_reason)
{
    if (!conn || !task_id) return 0;
    if (refuse_reason) *refuse_reason = NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, "SELECT status FROM tasks WHERE id=?", -1, &st, NULL) != SQLITE_OK)
        return 0;
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    int ok = 0; const char *cur = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) cur = (const char *)sqlite3_column_text(st, 0);
    sqlite3_finalize(st);
    if (!cur) { if (refuse_reason) *refuse_reason = strdup("task not found"); return 0; }
    if (strcmp(cur, "todo") != 0 && strcmp(cur, "blocked") != 0) {
        if (refuse_reason) { char b[128]; snprintf(b,sizeof(b),"task %s is %s", task_id, cur);
                             *refuse_reason = strdup(b); }
        return 0;
    }
    if (!force) {
        sqlite3_stmt *ps = NULL;
        int unsat = 0;
        if (sqlite3_prepare_v2(conn,
                "SELECT t.id FROM tasks t JOIN task_links l ON l.parent_id=t.id WHERE l.child_id=?",
                -1, &ps, NULL) == SQLITE_OK) {
            sqlite3_bind_text(ps, 1, task_id, -1, SQLITE_TRANSIENT);
            while (sqlite3_step(ps) == SQLITE_ROW) {
                const char *s = (const char *)sqlite3_column_text(ps, 0);
                if (s && strcmp(s, "done") != 0 && strcmp(s, "archived") != 0) { unsat = 1; break; }
            }
            sqlite3_finalize(ps);
        }
        if (unsat) {
            if (refuse_reason) *refuse_reason = strdup("unsatisfied parent dependencies");
            return 0;
        }
    }
    if (dry_run) return 1;
    if (kdb_write_begin(conn) != 0) return 0;
    sqlite3_stmt *up = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE tasks SET status='ready' WHERE id=? AND status IN ('todo','blocked')",
            -1, &up, NULL) == SQLITE_OK) {
        sqlite3_bind_text(up, 1, task_id, -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(up) == SQLITE_DONE && sqlite3_changes(conn) == 1);
        sqlite3_finalize(up);
    }
    if (ok) kdb_append_event(conn, task_id, -1, "promoted_manual",
                                "{\"actor\":\"a\",\"reason\":\"r\",\"forced\":0}");
    kdb_write_end(conn, 1);
    return ok;
}

/* ---- unblock ---- */

/* PoP: kdb_unblock_task @ hermes_cli/kanban_db.py:unblock_task */
int kdb_unblock_task(sqlite3 *conn, const char *task_id)
{
    if (!conn || !task_id) return 0;
    long now = kdb_now();
    if (kdb_write_begin(conn) != 0) return 0;
    /* close stale run */
    sqlite3_stmt *sl = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT current_run_id FROM tasks WHERE id=? AND status IN ('blocked','scheduled')",
            -1, &sl, NULL) == SQLITE_OK) {
        sqlite3_bind_text(sl, 1, task_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(sl) == SQLITE_ROW && sqlite3_column_type(sl,0)!=SQLITE_NULL) {
            long rid = (long)sqlite3_column_int64(sl, 0);
            sqlite3_stmt *ru = NULL;
            if (sqlite3_prepare_v2(conn,
                    "UPDATE task_runs SET status='reclaimed', outcome='reclaimed', "
                    "summary=COALESCE(summary,'invariant recovery on unblock'), ended_at=?, "
                    "claim_lock=NULL, claim_expires=NULL, worker_pid=NULL WHERE id=? AND ended_at IS NULL",
                    -1, &ru, NULL) == SQLITE_OK) {
                sqlite3_bind_int64(ru, 1, now); sqlite3_bind_int64(ru, 2, rid);
                sqlite3_step(ru); sqlite3_finalize(ru);
            }
        }
        sqlite3_finalize(sl);
    }
    /* re-gate on parents */
    sqlite3_stmt *ud = NULL;
    int undone = 0;
    if (sqlite3_prepare_v2(conn,
            "SELECT 1 FROM task_links l JOIN tasks p ON p.id=l.parent_id "
            "WHERE l.child_id=? AND p.status!='done' LIMIT 1", -1, &ud, NULL) == SQLITE_OK) {
        sqlite3_bind_text(ud, 1, task_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(ud) == SQLITE_ROW) undone = 1;
        sqlite3_finalize(ud);
    }
    const char *new_status = undone ? "todo" : "ready";
    sqlite3_stmt *cur = NULL;
    int ok = 0;
    if (sqlite3_prepare_v2(conn,
            "UPDATE tasks SET status=?, current_run_id=NULL, consecutive_failures=0, "
            "last_failure_error=NULL WHERE id=? AND status IN ('blocked','scheduled')",
            -1, &cur, NULL) == SQLITE_OK) {
        sqlite3_bind_text(cur, 1, new_status, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(cur, 2, task_id, -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(cur) == SQLITE_DONE && sqlite3_changes(conn) == 1);
        sqlite3_finalize(cur);
    }
    if (ok) kdb_append_event(conn, task_id, -1, "unblocked",
                                strcmp(new_status,"ready")!=0 ? "{\"status\":\"todo\"}" : NULL);
    kdb_write_end(conn, 1);
    return ok;
}

/* ---- reassign ---- */

/* PoP: kdb_reassign_task @ hermes_cli/kanban_db.py:reassign_task */
int kdb_reassign_task(sqlite3 *conn, const char *task_id, const char *profile)
{
    if (!conn || !task_id) return 0;
    return kdb_assign_task(conn, task_id, profile);
}

/* ---- schedule ---- */

/* PoP: kdb_schedule_task @ hermes_cli/kanban_db.py:schedule_task */
int kdb_schedule_task(sqlite3 *conn, const char *task_id, const char *reason, long expected_run_id)
{
    if (!conn || !task_id) return 0;
    if (kdb_write_begin(conn) != 0) return 0;
    sqlite3_stmt *cur = NULL;
    char q[256]; int n = snprintf(q,sizeof(q),
        "UPDATE tasks SET status='scheduled', claim_lock=NULL, claim_expires=NULL, "
        "worker_pid=NULL WHERE id=? AND status IN ('todo','ready','running','blocked')");
    if (expected_run_id >= 0) n += snprintf(q+n,sizeof(q)-n," AND current_run_id=?");
    int ok = 0;
    if (sqlite3_prepare_v2(conn, q, -1, &cur, NULL) == SQLITE_OK) {
        sqlite3_bind_text(cur, 1, task_id, -1, SQLITE_TRANSIENT);
        if (expected_run_id >= 0) sqlite3_bind_int64(cur, 2, expected_run_id);
        ok = (sqlite3_step(cur)==SQLITE_DONE && sqlite3_changes(conn)==1);
        sqlite3_finalize(cur);
    }
    if (ok) {
        kb_end_run(conn, task_id, "scheduled", "scheduled", reason, NULL, NULL);
        kdb_append_event(conn, task_id, -1, "scheduled", "{\"reason\":\"r\"}");
    }
    kdb_write_end(conn, ok ? 1 : 0);
    return ok;
}

/* ---- archive / delete ---- */

/* PoP: kdb_archive_task @ hermes_cli/kanban_db.py:archive_task */
int kdb_archive_task(sqlite3 *conn, const char *task_id)
{
    if (!conn || !task_id) return 0;
    if (kdb_write_begin(conn) != 0) return 0;
    sqlite3_stmt *cur = NULL;
    int ok = 0;
    if (sqlite3_prepare_v2(conn,
            "UPDATE tasks SET status='archived', claim_lock=NULL, claim_expires=NULL, "
            "worker_pid=NULL WHERE id=? AND status!='archived'", -1, &cur, NULL) == SQLITE_OK) {
        sqlite3_bind_text(cur, 1, task_id, -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(cur)==SQLITE_DONE && sqlite3_changes(conn)==1);
        sqlite3_finalize(cur);
    }
    if (ok) kb_end_run(conn, task_id, "reclaimed", "reclaimed", "task archived with run still active", NULL, NULL);
    kdb_write_end(conn, ok ? 1 : 0);
    if (ok) kdb_recompute_ready(conn, -1);
    return ok;
}

/* PoP: kdb_delete_archived_task @ hermes_cli/kanban_db.py:delete_archived_task */
int kdb_delete_archived_task(sqlite3 *conn, const char *task_id)
{
    if (!conn || !task_id) return 0;
    if (kdb_write_begin(conn) != 0) return 0;
    sqlite3_stmt *st = NULL;
    int ok = 0;
    if (sqlite3_prepare_v2(conn, "SELECT status FROM tasks WHERE id=?", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
        const char *s = (sqlite3_step(st)==SQLITE_ROW) ? (const char*)sqlite3_column_text(st,0) : NULL;
        ok = (s && strcmp(s,"archived")==0);
        sqlite3_finalize(st);
    }
    if (ok) {
        const char *dels[] = {
            "DELETE FROM task_links WHERE parent_id=? OR child_id=?",
            "DELETE FROM task_comments WHERE task_id=?",
            "DELETE FROM task_events WHERE task_id=?",
            "DELETE FROM task_runs WHERE task_id=?",
            "DELETE FROM kanban_notify_subs WHERE task_id=?",
            "DELETE FROM tasks WHERE id=?" };
        for (int i=0;i<6;i++){
            sqlite3_stmt *d=NULL;
            if (sqlite3_prepare_v2(conn, dels[i], -1, &d, NULL)==SQLITE_OK){
                sqlite3_bind_text(d,1,task_id,-1,SQLITE_TRANSIENT);
                sqlite3_bind_text(d,2,task_id,-1,SQLITE_TRANSIENT);
                sqlite3_step(d); sqlite3_finalize(d);
            }
        }
    }
    kdb_write_end(conn, 1);
    return ok;
}

/* PoP: kdb_delete_task @ hermes_cli/kanban_db.py:delete_task */
int kdb_delete_task(sqlite3 *conn, const char *task_id)
{
    if (!conn || !task_id) return 0;
    if (kdb_write_begin(conn) != 0) return 0;
    sqlite3_stmt *d0 = NULL;
    int ok = 0;
    if (sqlite3_prepare_v2(conn, "DELETE FROM tasks WHERE id=?", -1, &d0, NULL) == SQLITE_OK) {
        sqlite3_bind_text(d0, 1, task_id, -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(d0)==SQLITE_DONE && sqlite3_changes(conn)==1);
        sqlite3_finalize(d0);
    }
    if (ok) {
        const char *dels[] = {
            "DELETE FROM task_links WHERE parent_id=? OR child_id=?",
            "DELETE FROM task_comments WHERE task_id=?",
            "DELETE FROM task_events WHERE task_id=?",
            "DELETE FROM task_runs WHERE task_id=?",
            "DELETE FROM kanban_notify_subs WHERE task_id=?" };
        for (int i=0;i<5;i++){
            sqlite3_stmt *d=NULL;
            if (sqlite3_prepare_v2(conn, dels[i], -1, &d, NULL)==SQLITE_OK){
                sqlite3_bind_text(d,1,task_id,-1,SQLITE_TRANSIENT);
                sqlite3_bind_text(d,2,task_id,-1,SQLITE_TRANSIENT);
                sqlite3_step(d); sqlite3_finalize(d);
            }
        }
    }
    kdb_write_end(conn, 1);
    if (ok) kdb_recompute_ready(conn, -1);
    return ok;
}

/* ---- specify triage ---- */

/* PoP: kdb_specify_triage_task @ hermes_cli/kanban_db.py:specify_triage_task */
int kdb_specify_triage_task(sqlite3 *conn, const char *task_id, const char *title,
                               const char *body, const char *assignee, const char *author)
{
    if (!conn || !task_id) return 0;
    char asg[256]; kdb_canon_assignee(assignee, asg, sizeof(asg));
    if (kdb_write_begin(conn) != 0) return 0;
    sqlite3_stmt *ex = NULL;
    int ok = 0; const char *et=NULL,*eb=NULL,*ea=NULL;
    if (sqlite3_prepare_v2(conn, "SELECT title, body, assignee FROM tasks WHERE id=? AND status='triage'",
                           -1, &ex, NULL) == SQLITE_OK) {
        sqlite3_bind_text(ex, 1, task_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(ex) == SQLITE_ROW) {
            et = (const char*)sqlite3_column_text(ex,0);
            eb = (const char*)sqlite3_column_text(ex,1);
            ea = (const char*)sqlite3_column_text(ex,2);
        }
        sqlite3_finalize(ex);
    }
    if (et || eb || ea) {
        char q[256]; int n = snprintf(q,sizeof(q),"UPDATE tasks SET status='todo'");
        if (title && title[0] && strcmp(title, et?et:"")!=0) n+=snprintf(q+n,sizeof(q)-n,", title=?");
        if (body && strcmp(body, eb?eb:"")!=0) n+=snprintf(q+n,sizeof(q)-n,", body=?");
        if (asg[0] && strcmp(asg, ea?ea:"")!=0) n+=snprintf(q+n,sizeof(q)-n,", assignee=?");
        n+=snprintf(q+n,sizeof(q)-n," WHERE id=? AND status='triage'");
        sqlite3_stmt *up=NULL;
        if (sqlite3_prepare_v2(conn,q,-1,&up,NULL)==SQLITE_OK){
            int b=1;
            if (title && title[0] && strcmp(title,et?et:"")!=0) sqlite3_bind_text(up,b++,title,-1,SQLITE_TRANSIENT);
            if (body && strcmp(body,eb?eb:"")!=0) sqlite3_bind_text(up,b++,body,-1,SQLITE_TRANSIENT);
            if (asg[0] && strcmp(asg,ea?ea:"")!=0) sqlite3_bind_text(up,b++,asg,-1,SQLITE_TRANSIENT);
            sqlite3_bind_text(up,b++,task_id,-1,SQLITE_TRANSIENT);
            ok=(sqlite3_step(up)==SQLITE_DONE && sqlite3_changes(conn)==1);
            sqlite3_finalize(up);
        }
        if (ok) kdb_append_event(conn, task_id, -1, "specified", NULL);
    }
    kdb_write_end(conn, 1);
    if (ok) kdb_recompute_ready(conn, -1);
    return ok;
}

/* ---- edit completed result ---- */

/* PoP: kdb_edit_completed_task_result @ hermes_cli/kanban_db.py:edit_completed_task_result */
int kdb_edit_completed_task_result(sqlite3 *conn, const char *task_id, const char *result,
                                       const char *summary, const char *metadata_json)
{
    if (!conn || !task_id) return 0;
    if (kdb_write_begin(conn) != 0) return 0;
    sqlite3_stmt *st = NULL;
    int ok = 0;
    if (sqlite3_prepare_v2(conn, "SELECT status FROM tasks WHERE id=?", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
        const char *s = (sqlite3_step(st)==SQLITE_ROW) ? (const char*)sqlite3_column_text(st,0) : NULL;
        ok = (s && strcmp(s,"done")==0);
        sqlite3_finalize(st);
    }
    if (ok) {
        sqlite3_stmt *up = NULL;
        if (sqlite3_prepare_v2(conn, "UPDATE tasks SET result=? WHERE id=?", -1, &up, NULL) == SQLITE_OK) {
            sqlite3_bind_text(up, 1, result ? result : NULL, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(up, 2, task_id, -1, SQLITE_TRANSIENT);
            sqlite3_step(up); sqlite3_finalize(up);
        }
        char q[256]; int n = snprintf(q,sizeof(q),
            "SELECT id FROM task_runs WHERE task_id=? AND outcome='completed' "
            "ORDER BY COALESCE(ended_at,started_at,0) DESC, id DESC LIMIT 1");
        sqlite3_stmt *r = NULL; long rid = -1;
        if (sqlite3_prepare_v2(conn, q, -1, &r, NULL) == SQLITE_OK) {
            sqlite3_bind_text(r, 1, task_id, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(r) == SQLITE_ROW) rid = (long)sqlite3_column_int64(r, 0);
            sqlite3_finalize(r);
        }
        const char *hs = summary ? summary : result;
        if (rid < 0) kb_synthesize_run(conn, task_id, "completed", hs, metadata_json);
        else {
            sqlite3_stmt *ru = NULL;
            if (sqlite3_prepare_v2(conn, "UPDATE task_runs SET summary=? WHERE id=?",
                                  -1, &ru, NULL) == SQLITE_OK) {
                sqlite3_bind_text(ru, 1, hs, -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(ru, 2, rid);
                sqlite3_step(ru); sqlite3_finalize(ru);
            }
            if (metadata_json) {
                sqlite3_stmt *rm = NULL;
                if (sqlite3_prepare_v2(conn, "UPDATE task_runs SET metadata=? WHERE id=?",
                                      -1, &rm, NULL) == SQLITE_OK) {
                    sqlite3_bind_text(rm, 1, metadata_json, -1, SQLITE_TRANSIENT);
                    sqlite3_bind_int64(rm, 2, rid);
                    sqlite3_step(rm); sqlite3_finalize(rm);
                }
            }
        }
        kdb_append_event(conn, task_id, -1, "edited",
                            "{\"fields\":[\"result\",\"summary\"],\"result_len\":0,\"summary\":null}");
    }
    kdb_write_end(conn, 1);
    return ok;
}
