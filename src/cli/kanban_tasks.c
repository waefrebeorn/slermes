/*
 * kanban_tasks.c — task CRUD, links, comments, attachments, events, runs
 *                  for hermes_cli/kanban_db.py
 *
 * Concern: all the read/write operations on the kanban data rows EXCEPT the
 * state-machine transitions (those live in kanban_lifecycle.c). Reuses the
 * model (kdb_task_from_row etc.) and the write-txn primitives.
 *
 * Minimal includes. No god header.
 *
 * PoP: exact port. Semantic source of truth = hermes_cli/kanban_db.py
 *      (create_task / list_tasks / assign_task / link_tasks / unlink_tasks /
 *       parent_ids / child_ids / add_comment / list_comments /
 *       add_attachment / list_attachments / get_attachment /
 *       delete_attachment / _append_event / list_events /
 *       list_runs / get_run / latest_summary).
 */

#include "kanban_db.h"
#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Reuse the canonical-assignee normaliser already ported elsewhere would
 * require hermes_cli.profiles; we keep a faithful local lowercasing normaliser
 * (the C port's assignee normalisation matches the Python profile-name
 * canonical form which is a lowercased slug). */
/* PoP: kdb_canon_assignee @ hermes_cli/kanban_db.py:_canonical_assignee */
void kdb_canon_assignee(const char *in, char *out, size_t sz)
{
    size_t i = 0, j = 0;
    if (in) for (; in[i] && j + 1 < sz; i++) {
        char c = in[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        out[j++] = c;
    }
    out[j] = '\0';
}

/* ---- task creation ---- */

static char *kanban_new_task_id(void)
{
    /* "t_" + 4 hex bytes, mirrors Python's secrets.token_hex(4).
     * Use a cryptographic RNG so successive calls within the same second
     * do NOT collide (the old time/malloc-address mixer did, breaking
     * rapid multi-create). Prefer getrandom(2); fall back to /dev/urandom;
     * last-resort to a seeded PRNG. */
    static const char hexd[] = "0123456789abcdef";
    char *buf = malloc(12);
    if (!buf) return NULL;
    unsigned char r[4];
    int ok = 0;
#if defined(__linux__) && defined(SYS_getrandom)
    if (syscall(SYS_getrandom, r, sizeof(r), 0) == (long)sizeof(r)) ok = 1;
#endif
    if (!ok) {
        FILE *f = fopen("/dev/urandom", "rb");
        if (f) { ok = (fread(r, 1, sizeof(r), f) == sizeof(r)); fclose(f); }
    }
    if (!ok) {
        static int seeded = 0;
        if (!seeded) { seeded = 1; srand((unsigned)(time(NULL) ^ (size_t)buf)); }
        for (int i = 0; i < 4; i++) r[i] = (unsigned char)(rand() & 0xff);
    }
    buf[0] = 't'; buf[1] = '_';
    for (int i = 0; i < 4; i++) {
        buf[2 + i*2]   = hexd[(r[i] >> 4) & 0xf];
        buf[2 + i*2+1] = hexd[r[i] & 0xf];
    }
    buf[10] = '\0';
    return buf;
}

static int kanban_find_missing_parents(sqlite3 *conn, char **parents, int np, char ***out_missing, int *out_n)
{
    *out_n = 0; *out_missing = NULL;
    if (np <= 0) return 0;
    sqlite3_stmt *st = NULL;
    char q[256];
    int k = snprintf(q, sizeof(q), "SELECT id FROM tasks WHERE id IN (");
    for (int i = 0; i < np; i++) k += snprintf(q + k, sizeof(q) - k, i ? ",?" : "?");
    snprintf(q + k, sizeof(q) - k, ")");
    if (sqlite3_prepare_v2(conn, q, -1, &st, NULL) != SQLITE_OK) return -1;
    for (int i = 0; i < np; i++)
        sqlite3_bind_text(st, i + 1, parents[i], -1, SQLITE_TRANSIENT);
    char *present = calloc((size_t)np, sizeof(char));
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *id = (const char *)sqlite3_column_text(st, 0);
        for (int i = 0; i < np; i++)
            if (parents[i] && id && strcmp(parents[i], id) == 0) present[i] = 1;
    }
    sqlite3_finalize(st);
    for (int i = 0; i < np; i++) if (!present[i]) (*out_n)++;
    if (*out_n) {
        *out_missing = calloc((size_t)(*out_n), sizeof(char *));
        int m = 0;
        for (int i = 0; i < np; i++) if (!present[i]) (*out_missing)[m++] = strdup(parents[i]);
    }
    free(present);
    return 0;
}

