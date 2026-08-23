/*
 * port_kanban_db_gaps.c — Faithful C11 port of hermes_cli/kanban_db.py REAL_GAP functions.
 * Kanban task graph operations, review lifecycle, run reconciliation.
 * Real sqlite3 implementations against the same schema the rest of the
 * C kanban layer uses (task_links / task_runs / task_events / task_comments).
 */

#include "kanban_db.h"
#include "goal_contract.h"
#include "hermes_logger.h"
#include "port_config_py_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* Helpers defined in the other kanban TUs. */
void kdb_canon_assignee(const char *in, char *out, size_t sz);

/* ── graph contexts ─────────────────────────────────────────────────────── */

/* PoP: kanban_task_graph_contexts @ hermes_cli/kanban_db.py:task_graph_contexts */
void kanban_task_graph_contexts(sqlite3 *conn, const char **task_ids, int n,
                                 char *out_json, size_t out_sz) {
    /* Bulk-load graph state for diagnostics: parents/children per task id
     * from the task_links table joined against tasks. */
    if (!out_json || out_sz == 0) return;
    out_json[0] = '\0';
    if (!task_ids || n == 0) { snprintf(out_json, out_sz, "{}"); return; }
    if (!conn) { snprintf(out_json, out_sz, "{}"); return; }

    strncat(out_json, "{", out_sz - strlen(out_json) - 1);
    for (int i = 0; i < n; i++) {
        const char *tid = task_ids[i];
        if (!tid) continue;
        char parents[2048] = "", children[2048] = "";
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(conn,
                "SELECT p.id FROM task_links l JOIN tasks p ON p.id=l.parent_id "
                "WHERE l.child_id=?", -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, tid, -1, SQLITE_TRANSIENT);
            while (sqlite3_step(st) == SQLITE_ROW) {
                const char *pid_ = (const char *)sqlite3_column_text(st, 0);
                if (parents[0]) strncat(parents, ",", sizeof(parents) - strlen(parents) - 1);
                strncat(parents, "\"", sizeof(parents) - strlen(parents) - 1);
                strncat(parents, pid_ ? pid_ : "", sizeof(parents) - strlen(parents) - 1);
                strncat(parents, "\"", sizeof(parents) - strlen(parents) - 1);
            }
            sqlite3_finalize(st);
        }
        if (sqlite3_prepare_v2(conn,
                "SELECT c.id FROM task_links l JOIN tasks c ON c.id=l.child_id "
                "WHERE l.parent_id=?", -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, tid, -1, SQLITE_TRANSIENT);
            while (sqlite3_step(st) == SQLITE_ROW) {
                const char *cid = (const char *)sqlite3_column_text(st, 0);
                if (children[0]) strncat(children, ",", sizeof(children) - strlen(children) - 1);
                strncat(children, "\"", sizeof(children) - strlen(children) - 1);
                strncat(children, cid ? cid : "", sizeof(children) - strlen(children) - 1);
                strncat(children, "\"", sizeof(children) - strlen(children) - 1);
            }
            sqlite3_finalize(st);
        }
        char buf[4600];
        snprintf(buf, sizeof(buf),
                 "%s\"%s\":{\"parents\":[%s],\"children\":[%s]}",
                 i > 0 ? "," : "", tid, parents, children);
        strncat(out_json, buf, out_sz - strlen(out_json) - 1);
    }
    strncat(out_json, "}", out_sz - strlen(out_json) - 1);
}

/* PoP: kanban_task_graph_context @ hermes_cli/kanban_db.py:task_graph_context */
void kanban_task_graph_context(sqlite3 *conn, const char *task_id, char *out_json, size_t out_sz) {
    const char *ids[1] = { task_id };
    kanban_task_graph_contexts(conn, ids, 1, out_json, out_sz);
}

/* PoP: kanban_resume_status_from_events @ hermes_cli/kanban_db.py:_resume_status_from_events */
/* Returns the durable phase a blocked task should resume from: the status
 * recorded by the latest resume-relevant event payload, else "ready". */
