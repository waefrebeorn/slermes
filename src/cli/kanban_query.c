/*
 * kanban_query.c — read-only analytics / enumeration for hermes_cli/kanban_db.py
 *
 * Concern: GC of stale events (gc_events), profile/on-disk discovery
 * (kdb_list_profiles_on_disk), assignee enumeration (known_assignees),
 * and run-summary lookups (latest_run / latest_summary / latest_summaries).
 * All read-mostly; no storage duplication. Reuses the engine's model
 * from_row constructors + the path helper kanban_home().
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
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

/* Path helper already ported in port_kanban_db.c. */
char *kanban_home(void);

/* =========================================================================
 * Event GC
 * ========================================================================= */

/* PoP: kdb_gc_events @ hermes_cli/kanban_db.py:gc_events */
int kdb_gc_events(sqlite3 *conn, int older_than_seconds)
{
    if (!conn) return 0;
    if (older_than_seconds <= 0) older_than_seconds = 30 * 24 * 3600;
    long cutoff = kdb_now() - older_than_seconds;
    if (kdb_write_begin(conn) != 0) return 0;
    sqlite3_stmt *st = NULL;
    int n = 0;
    if (sqlite3_prepare_v2(conn,
            "DELETE FROM task_events WHERE created_at < ? AND task_id IN "
            "(SELECT id FROM tasks WHERE status IN ('done','archived'))",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(st, 1, cutoff);
        if (sqlite3_step(st) == SQLITE_DONE) n = sqlite3_changes(conn);
        sqlite3_finalize(st);
    }
    kdb_write_end(conn, 1);
    return n;
}

/* =========================================================================
 * On-disk profile discovery
 * ========================================================================= */

/* PoP: kdb_list_profiles_on_disk @ hermes_cli/kanban_db.py:list_profiles_on_disk */
char **kdb_list_profiles_on_disk(void)
{
    int n = 0, cap = 8;
    char **names = malloc(sizeof(char*) * cap);
    char *home = kanban_home();
    if (!home) { names = realloc(names, sizeof(char*)); names[0] = NULL; return names; }
    int have_default_root = 0;
    /* default root exists if <home> exists */
    struct stat hs;
    if (stat(home, &hs) == 0) have_default_root = 1;
    char profiles_dir[4096];
    snprintf(profiles_dir, sizeof(profiles_dir), "%s/profiles", home);
    if (have_default_root) { names[n++] = strdup("default"); }
    DIR *d = opendir(profiles_dir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (e->d_name[0] == '.') continue;
            char sub[8192];
            snprintf(sub, sizeof(sub), "%s/%s", profiles_dir, e->d_name);
            struct stat st;
            if (stat(sub, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
            char cfg[9000];
            snprintf(cfg, sizeof(cfg), "%s/%s/config.yaml", profiles_dir, e->d_name);
            if (stat(cfg, &st) == 0) {
                if (n >= cap) { cap *= 2; names = realloc(names, sizeof(char*) * cap); }
                names[n++] = strdup(e->d_name);
            }
        }
        closedir(d);
    }
    /* de-dup "default" if a profile dir named default also existed */
    free(home);
    names = realloc(names, sizeof(char*) * (n + 1));
    names[n] = NULL;
    /* simple stable sort */
    for (int i = 1; i < n; i++) {
        char *key = names[i];
        int j = i - 1;
        while (j >= 0 && strcmp(names[j], key) > 0) { names[j+1] = names[j]; j--; }
        names[j+1] = key;
    }
    return names;
}

void kdb_strv_free(char **v)
{
    if (!v) return;
    for (int i = 0; v[i]; i++) free(v[i]);
    free(v);
}

/* =========================================================================
 * Assignee enumeration
 * ========================================================================= */

/* PoP: kdb_known_assignees @ hermes_cli/kanban_db.py:known_assignees */
char *kdb_known_assignees(sqlite3 *conn)
{
    if (!conn) return strdup("[]");
    char **on_disk = kdb_list_profiles_on_disk();
    int n_disk = 0;
    while (on_disk[n_disk]) n_disk++;

    /* counts[assignee]["status"] = n ; stored in a small flat structure. */
    typedef struct { char *name; int on_disk; int n_status; char **st; int *cnt; } Asg;
    int n_asg = 0, cap_a = 8;
    Asg *asg = malloc(sizeof(Asg) * cap_a);

    /* seed with disk names */
    for (int i = 0; i < n_disk; i++) {
        if (n_asg >= cap_a) { cap_a *= 2; asg = realloc(asg, sizeof(Asg) * cap_a); }
        asg[n_asg].name = strdup(on_disk[i]);
        asg[n_asg].on_disk = 1;
        asg[n_asg].n_status = 0; asg[n_asg].st = NULL; asg[n_asg].cnt = NULL;
        n_asg++;
    }

    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT assignee, status, COUNT(*) AS n FROM tasks "
            "WHERE status != 'archived' AND assignee IS NOT NULL "
            "GROUP BY assignee, status", -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            const char *a = (const char*)sqlite3_column_text(st, 0);
            const char *s = (const char*)sqlite3_column_text(st, 1);
            int c = (int)sqlite3_column_int64(st, 2);
            if (!a) continue;
            int idx = -1;
            for (int i = 0; i < n_asg; i++) if (strcmp(asg[i].name, a) == 0) { idx = i; break; }
            if (idx < 0) {
                if (n_asg >= cap_a) { cap_a *= 2; asg = realloc(asg, sizeof(Asg) * cap_a); }
                asg[n_asg].name = strdup(a);
                asg[n_asg].on_disk = 0;
                asg[n_asg].n_status = 0; asg[n_asg].st = NULL; asg[n_asg].cnt = NULL;
                idx = n_asg++;
            }
            int k = asg[idx].n_status++;
            asg[idx].st = realloc(asg[idx].st, sizeof(char*) * asg[idx].n_status);
            asg[idx].cnt = realloc(asg[idx].cnt, sizeof(int) * asg[idx].n_status);
            asg[idx].st[k] = strdup(s ? s : "");
            asg[idx].cnt[k] = c;
        }
        sqlite3_finalize(st);
    }

    /* emit JSON array, sorted by name (stable) */
    for (int i = 1; i < n_asg; i++) {
        Asg key = asg[i]; int j = i - 1;
        while (j >= 0 && strcmp(asg[j].name, key.name) > 0) { asg[j+1] = asg[j]; j--; }
        asg[j+1] = key;
    }

    size_t cap = 4096; char *out = malloc(cap); size_t len = 0;
    len += snprintf(out + len, cap - len, "[");
    for (int i = 0; i < n_asg; i++) {
        if (len + 256 >= cap) { cap *= 2; out = realloc(out, cap); }
        len += snprintf(out + len, cap - len,
                        "%s{\"name\":", i ? "," : "");
        /* JSON-quote the name (value) */
        {
            const char *nm = asg[i].name;
            len += snprintf(out + len, cap - len, "\"");
            for (const char *p = nm; *p; p++) {
                if (*p == '"' || *p == '\\') {
                    if (len + 8 >= cap) { cap *= 2; out = realloc(out, cap); }
                    len += snprintf(out + len, cap - len, "\\%c", *p);
                } else {
                    if (len + 2 >= cap) { cap *= 2; out = realloc(out, cap); }
                    out[len++] = *p;
                }
            }
            out[len] = '\0';
            len += snprintf(out + len, cap - len, "\"");
        }
        len += snprintf(out + len, cap - len, ",\"on_disk\":%s,\"counts\":{",
                        asg[i].on_disk ? "true" : "false");
        for (int k = 0; k < asg[i].n_status; k++) {
            if (len + 256 >= cap) { cap *= 2; out = realloc(out, cap); }
            len += snprintf(out + len, cap - len, "%s", k ? "," : "");
            const char *stt = asg[i].st[k];
            len += snprintf(out + len, cap - len, "\"");
            for (const char *p = stt; *p; p++) {
                if (*p == '"' || *p == '\\') {
                    if (len + 8 >= cap) { cap *= 2; out = realloc(out, cap); }
                    len += snprintf(out + len, cap - len, "\\%c", *p);
                } else {
                    if (len + 2 >= cap) { cap *= 2; out = realloc(out, cap); }
                    out[len++] = *p;
                }
            }
            out[len] = '\0';
            len += snprintf(out + len, cap - len, "\":%d", asg[i].cnt[k]);
        }
        len += snprintf(out + len, cap - len, "}");
        /* close object */
        len += snprintf(out + len, cap - len, "}");
    }
    len += snprintf(out + len, cap - len, "]");

    /* cleanup */
    kdb_strv_free(on_disk);
    for (int i = 0; i < n_asg; i++) {
        free(asg[i].name);
        for (int k = 0; k < asg[i].n_status; k++) free(asg[i].st[k]);
        free(asg[i].st); free(asg[i].cnt);
    }
    free(asg);
    return out;
}