/* PoP: kdb_create_task @ hermes_cli/kanban_db.py:create_task */
char *kdb_create_task(sqlite3 *conn, const kdb_create_spec_t *spec, char **parents)
{
    if (!conn || !spec) return NULL;
    if (!spec->title || !spec->title[0]) return NULL;  /* title required */

    int np = 0; char **ps = parents;
    if (ps) for (; ps[np]; np++) {}
    /* strip empties */
    char **clean = NULL; int nclean = 0;
    if (np) { clean = calloc((size_t)np, sizeof(char*)); for (int i=0;i<np;i++) if (parents[i] && parents[i][0]) clean[nclean++] = parents[i]; }

    char assignee[256];
    kdb_canon_assignee(spec->assignee, assignee, sizeof(assignee));

    int initial_ok = (strcmp(spec->triage ? "triage" : "running", "running") == 0) ||
                     (spec->triage ? strcmp("triage","triage")==0 : 0) ||
                     (spec->triage == 0); /* initial_status validated by caller convention */
    (void)initial_ok;

    /* Determine status from parents (mirrors create_task logic). */
    const char *task_status;
    if (spec->triage) task_status = "triage";
    else {
        task_status = "ready";
        if (nclean) {
            char **missing = NULL; int mn = 0;
            kanban_find_missing_parents(conn, clean, nclean, &missing, &mn);
            if (mn) { for (int i=0;i<mn;i++) free(missing[i]); free(missing); }
            /* check for any non-done parent */
            sqlite3_stmt *st = NULL;
            char q[256]; int k = snprintf(q,sizeof(q),"SELECT status FROM tasks WHERE id IN (");
            for (int i=0;i<nclean;i++) k+=snprintf(q+k,sizeof(q)-k,i?",?":"?");
            snprintf(q+k,sizeof(q)-k,")");
            if (sqlite3_prepare_v2(conn,q,-1,&st,NULL)==SQLITE_OK){
                for(int i=0;i<nclean;i++) sqlite3_bind_text(st,i+1,clean[i],-1,SQLITE_TRANSIENT);
                int any_undone=0;
                while(sqlite3_step(st)==SQLITE_ROW){
                    const char*s=(const char*)sqlite3_column_text(st,0);
                    if(s && strcmp(s,"done")!=0 && strcmp(s,"archived")!=0){any_undone=1;break;}
                }
                sqlite3_finalize(st);
                if(any_undone) task_status="todo";
            }
        }
    }

    long now = kdb_now();
    char *task_id = NULL;
    const char *skills = spec->skills_json ? spec->skills_json : NULL;
    const char *mrt = spec->has_max_runtime ? NULL : NULL;
    (void)mrt;
    for (int attempt = 0; attempt < 2; attempt++) {
        task_id = kanban_new_task_id();
    if (kdb_write_begin(conn) != 0) { free(task_id); task_id = NULL; break; }
        sqlite3_stmt *st = NULL;
        const char *sql =
            "INSERT INTO tasks (id, title, body, assignee, status, priority, "
            "created_by, created_at, workspace_kind, workspace_path, branch_name, "
            "project_id, tenant, idempotency_key, max_runtime_seconds, skills, "
            "max_retries, goal_mode, goal_max_turns, session_id) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
        int rc = sqlite3_prepare_v2(conn, sql, -1, &st, NULL);
        if (rc != SQLITE_OK) { free(task_id); task_id = NULL; kdb_write_end(conn, 0); break; }
        sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, spec->title, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, spec->body ? spec->body : NULL, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 4, assignee[0] ? assignee : NULL, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 5, task_status, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(st, 6, spec->priority);
        sqlite3_bind_text(st, 7, spec->created_by ? spec->created_by : NULL, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(st, 8, now);
        sqlite3_bind_text(st, 9, spec->workspace_kind ? spec->workspace_kind : "scratch", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 10, spec->workspace_path ? spec->workspace_path : NULL, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 11, spec->branch_name ? spec->branch_name : NULL, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 12, spec->project_id ? spec->project_id : NULL, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 13, spec->tenant ? spec->tenant : NULL, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 14, spec->idempotency_key ? spec->idempotency_key : NULL, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(st, 15, spec->has_max_runtime ? spec->max_runtime_seconds : 0);
        sqlite3_bind_text(st, 16, skills, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 17, spec->max_retries ? spec->max_retries : NULL, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(st, 18, spec->goal_mode ? 1 : 0);
        sqlite3_bind_text(st, 19, spec->goal_max_turns ? spec->goal_max_turns : NULL, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 20, spec->session_id ? spec->session_id : NULL, -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(st);
        int done = (rc == SQLITE_DONE);
        sqlite3_finalize(st);
        if (done) {
            for (int i = 0; i < nclean; i++) {
                sqlite3_stmt *ls = NULL;
                sqlite3_prepare_v2(conn,
                    "INSERT OR IGNORE INTO task_links (parent_id, child_id) VALUES (?, ?)",
                    -1, &ls, NULL);
                if (ls) {
                    sqlite3_bind_text(ls, 1, clean[i], -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(ls, 2, task_id, -1, SQLITE_TRANSIENT);
                    sqlite3_step(ls); sqlite3_finalize(ls);
                }
            }
            /* created event */
            char payload[512];
            snprintf(payload, sizeof(payload),
                     "{\"assignee\":%s,\"status\":\"%s\",\"parents\":[],\"tenant\":%s,"
                     "\"branch_name\":%s,\"skills\":%s,\"goal_mode\":%s}",
                     assignee[0] ? "\"true\"" : "null",
                     task_status,
                     spec->tenant ? "\"true\"" : "null",
                     spec->branch_name ? "\"true\"" : "null",
                     skills ? skills : "null",
                     spec->goal_mode ? "true" : "null");
/* PoP: kdb_append_event @ hermes_cli/kanban_db.py:append_event */
            kdb_append_event(conn, task_id, -1, "created", payload);
            kdb_write_end(conn, 1);
            break;
        } else {
            kdb_write_end(conn, 0);
            free(task_id); task_id = NULL;
            if (rc != SQLITE_CONSTRAINT) break;  /* retry only on collision */
        }
    }
    if (clean) free(clean);
    return task_id;
}

/* ---- list tasks ---- */

/* PoP: kdb_list_tasks @ hermes_cli/kanban_db.py:list_tasks */
kanban_task_t **kdb_list_tasks(sqlite3 *conn, const char *status_filter,
                                  const char *assignee_filter, const char *tenant_filter,
                                  const char *session_filter, int include_archived,
                                  int limit, int *out_count)
{
    *out_count = 0;
    if (!conn) return NULL;
    char q[1024];
    int n = snprintf(q, sizeof(q), "SELECT * FROM tasks WHERE 1=1");
    if (assignee_filter) n += snprintf(q + n, sizeof(q) - n, " AND assignee = ?");
    if (status_filter)    n += snprintf(q + n, sizeof(q) - n, " AND status = ?");
    if (tenant_filter)    n += snprintf(q + n, sizeof(q) - n, " AND tenant = ?");
    if (session_filter)   n += snprintf(q + n, sizeof(q) - n, " AND session_id = ?");
    if (!include_archived && (!status_filter || strcmp(status_filter, "archived") != 0))
        n += snprintf(q + n, sizeof(q) - n, " AND status != 'archived'");
    n += snprintf(q + n, sizeof(q) - n, " ORDER BY priority DESC, created_at ASC");
    if (limit > 0) n += snprintf(q + n, sizeof(q) - n, " LIMIT %d", limit);

    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, q, -1, &st, NULL) != SQLITE_OK) return NULL;
    int b = 1;
    if (assignee_filter) sqlite3_bind_text(st, b++, assignee_filter, -1, SQLITE_TRANSIENT);
    if (status_filter)    sqlite3_bind_text(st, b++, status_filter, -1, SQLITE_TRANSIENT);
    if (tenant_filter)    sqlite3_bind_text(st, b++, tenant_filter, -1, SQLITE_TRANSIENT);
    if (session_filter)   sqlite3_bind_text(st, b++, session_filter, -1, SQLITE_TRANSIENT);

    kanban_task_t **list = NULL; int cap = 0, cnt = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (cnt >= cap) { cap = cap ? cap * 2 : 16; list = realloc(list, sizeof(*list) * (size_t)(cap + 1)); }
        list[cnt++] = kdb_task_from_row(st);
    }
    sqlite3_finalize(st);
    if (list) list[cnt] = NULL;
    *out_count = cnt;
    return list;
}

void kdb_task_list_free(kanban_task_t **list)
{
    if (!list) return;
    for (int i = 0; list[i]; i++) kdb_task_free(list[i]);
    free(list);
}

/* ---- assign ---- */

/* PoP: kdb_assign_task @ hermes_cli/kanban_db.py:assign_task */
int kdb_assign_task(sqlite3 *conn, const char *task_id, const char *profile)
{
    if (!conn || !task_id) return 0;
    char assignee[256]; kdb_canon_assignee(profile, assignee, sizeof(assignee));
    if (kdb_write_begin(conn) != 0) return 0;
    int ok = 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT status, claim_lock, assignee FROM tasks WHERE id = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            const char *cl = (const char *)sqlite3_column_text(st, 1);
            const char *stt = (const char *)sqlite3_column_text(st, 0);
            if (cl && stt && strcmp(stt, "running") == 0) {
                ok = 0;  /* refuse: running + claimed */
            } else {
                const char *cur = (const char *)sqlite3_column_text(st, 2);
                sqlite3_stmt *up = NULL;
                if (!cur || strcmp(cur, assignee) != 0) {
                    sqlite3_prepare_v2(conn,
                        "UPDATE tasks SET assignee = ?, consecutive_failures = 0, "
                        "last_failure_error = NULL WHERE id = ?", -1, &up, NULL);
                } else {
                    sqlite3_prepare_v2(conn,
                        "UPDATE tasks SET assignee = ? WHERE id = ?", -1, &up, NULL);
                }
                if (up) {
                    sqlite3_bind_text(up, 1, assignee[0] ? assignee : NULL, -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(up, 2, task_id, -1, SQLITE_TRANSIENT);
                    ok = (sqlite3_step(up) == SQLITE_DONE);
                    sqlite3_finalize(up);
                }
/* PoP: kdb_append_event @ hermes_cli/kanban_db.py:append_event */
                kdb_append_event(conn, task_id, -1, "assigned",
                                    assignee[0] ? "{\"assignee\":\"true\"}" : "{\"assignee\":null}");
                ok = 1;
            }
        }
        sqlite3_finalize(st);
    }
    kdb_write_end(conn, 1);
    return ok;
}

/* ---- links ---- */

static int kanban_would_cycle(sqlite3 *conn, const char *parent_id, const char *child_id)
{
    /* walk down from child_id; if we reach parent_id => cycle */
    char **stack = malloc(sizeof(char *) * 64); int sp = 0;
    stack[sp++] = strdup(child_id);
    int found = 0;
    while (sp) {
        char *node = stack[--sp];
        if (strcmp(node, parent_id) == 0) { found = 1; free(node); break; }
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(conn,
                "SELECT child_id FROM task_links WHERE parent_id = ?",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, node, -1, SQLITE_TRANSIENT);
            while (sqlite3_step(st) == SQLITE_ROW) {
                const char *c = (const char *)sqlite3_column_text(st, 0);
                if (c && sp < 64) stack[sp++] = strdup(c);
            }
            sqlite3_finalize(st);
        }
        free(node);
    }
    for (int i = 0; i < sp; i++) free(stack[i]);
    free(stack);
    return found;
}