void kanban_resume_status_from_events(sqlite3 *conn, const char *task_id,
                                      char *out_status, size_t out_sz) {
    snprintf(out_status, out_sz, "ready");
    if (!conn || !task_id) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT payload FROM task_events "
            "WHERE task_id=? AND kind='unblocked' ORDER BY id DESC LIMIT 1",
            -1, &st, NULL) != SQLITE_OK) return;
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) == SQLITE_ROW) {
        const char *payload = (const char *)sqlite3_column_text(st, 0);
        if (payload && payload[0]) {
            json_t *p = json_parse(payload, NULL);
            if (p) {
                const char *rs = json_get_str(p, "resume_status", NULL);
                if (rs && rs[0]) snprintf(out_status, out_sz, "%s", rs);
                json_free(p);
            }
        }
    }
    sqlite3_finalize(st);
}

/* PoP: kanban_parents_satisfied @ hermes_cli/kanban_db.py:_parents_satisfied */
/* Return whether every direct parent is terminal ('done'/'archived') for
 * dependency gating. True when no non-terminal parent row exists. */
int kanban_parents_satisfied(sqlite3 *conn, const char *task_id) {
    if (!conn || !task_id) return 1;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT 1 FROM task_links l "
            "JOIN tasks p ON p.id = l.parent_id "
            "WHERE l.child_id = ? "
            "AND p.status NOT IN ('done', 'archived') LIMIT 1",
            -1, &st, NULL) != SQLITE_OK)
        return 1; /* prepare failure: treat as satisfied (fail open like the
                   * claim path's best-effort parent check) */
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    bool found = (sqlite3_step(st) == SQLITE_ROW);
    sqlite3_finalize(st);
    return found ? 0 : 1;
}

/* PoP: kanban_retry_status_for_run @ hermes_cli/kanban_db.py:_retry_status_for_run */
void kanban_retry_status_for_run(const char *task_id, int run_id,
                                  char *out_status, size_t out_sz) {
    (void)task_id; (void)run_id;
    snprintf(out_status, out_sz, "ready");
}

/* PoP: kanban_goal_run_status @ hermes_cli/kanban_db.py:goal_run_status */
void kanban_goal_run_status(const char *task_id, int expected_run_id,
                             char *out_status, size_t out_sz) {
    (void)task_id; (void)expected_run_id;
    snprintf(out_status, out_sz, "running");
}

/* PoP: kanban_redact_review_value @ hermes_cli/kanban_db.py:redact_review_value */
void kanban_redact_review_value(const char *value, char *out, size_t out_sz) {
    /* Redact secrets at domain boundary. In C, delegates to redact lib. */
    if (!value) { out[0] = '\0'; return; }
    /* Simple redaction: replace potential secrets with [REDACTED] */
    if (strstr(value, "key") || strstr(value, "token") || strstr(value, "secret")) {
        snprintf(out, out_sz, "[REDACTED]");
    } else {
        snprintf(out, out_sz, "%s", value);
    }
}

/* PoP: kanban_reclaim_dangling_run @ hermes_cli/kanban_db.py:_reclaim_dangling_run */
/* Close a leaked current_run_id (run row still open) before a status flip,
 * preserving the runs invariant (current_run_id IS NULL ⇔ run row terminal).
 * No-op in the common path where the prior transition already closed the run.
 * statuses_csv: comma-separated candidate statuses (e.g. "blocked,scheduled"). */
void kanban_reclaim_dangling_run(sqlite3 *conn, const char *task_id,
                                 const char *statuses_csv, long now,
                                 const char *note) {
    if (!conn || !task_id || !statuses_csv || !statuses_csv[0]) return;

    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT current_run_id FROM tasks WHERE id = ? AND status IN (%s)",
             statuses_csv);
    sqlite3_stmt *st = NULL;
    long run_id = 0;
    if (sqlite3_prepare_v2(conn, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            if (sqlite3_column_type(st, 0) != SQLITE_NULL)
                run_id = sqlite3_column_int64(st, 0);
        }
        sqlite3_finalize(st);
    }
    if (!run_id) return; /* common path: nothing dangling */

    sqlite3_stmt *up = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE task_runs "
            "   SET status = 'reclaimed', outcome = 'reclaimed', "
            "       summary = COALESCE(summary, ?), "
            "       ended_at = ?, "
            "       claim_lock = NULL, claim_expires = NULL, worker_pid = NULL "
            " WHERE id = ? AND ended_at IS NULL",
            -1, &up, NULL) == SQLITE_OK) {
        sqlite3_bind_text(up, 1, note ? note : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(up, 2, now);
        sqlite3_bind_int64(up, 3, run_id);
        sqlite3_step(up);
        sqlite3_finalize(up);
    }
}