/* =========================================================================
 * Run summaries
 * ========================================================================= */

/* PoP: kdb_latest_run @ hermes_cli/kanban_db.py:latest_run */
kanban_run_t *kdb_latest_run(sqlite3 *conn, const char *task_id)
{
    if (!conn || !task_id) return NULL;
    sqlite3_stmt *st = NULL;
    kanban_run_t *r = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT * FROM task_runs WHERE task_id=? "
            "ORDER BY started_at DESC, id DESC LIMIT 1",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) r = kdb_run_from_row(st);
        sqlite3_finalize(st);
    }
    return r;
}

/* PoP: kdb_latest_summary @ hermes_cli/kanban_db.py:latest_summary
 * (definition lives in kanban_tasks.c) */

/* PoP: kdb_latest_summaries_json @ hermes_cli/kanban_db.py:latest_summaries */
char *kdb_latest_summaries_json(sqlite3 *conn, char **task_ids, int n_ids)
{
    if (!conn) return strdup("{}");
    if (n_ids <= 0) return strdup("{}");
    char q[8192];
    int off = snprintf(q, sizeof(q),
        "SELECT task_id, summary FROM ("
        "SELECT task_id, summary,"
        " ROW_NUMBER() OVER (PARTITION BY task_id "
        " ORDER BY COALESCE(ended_at, started_at) DESC, id DESC) AS rn"
        " FROM task_runs WHERE task_id IN (");
    for (int i = 0; i < n_ids; i++)
        off += snprintf(q + off, sizeof(q) - off, i ? ",?" : "?");
    off += snprintf(q + off, sizeof(q) - off,
        ") AND summary IS NOT NULL AND summary != '') WHERE rn = 1");

    size_t cap = 4096; char *out = malloc(cap); size_t len = 0;
    len += snprintf(out + len, cap - len, "{");
    int first = 1;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, q, -1, &st, NULL) == SQLITE_OK) {
        for (int i = 0; i < n_ids; i++)
            sqlite3_bind_text(st, i + 1, task_ids[i], -1, SQLITE_TRANSIENT);
        while (sqlite3_step(st) == SQLITE_ROW) {
            const char *tid = (const char*)sqlite3_column_text(st, 0);
            const char *sum = (const char*)sqlite3_column_text(st, 1);
            if (!tid || !sum) continue;
            if (len + strlen(tid) + strlen(sum) + 16 >= cap) { cap *= 2; out = realloc(out, cap); }
            len += snprintf(out + len, cap - len, "%s%c%s%c:%c%s%c",
                            first ? "" : ",", '"', tid, '"', '"', sum, '"');
            first = 0;
        }
        sqlite3_finalize(st);
    }
    len += snprintf(out + len, cap - len, "}");
    return out;
}