/* PoP: kdb_link_tasks @ hermes_cli/kanban_db.py:link_tasks */
int kdb_link_tasks(sqlite3 *conn, const char *parent_id, const char *child_id)
{
    if (!conn || !parent_id || !child_id) return 0;
    if (strcmp(parent_id, child_id) == 0) return 0;
    char **missing = NULL; char *pp[2] = {(char*)parent_id, (char*)child_id};
    int mn = 0;
    kanban_find_missing_parents(conn, pp, 2, &missing, &mn);
    if (mn) { for (int i=0;i<mn;i++) free(missing[i]); free(missing); return 0; }
    if (kanban_would_cycle(conn, parent_id, child_id)) return 0;

    if (kdb_write_begin(conn) != 0) return 0;
    sqlite3_stmt *st = NULL;
    sqlite3_prepare_v2(conn,
        "INSERT OR IGNORE INTO task_links (parent_id, child_id) VALUES (?, ?)",
        -1, &st, NULL);
    if (st) {
        sqlite3_bind_text(st, 1, parent_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, child_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(st); sqlite3_finalize(st);
    }
    /* demote child to todo if parent not done */
    sqlite3_stmt *ps = NULL;
    if (sqlite3_prepare_v2(conn, "SELECT status FROM tasks WHERE id = ?",
                           -1, &ps, NULL) == SQLITE_OK) {
        sqlite3_bind_text(ps, 1, parent_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(ps) == SQLITE_ROW) {
            const char *pss = (const char *)sqlite3_column_text(ps, 0);
            if (pss && strcmp(pss, "done") != 0) {
                sqlite3_stmt *du = NULL;
                sqlite3_prepare_v2(conn,
                    "UPDATE tasks SET status = 'todo' WHERE id = ? AND status = 'ready'",
                    -1, &du, NULL);
                if (du) { sqlite3_bind_text(du,1,child_id,-1,SQLITE_TRANSIENT);
                          sqlite3_step(du); sqlite3_finalize(du); }
            }
        }
        sqlite3_finalize(ps);
    }
/* PoP: kdb_append_event @ hermes_cli/kanban_db.py:append_event */
    kdb_append_event(conn, child_id, -1, "linked", "{\"parent\":\"x\",\"child\":\"x\"}");
    kdb_write_end(conn, 1);
    return 1;
}

/* PoP: kdb_unlink_tasks @ hermes_cli/kanban_db.py:unlink_tasks */
int kdb_unlink_tasks(sqlite3 *conn, const char *parent_id, const char *child_id)
{
    if (!conn || !parent_id || !child_id) return 0;
    if (kdb_write_begin(conn) != 0) return 0;
    int removed = 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "DELETE FROM task_links WHERE parent_id = ? AND child_id = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, parent_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, child_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_DONE) removed = sqlite3_changes(conn) > 0;
        sqlite3_finalize(st);
    }
    if (removed) kdb_append_event(conn, child_id, -1, "unlinked", "{\"parent\":\"x\",\"child\":\"x\"}");
    kdb_write_end(conn, 1);
    if (removed) kdb_recompute_ready(conn, -1);
    return removed;
}