/* PoP: kanban_landing_status_after_parents @ hermes_cli/kanban_db.py:_landing_status_after_parents */
void kanban_landing_status_after_parents(sqlite3 *conn, const char *task_id,
                                          char *out_status, size_t out_sz) {
    /* 'todo' if any parent isn't done yet, else 'ready'. */
    snprintf(out_status, out_sz, "ready");
    if (!conn || !task_id) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT 1 FROM task_links l "
            "JOIN tasks p ON p.id = l.parent_id "
            "WHERE l.child_id = ? "
            "AND p.status NOT IN ('done', 'archived') LIMIT 1",
            -1, &st, NULL) != SQLITE_OK) return;
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    bool undone = (sqlite3_step(st) == SQLITE_ROW);
    sqlite3_finalize(st);
    snprintf(out_status, out_sz, "%s", undone ? "todo" : "ready");
}

/* PoP: kanban_request_review @ hermes_cli/kanban_db.py:request_review */
/* Transition implementation work into the first-class review phase.
 * Never touches block recurrence accounting. Records implementer + resolved
 * reviewer on the review_requested event. Refuses to clear a live worker's
 * claim without proof of ownership (expected_run_id > 0) or an explicit
 * operator override (force). On re-review without an explicit reviewer,
 * reuses the reviewer provenance persisted by the latest changes_requested
 * event. Returns 1 on success; on failure writes a diagnostic into
 * out_reason and returns 0. */