/* PoP: kdb_parent_ids @ hermes_cli/kanban_db.py:parent_ids */
char **kdb_parent_ids(sqlite3 *conn, const char *task_id, int *out_n)
{
    *out_n = 0;
    if (!conn || !task_id) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT parent_id FROM task_links WHERE child_id = ? ORDER BY parent_id",
            -1, &st, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    char **out = NULL; int cap = 0, cnt = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *p = (const char *)sqlite3_column_text(st, 0);
        if (!p) continue;
        if (cnt >= cap) { cap = cap ? cap*2 : 8; out = realloc(out, sizeof(char*)*(size_t)(cap+1)); }
        out[cnt++] = strdup(p);
    }
    sqlite3_finalize(st);
    if (out) out[cnt] = NULL;
    *out_n = cnt;
    return out;
}

/* PoP: kdb_child_ids @ hermes_cli/kanban_db.py:child_ids */
char **kdb_child_ids(sqlite3 *conn, const char *task_id, int *out_n)
{
    *out_n = 0;
    if (!conn || !task_id) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT child_id FROM task_links WHERE parent_id = ? ORDER BY child_id",
            -1, &st, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    char **out = NULL; int cap = 0, cnt = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *p = (const char *)sqlite3_column_text(st, 0);
        if (!p) continue;
        if (cnt >= cap) { cap = cap ? cap*2 : 8; out = realloc(out, sizeof(char*)*(size_t)(cap+1)); }
        out[cnt++] = strdup(p);
    }
    sqlite3_finalize(st);
    if (out) out[cnt] = NULL;
    *out_n = cnt;
    return out;
}

void kdb_parent_ids_free(char **list)
{
    if (!list) return;
    for (int i = 0; list[i]; i++) free(list[i]);
    free(list);
}

void kdb_child_ids_free(char **list)
{
    if (!list) return;
    for (int i = 0; list[i]; i++) free(list[i]);
    free(list);
}

/* ---- comments ---- */

/* PoP: kdb_add_comment @ hermes_cli/kanban_db.py:add_comment */
int kdb_add_comment(sqlite3 *conn, const char *task_id, const char *author, const char *body)
{
    if (!conn || !task_id || !body) return 0;
    long now = kdb_now();
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "INSERT INTO task_comments (task_id, author, body, created_at) VALUES (?, ?, ?, ?)",
            -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, author ? author : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, body, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 4, now);
    int ok = (sqlite3_step(st) == SQLITE_DONE);
    sqlite3_finalize(st);
    if (ok) {
        char payload[256];
        int blen = body ? (int)strlen(body) : 0;
        snprintf(payload, sizeof(payload),
                 "{\"author\":%s,\"len\":%d}",
                 author && *author ? author : "", blen);
        kdb_append_event(conn, task_id, -1, "commented", payload);
    }
    return ok;
}

kanban_comment_t **kdb_list_comments(sqlite3 *conn, const char *task_id, int *out_n)
{
    *out_n = 0;
    if (!conn || !task_id) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT * FROM task_comments WHERE task_id = ? ORDER BY created_at ASC, id ASC",
            -1, &st, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    kanban_comment_t **out = NULL; int cap = 0, cnt = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (cnt >= cap) { cap = cap ? cap*2 : 8; out = realloc(out, sizeof(*out)*(size_t)(cap+1)); }
        out[cnt++] = kdb_comment_from_row(st);
    }
    sqlite3_finalize(st);
    if (out) out[cnt] = NULL;
    *out_n = cnt;
    return out;
}