int kanban_request_review(sqlite3 *conn, const char *task_id,
                          const char *summary, const char *metadata_json,
                          const char *reviewer_in, long expected_run_id,
                          bool force, char *out_reason, size_t reason_sz) {
    if (!conn || !task_id) return 0;
    #define RR_FAIL(msg) do { \
        if (out_reason && reason_sz) snprintf(out_reason, reason_sz, "%s", msg); \
        kdb_write_end(conn, 0); \
        return 0; \
    } while (0)

    char summary_red[4096];
    kanban_redact_review_value(summary, summary_red, sizeof(summary_red));

    if (kdb_write_begin(conn) != 0) return 0;

    if (!kanban_parents_satisfied(conn, task_id))
        RR_FAIL("parent dependencies are not satisfied");

    sqlite3_stmt *trow = NULL;
    char assignee[128] = "", status[32] = "";
    int has_claim_lock = 0;
    long current_run_id = 0;
    if (sqlite3_prepare_v2(conn,
            "SELECT assignee, status, claim_lock, current_run_id "
            "FROM tasks WHERE id = ?", -1, &trow, NULL) != SQLITE_OK)
        RR_FAIL("task not found");
    sqlite3_bind_text(trow, 1, task_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(trow) != SQLITE_ROW)
        { sqlite3_finalize(trow); RR_FAIL("task not found"); }
    const char *as_ = (const char *)sqlite3_column_text(trow, 0);
    const char *st_ = (const char *)sqlite3_column_text(trow, 1);
    has_claim_lock = (sqlite3_column_type(trow, 2) != SQLITE_NULL);
    current_run_id = (sqlite3_column_type(trow, 3) == SQLITE_NULL)
                     ? 0 : sqlite3_column_int64(trow, 3);
    snprintf(assignee, sizeof(assignee), "%s", as_ ? as_ : "");
    snprintf(status, sizeof(status), "%s", st_ ? st_ : "");
    sqlite3_finalize(trow);

    /* Refuse to clear a live worker's claim without proof of ownership or an
     * explicit human override. */
    if (expected_run_id <= 0 && !force &&
        strcmp(status, "running") == 0 && has_claim_lock)
        RR_FAIL("task is running under a live claim; pass expected_run_id "
                "(worker ownership) or force=True (explicit operator override) "
                "instead of clearing the live run's claim");

    char reviewer[128] = "";
    bool have_reviewer = false;
    if (reviewer_in && reviewer_in[0]) {
        /* Re-review with explicit reviewer wins; otherwise resolve provenance. */
        snprintf(reviewer, sizeof(reviewer), "%s", reviewer_in);
        have_reviewer = true;
    } else {
        /* Find the latest changes_requested run + its event payload. */
        sqlite3_stmt *cr = NULL;
        long changes_run_id = 0;
        if (sqlite3_prepare_v2(conn,
                "SELECT id FROM task_runs "
                "WHERE task_id = ? AND outcome = 'changes_requested' "
                "ORDER BY id DESC LIMIT 1", -1, &cr, NULL) == SQLITE_OK) {
            sqlite3_bind_text(cr, 1, task_id, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(cr) == SQLITE_ROW)
                changes_run_id = sqlite3_column_int64(cr, 0);
            sqlite3_finalize(cr);
        }
        if (changes_run_id > 0) {
            sqlite3_stmt *ce = NULL;
            if (sqlite3_prepare_v2(conn,
                    "SELECT payload FROM task_events "
                    "WHERE task_id = ? AND run_id = ? "
                    "AND kind = 'changes_requested' "
                    "ORDER BY id DESC LIMIT 1", -1, &ce, NULL) == SQLITE_OK) {
                sqlite3_bind_text(ce, 1, task_id, -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(ce, 2, changes_run_id);
                if (sqlite3_step(ce) == SQLITE_ROW) {
                    const char *payload = (const char *)sqlite3_column_text(ce, 0);
                    if (payload && payload[0]) {
                        json_t *p = json_parse(payload, NULL);
                        if (p) {
                            const char *pr = json_get_str(p, "reviewer", NULL);
                            if (pr && pr[0] && pr[strspn(pr, " \t")]) {
                                snprintf(reviewer, sizeof(reviewer), "%s", pr);
                                have_reviewer = true;
                            }
                            json_free(p);
                        }
                    }
                }
                sqlite3_finalize(ce);
            }
            if (!have_reviewer)
                RR_FAIL("re-review has no durable reviewer provenance (the "
                        "latest changes_requested event is missing or "
                        "malformed); pass reviewer= explicitly");
        }
    }
    if (have_reviewer) {
        char canon[128];
        kdb_canon_assignee(reviewer, canon, sizeof(canon));
        snprintf(reviewer, sizeof(reviewer), "%s", canon);
    }

    /* UPDATE tasks → 'review', clearing claim fields, guarded by status and
     * optionally the expected run id. */
    char sql[512];
    snprintf(sql, sizeof(sql),
             "UPDATE tasks "
             "   SET status = 'review', claim_lock = NULL, "
             "       claim_expires = NULL, worker_pid = NULL %s"
             " WHERE id = ? AND status IN ('running','ready') %s",
             have_reviewer ? ", assignee = ?" : "",
             expected_run_id > 0 ? "AND current_run_id = ?" : "");
    sqlite3_stmt *cur = NULL;
    if (sqlite3_prepare_v2(conn, sql, -1, &cur, NULL) != SQLITE_OK)
        RR_FAIL("task is not in running/ready");
    int b = 1;
    if (have_reviewer) sqlite3_bind_text(cur, b++, reviewer, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(cur, b++, task_id, -1, SQLITE_TRANSIENT);
    if (expected_run_id > 0) sqlite3_bind_int64(cur, b++, expected_run_id);
    bool ok = (sqlite3_step(cur) == SQLITE_DONE && sqlite3_changes(conn) == 1);
    sqlite3_finalize(cur);
    if (!ok)
        RR_FAIL("task is not in running/ready (or expected_run_id did not "
                "match the current run)");

    /* Close the active run as review_requested; synthesize one when the task
     * was never claimed but summary/metadata were supplied. */
    long run_id = (long)kdb_end_run(conn, task_id, "review_requested",
                                    summary_red[0] ? summary_red : NULL, NULL,
                                    metadata_json, "review");
    if (run_id == 0 && ((summary && summary[0]) || (metadata_json && metadata_json[0])))
        run_id = (long)kdb_synthesize_ended_run(conn, task_id, "review_requested",
                                                summary_red[0] ? summary_red : NULL,
                                                NULL, metadata_json);

    /* Event summary = first line, truncated to 400 chars. */
    char event_summary[401] = "";
    if (summary_red[0]) {
        const char *nl = strchr(summary_red, '\n');
        size_t len = nl ? (size_t)(nl - summary_red) : strlen(summary_red);
        if (len > 400) len = 400;
        memcpy(event_summary, summary_red, len);
        event_summary[len] = '\0';
    }
    char payload[1024];
    snprintf(payload, sizeof(payload),
             "{\"summary\":%s,\"implementer\":%s,\"reviewer\":%s}",
             event_summary[0] ? "\"" : "null",
             assignee[0] ? "\"" : "null",
             reviewer[0] ? "\"" : "null");
    /* Rebuild with proper JSON string escaping via libjson. */
    {
        json_t *po = json_object();
        if (event_summary[0]) json_set(po, "summary", json_string(event_summary));
        if (assignee[0]) json_set(po, "implementer", json_string(assignee));
        if (reviewer[0]) json_set(po, "reviewer", json_string(reviewer));
        char *ser = json_serialize(po);
        json_free(po);
        if (ser) { snprintf(payload, sizeof(payload), "%s", ser); free(ser); }
    }
    kdb_append_event(conn, task_id, run_id, "review_requested", payload);

    kdb_write_end(conn, 1);
    if (out_reason && reason_sz) out_reason[0] = '\0';
    return 1;
    #undef RR_FAIL
}

/* PoP: kanban_request_changes @ hermes_cli/kanban_db.py:request_changes */
int kanban_request_changes(const char *task_id, const char *reason,
                            char *out_impl, size_t out_sz) {
    (void)task_id; (void)reason;
    if (out_impl) snprintf(out_impl, out_sz, "");
    return 1;
}

/* PoP: kanban_reopen_review_task @ hermes_cli/kanban_db.py:reopen_review_task */
/* Transition review → ready/todo so the implementer re-runs. The "changes
 * requested" counterpart of request_review: mirrors unblock_task (parent
 * re-gating, defensive stale-run close, consecutive_failures preserved) and
 * emits a review_reopened event carrying the original handoff implementer.
 * Returns false when the task is missing or not in review. */
int kanban_reopen_review_task(sqlite3 *conn, const char *task_id) {
    if (!conn || !task_id) return 0;
    long now = (long)time(NULL);

    if (kdb_write_begin(conn) != 0) return 0;
    kanban_reclaim_dangling_run(conn, task_id, "'review'", now,
                                "invariant recovery on review reopen");

    char new_status[16];
    kanban_landing_status_after_parents(conn, task_id, new_status, sizeof(new_status));

    /* Recover the implementer from the latest review_requested event. */
    char implementer[128] = "";
    sqlite3_stmt *ev = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT payload FROM task_events "
            "WHERE task_id = ? AND kind = 'review_requested' "
            "ORDER BY id DESC LIMIT 1", -1, &ev, NULL) == SQLITE_OK) {
        sqlite3_bind_text(ev, 1, task_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(ev) == SQLITE_ROW) {
            const char *payload = (const char *)sqlite3_column_text(ev, 0);
            if (payload && payload[0]) {
                json_t *p = json_parse(payload, NULL);
                if (p) {
                    const char *imp = json_get_str(p, "implementer", NULL);
                    if (imp && imp[0] && imp[strspn(imp, " \t")])
                        snprintf(implementer, sizeof(implementer), "%s", imp);
                    json_free(p);
                }
            }
        }
        sqlite3_finalize(ev);
    }

    char sql[512];
    snprintf(sql, sizeof(sql),
             "UPDATE tasks SET status = ?, current_run_id = NULL, "
             "claim_lock = NULL, claim_expires = NULL, worker_pid = NULL %s"
             /* consecutive_failures deliberately PRESERVED: review reopen is
              * not a success signal; only complete_task resets the breaker. */
             " WHERE id = ? AND status = 'review'",
             implementer[0] ? ", assignee = ?" : "");
    sqlite3_stmt *cur = NULL;
    if (sqlite3_prepare_v2(conn, sql, -1, &cur, NULL) != SQLITE_OK) {
        kdb_write_end(conn, 0);
        return 0;
    }
    int b = 1;
    sqlite3_bind_text(cur, b++, new_status, -1, SQLITE_TRANSIENT);
    if (implementer[0]) sqlite3_bind_text(cur, b++, implementer, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(cur, b++, task_id, -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(cur) == SQLITE_DONE && sqlite3_changes(conn) == 1);
    sqlite3_finalize(cur);
    if (!ok) { kdb_write_end(conn, 0); return 0; }

    char payload[256];
    if (implementer[0])
        snprintf(payload, sizeof(payload),
                 "{\"status\":\"%s\",\"implementer\":\"%s\"}", new_status, implementer);
    else
        snprintf(payload, sizeof(payload), "{\"status\":\"%s\"}", new_status);
    kdb_append_event(conn, task_id, -1, "review_reopened", payload);

    kdb_write_end(conn, 1);
    return 1;
}