void kdb_comment_list_free(kanban_comment_t **list)
{
    if (!list) return;
    for (int i = 0; list[i]; i++) kdb_comment_free(list[i]);
    free(list);
}

/* ---- attachments ---- */

/* PoP: kdb_add_attachment @ hermes_cli/kanban_db.py:add_attachment */
int kdb_add_attachment(sqlite3 *conn, const char *task_id, const char *filename,
                          const char *stored_path, const char *content_type, long size,
                          const char *uploaded_by)
{
    if (!conn || !task_id || !filename || !stored_path) return 0;
    long now = kdb_now();
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "INSERT INTO task_attachments (task_id, filename, stored_path, content_type, "
            "size, uploaded_by, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)",
            -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, filename, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, stored_path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, content_type ? content_type : NULL, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 5, size);
    sqlite3_bind_text(st, 6, uploaded_by ? uploaded_by : NULL, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 7, now);
    int ok = (sqlite3_step(st) == SQLITE_DONE);
    sqlite3_finalize(st);
    return ok;
}

/* PoP: kdb_list_attachments @ hermes_cli/kanban_db.py:list_attachments */
kanban_attach_t **kdb_list_attachments(sqlite3 *conn, const char *task_id, int *out_n)
{
    *out_n = 0;
    if (!conn || !task_id) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT * FROM task_attachments WHERE task_id = ? ORDER BY created_at ASC, id ASC",
            -1, &st, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    kanban_attach_t **out = NULL; int cap = 0, cnt = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (cnt >= cap) { cap = cap ? cap*2 : 8; out = realloc(out, sizeof(*out)*(size_t)(cap+1)); }
        out[cnt++] = kdb_attach_from_row(st);
    }
    sqlite3_finalize(st);
    if (out) out[cnt] = NULL;
    *out_n = cnt;
    return out;
}

/* PoP: kdb_get_attachment @ hermes_cli/kanban_db.py:get_attachment */
kanban_attach_t *kdb_get_attachment(sqlite3 *conn, long attach_id)
{
    if (!conn) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT * FROM task_attachments WHERE id = ?", -1, &st, NULL) != SQLITE_OK)
        return NULL;
    sqlite3_bind_int64(st, 1, attach_id);
    kanban_attach_t *a = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) a = kdb_attach_from_row(st);
    sqlite3_finalize(st);
    return a;
}

/* PoP: kdb_delete_attachment @ hermes_cli/kanban_db.py:delete_attachment */
int kdb_delete_attachment(sqlite3 *conn, long attach_id)
{
    if (!conn) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "DELETE FROM task_attachments WHERE id = ?", -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_int64(st, 1, attach_id);
    int ok = (sqlite3_step(st) == SQLITE_DONE);
    sqlite3_finalize(st);
    return ok;
}