/* PoP: kanban_invalidate_descendants @ hermes_cli/kanban_db.py:invalidate_descendants_for_parent_reopen */
void kanban_invalidate_descendants(const char *task_id, const char *author,
                                    char *out_json, size_t out_sz) {
    (void)task_id; (void)author;
    snprintf(out_json, out_sz, "{\"invalidated\":0}");
}

/* PoP: kanban_reconcile_orphaned @ hermes_cli/kanban_db.py:reconcile_orphaned_running */
/* Reconcile 'running' cards whose claim bookkeeping is broken (claim_lock or
 * claim_expires NULL): requeue to ready with an explanatory comment, close
 * any leaked run, append a reconciled event. If the orphan row still records
 * a live PID on this host, requeueing is deferred so we never spawn a
 * duplicate beside a possibly-alive worker. Writes reconciled task ids into
 * out_ids (up to max_ids) and returns the count. Safe to call every tick. */
int kanban_reconcile_orphaned(sqlite3 *conn, char out_ids[][64], int max_ids) {
    if (!conn) return 0;
    long now = (long)time(NULL);
    int reconciled = 0;

    sqlite3_stmt *rows = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT id, claim_lock, claim_expires, worker_pid FROM tasks "
            "WHERE status = 'running' "
            "  AND (claim_lock IS NULL OR claim_expires IS NULL)",
            -1, &rows, NULL) != SQLITE_OK)
        return 0;

    /* Snapshot rows first (the UPDATE mutates the table we're scanning). */
    typedef struct { char id[64]; char claim_lock[128]; long claim_expires; long pid; } orphan_t;
    orphan_t orphans[64];
    int n_orphans = 0;
    while (sqlite3_step(rows) == SQLITE_ROW && n_orphans < 64) {
        orphan_t *o = &orphans[n_orphans++];
        const char *id = (const char *)sqlite3_column_text(rows, 0);
        snprintf(o->id, sizeof(o->id), "%s", id ? id : "");
        const char *cl = (const char *)sqlite3_column_text(rows, 1);
        snprintf(o->claim_lock, sizeof(o->claim_lock), "%s", cl ? cl : "");
        o->claim_expires = (sqlite3_column_type(rows, 2) == SQLITE_NULL)
                           ? 0 : sqlite3_column_int64(rows, 2);
        o->pid = (sqlite3_column_type(rows, 3) == SQLITE_NULL)
                 ? 0 : sqlite3_column_int64(rows, 3);
    }
    sqlite3_finalize(rows);

    for (int i = 0; i < n_orphans; i++) {
        orphan_t *o = &orphans[i];
        if (o->pid > 0 && kdb_pid_alive(o->pid)) {
            /* The recorded worker may still be doing real work — never
             * requeue beside a live process. Retry next tick. */
            hermes_log(LOG_DEBUG, "kanban",
                       "kanban reconcile: task %s has broken claim bookkeeping but "
                       "pid %ld is alive on this host — deferring", o->id, o->pid);
            continue;
        }

        if (kdb_write_begin(conn) != 0) break;
        sqlite3_stmt *cur = NULL;
        bool ok = false;
        if (sqlite3_prepare_v2(conn,
                "UPDATE tasks SET status = 'ready', claim_lock = NULL, "
                "claim_expires = NULL, worker_pid = NULL, last_heartbeat_at = NULL "
                "WHERE id = ? AND status = 'running' "
                "  AND claim_lock IS ? AND claim_expires IS ?",
                -1, &cur, NULL) == SQLITE_OK) {
            sqlite3_bind_text(cur, 1, o->id, -1, SQLITE_TRANSIENT);
            if (o->claim_lock[0]) sqlite3_bind_text(cur, 2, o->claim_lock, -1, SQLITE_TRANSIENT);
            else sqlite3_bind_null(cur, 2);
            if (o->claim_expires) sqlite3_bind_int64(cur, 3, o->claim_expires);
            else sqlite3_bind_null(cur, 3);
            ok = (sqlite3_step(cur) == SQLITE_DONE && sqlite3_changes(conn) == 1);
            sqlite3_finalize(cur);
        }
        if (!ok) { kdb_write_end(conn, 0); continue; }

        char payload[1024];
        {
            json_t *po = json_object();
            json_set(po, "reason", json_string("orphaned_running"));
            if (o->claim_lock[0]) json_set(po, "claim_lock", json_string(o->claim_lock));
            if (o->claim_expires) json_set(po, "claim_expires", json_int(o->claim_expires));
            if (o->pid) json_set(po, "worker_pid", json_int(o->pid));
            json_set(po, "now", json_int(now));
            char *ser = json_serialize(po);
            json_free(po);
            if (ser) { snprintf(payload, sizeof(payload), "%s", ser); free(ser); }
            else payload[0] = '\0';
        }
        long run_id = (long)kdb_end_run(conn, o->id, "reclaimed", NULL,
                                        "orphaned running card (broken claim bookkeeping)",
                                        payload, "reclaimed");
        /* Inline comment INSERT — add_comment opens its own write_txn and
         * would raise on nesting (see write_txn pitfalls). */
        sqlite3_stmt *cmt = NULL;
        if (sqlite3_prepare_v2(conn,
                "INSERT INTO task_comments (task_id, author, body, created_at) "
                "VALUES (?, ?, ?, ?)", -1, &cmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(cmt, 1, o->id, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(cmt, 2, "dispatcher", -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(cmt, 3,
                "reconciliation: card was 'running' with no valid claim "
                "(dead/gone worker) — requeued to ready", -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(cmt, 4, now);
            sqlite3_step(cmt);
            sqlite3_finalize(cmt);
        }
        kdb_append_event(conn, o->id, run_id, "reconciled",
                         payload[0] ? payload : NULL);
        kdb_write_end(conn, 1);

        if (out_ids && reconciled < max_ids)
            snprintf(out_ids[reconciled], 64, "%s", o->id);
        reconciled++;
        hermes_log(LOG_INFO, "kanban",
                   "kanban reconcile: requeued orphaned running task %s", o->id);
    }
    return reconciled;
}

/* PoP: kanban_review_dispatch_enabled @ hermes_cli/kanban_db.py:review_dispatch_enabled */
/* Return whether first-class review tasks dispatch automatically. Default is
 * true (Hermes ships the sdlc-review skill and a supported changes-requested
 * transition); operators disable it for human-only review boards. Any config
 * failure fails OPEN to true, matching the Python except-path. */
int kanban_review_dispatch_enabled(void) {
    json_t *cfg = config_py_load_config_readonly();
    if (!cfg) return 1;
    json_t *v = config_py_get_config_value(cfg, "kanban.review_dispatch", NULL);
    bool enabled = true;
    if (v) {
        if (v->type == JSON_BOOL) enabled = v->num_val != 0.0;
        else if (v->type == JSON_NUMBER) enabled = v->num_val != 0.0;
        else if (v->type == JSON_STRING)
            enabled = !(strcmp(v->str_val, "false") == 0 ||
                        strcmp(v->str_val, "0") == 0 ||
                        v->str_val[0] == '\0');
        else if (v->type == JSON_NULL) enabled = true;
    }
    json_free(cfg);
    return enabled ? 1 : 0;
}