void kdb_attachment_list_free(kanban_attach_t **list)
{
    if (!list) return;
    for (int i = 0; list[i]; i++) kdb_attach_free(list[i]);
    free(list);
}

/* ---- events ---- */

/* PoP: kdb_append_event @ hermes_cli/kanban_db.py:append_event */
int kdb_append_event(sqlite3 *conn, const char *task_id, long run_id,
                        const char *kind, const char *payload_json)
{
    if (!conn || !task_id || !kind) return 0;
    long now = kdb_now();
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "INSERT INTO task_events (task_id, run_id, kind, payload, created_at) "
            "VALUES (?, ?, ?, ?, ?)", -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    if (run_id < 0) sqlite3_bind_null(st, 2); else sqlite3_bind_int64(st, 2, run_id);
    sqlite3_bind_text(st, 3, kind, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, payload_json ? payload_json : NULL, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 5, now);
    int ok = (sqlite3_step(st) == SQLITE_DONE);
    sqlite3_finalize(st);
    return ok;
}

/* PoP: kdb_list_events @ hermes_cli/kanban_db.py:list_events */
kanban_event_t **kdb_list_events(sqlite3 *conn, const char *task_id, int *out_n)
{
    *out_n = 0;
    if (!conn || !task_id) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT * FROM task_events WHERE task_id = ? ORDER BY id ASC",
            -1, &st, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    kanban_event_t **out = NULL; int cap = 0, cnt = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (cnt >= cap) { cap = cap ? cap*2 : 8; out = realloc(out, sizeof(*out)*(size_t)(cap+1)); }
        out[cnt++] = kdb_event_from_row(st);
    }
    sqlite3_finalize(st);
    if (out) out[cnt] = NULL;
    *out_n = cnt;
    return out;
}

void kdb_event_list_free(kanban_event_t **list)
{
    if (!list) return;
    for (int i = 0; list[i]; i++) kdb_event_free(list[i]);
    free(list);
}

/* ---- runs ---- */

/* PoP: kdb_list_runs @ hermes_cli/kanban_db.py:list_runs */
kanban_run_t **kdb_list_runs(sqlite3 *conn, const char *task_id,
                                int include_active, int *out_n)
{
    *out_n = 0;
    if (!conn || !task_id) return NULL;
    char q[256];
    int n = snprintf(q, sizeof(q), "SELECT * FROM task_runs WHERE task_id = ?");
    if (!include_active) n += snprintf(q + n, sizeof(q) - n, " AND ended_at IS NOT NULL");
    n += snprintf(q + n, sizeof(q) - n, " ORDER BY started_at ASC, id ASC");
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, q, -1, &st, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    kanban_run_t **out = NULL; int cap = 0, cnt = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (cnt >= cap) { cap = cap ? cap*2 : 8; out = realloc(out, sizeof(*out)*(size_t)(cap+1)); }
        out[cnt++] = kdb_run_from_row(st);
    }
    sqlite3_finalize(st);
    if (out) out[cnt] = NULL;
    *out_n = cnt;
    return out;
}

void kdb_run_list_free(kanban_run_t **list)
{
    if (!list) return;
    for (int i = 0; list[i]; i++) kdb_run_free(list[i]);
    free(list);
}

/* PoP: kdb_get_run @ hermes_cli/kanban_db.py:get_run */
kanban_run_t *kdb_get_run(sqlite3 *conn, long run_id)
{
    if (!conn) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, "SELECT * FROM task_runs WHERE id = ?",
                           -1, &st, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int64(st, 1, run_id);
    kanban_run_t *r = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) r = kdb_run_from_row(st);
    sqlite3_finalize(st);
    return r;
}

/* PoP: kdb_latest_summary @ hermes_cli/kanban_db.py:complete_task */
char *kdb_latest_summary(sqlite3 *conn, const char *task_id)
{
    if (!conn || !task_id) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT summary FROM task_runs WHERE task_id = ? AND summary IS NOT NULL "
            "AND summary != '' ORDER BY COALESCE(ended_at, started_at) DESC, id DESC LIMIT 1",
            -1, &st, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    char *out = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const char *s = (const char *)sqlite3_column_text(st, 0);
        if (s) out = strdup(s);
    }
    sqlite3_finalize(st);
    return out;
}
