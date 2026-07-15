/*
 * kanban_util.c — portable DB/logic helpers ported from hermes_cli/kanban_db.py
 *
 * Concern-split companion to the kanban engine. Holds the genuinely-portable
 * helper surface the dispatcher/CLI rely on but that has no OS/subprocess or
 * corruption-recovery dependency: id generation, parent/phantom resolution,
 * scratch-path guards, workspace resolution, and spawnable health queries.
 *
 * Kept separate from kanban_tasks.c / kanban_runs.c so no single module grows
 * into a monolith; reuses kdb_child_ids / kdb_strv_free from siblings.
 */

#include "kanban_db.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <dirent.h>

/* local hex/word helpers (used by prose scanner below) */
static int is_hex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
static int is_alnum_kanban(char c) {
    return is_hex(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/* Make all components of a path (like mkdir -p). */
static void mkdirs(const char *path);

/* ------------------------------------------------------------------ */
/* id generation                                                      */
/* ------------------------------------------------------------------ */

/* PoP: kdb_new_task_id @ hermes_cli/kanban_db.py:_new_task_id */
char *kdb_new_task_id(void)
{
    unsigned char b[4];
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) { size_t got = fread(b, 1, 4, f); (void)got; fclose(f); }
    else { for (int i = 0; i < 4; i++) b[i] = (unsigned char)(time(NULL) ^ (i * 2654435761u)); }
    char *out = malloc(16);
    snprintf(out, 16, "t_%02x%02x%02x%02x", b[0], b[1], b[2], b[3]);
    return out;
}

/* PoP: kdb_claimer_id @ hermes_cli/kanban_db.py:_claimer_id */
char *kdb_claimer_id(void)
{
    char host[256];
    if (gethostname(host, sizeof(host)) != 0) snprintf(host, sizeof(host), "unknown");
    host[sizeof(host) - 1] = '\0';
    char *out = malloc(strlen(host) + 24);
    snprintf(out, strlen(host) + 24, "%s:%d", host, (int)getpid());
    return out;
}

/* ------------------------------------------------------------------ */
/* parent / phantom resolution                                        */
/* ------------------------------------------------------------------ */

/* PoP: kdb_find_missing_parents @ hermes_cli/kanban_db.py:_find_missing_parents */
char **kdb_find_missing_parents(sqlite3 *conn, char **parents, int n)
{
    if (!conn || n <= 0) { char **e = malloc(sizeof(char*)); e[0] = NULL; return e; }
    char q[256];
    int off = snprintf(q, sizeof(q), "SELECT id FROM tasks WHERE id IN (");
    for (int i = 0; i < n; i++) off += (int)snprintf(q + off, sizeof(q) - (size_t)off, i ? ",?" : "?");
    snprintf(q + off, sizeof(q) - (size_t)off, ")");
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, q, -1, &st, NULL) != SQLITE_OK) { char **e = malloc(sizeof(char*)); e[0] = NULL; return e; }
    for (int i = 0; i < n; i++) sqlite3_bind_text(st, i + 1, parents[i], -1, SQLITE_TRANSIENT);
    char **present = NULL; int pc = 0, pcap = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *id = (const char *)sqlite3_column_text(st, 0);
        if (!id) continue;
        if (pc >= pcap) { pcap = pcap ? pcap * 2 : 8; present = realloc(present, sizeof(char*) * (size_t)(pcap + 1)); }
        present[pc++] = strdup(id);
    }
    sqlite3_finalize(st);
    if (present) present[pc] = NULL;

    char **miss = malloc(sizeof(char*) * (size_t)(n + 1));
    int mc = 0;
    for (int i = 0; i < n; i++) {
        int found = 0;
        for (int j = 0; present && present[j]; j++) if (strcmp(present[j], parents[i]) == 0) { found = 1; break; }
        if (!found) miss[mc++] = strdup(parents[i]);
    }
    miss[mc] = NULL;
    if (present) { for (int j = 0; present[j]; j++) free(present[j]); free(present); }
    return miss;
}

/* PoP: kdb_verify_created_cards @ hermes_cli/kanban_db.py:_verify_created_cards */
int kdb_verify_created_cards(sqlite3 *conn, const char *completing_task_id,
                             char **claimed_ids, int n,
                             char **out_verified, char **out_phantom)
{
    char **verified = malloc(sizeof(char*) * (size_t)(n + 1));
    char **phantom = malloc(sizeof(char*) * (size_t)(n + 1));
    int vc = 0, pc = 0;
    verified[0] = NULL; phantom[0] = NULL;
    *out_verified = NULL; *out_phantom = NULL;
    if (!conn || n <= 0) { free(verified); free(phantom); return 1; }

    char **ordered = malloc(sizeof(char*) * (size_t)(n + 1));
    int oc = 0; char **seen = malloc(sizeof(char*) * (size_t)(n + 1)); int sc = 0;
    for (int i = 0; i < n; i++) {
        char *cid = claimed_ids[i] ? claimed_ids[i] : (char*)"";
        while (*cid == ' ' || *cid == '\t') cid++;
        size_t L = strlen(cid); while (L && (cid[L-1]==' '||cid[L-1]=='\t')) cid[--L]=0;
        if (!*cid) continue;
        int dup = 0;
        for (int j = 0; j < sc; j++) if (strcmp(seen[j], cid) == 0) { dup = 1; break; }
        if (dup) continue;
        seen[sc++] = (char*)cid; ordered[oc++] = (char*)cid;
    }
    ordered[oc] = NULL;

    char *completing_assignee = NULL;
    char qa[512];
    snprintf(qa, sizeof(qa), "SELECT assignee FROM tasks WHERE id = ?");
    sqlite3_stmt *sa = NULL;
    if (sqlite3_prepare_v2(conn, qa, -1, &sa, NULL) == SQLITE_OK) {
        sqlite3_bind_text(sa, 1, completing_task_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(sa) == SQLITE_ROW) {
            const char *a = (const char *)sqlite3_column_text(sa, 0);
            if (a) completing_assignee = strdup(a);
        }
        sqlite3_finalize(sa);
    }
    int completing_found = (completing_assignee != NULL);

    char **found_created = malloc(sizeof(char*) * (size_t)(oc + 1)); int fc = 0;
    char **cby = calloc((size_t)(oc + 1), sizeof(char*));
    if (oc) {
        char q[256]; int off = snprintf(q, sizeof(q), "SELECT id, created_by FROM tasks WHERE id IN (");
        for (int i = 0; ordered[i]; i++) off += (int)snprintf(q + off, sizeof(q) - (size_t)off, i ? ",?" : "?");
        snprintf(q + off, sizeof(q) - (size_t)off, ")");
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(conn, q, -1, &st, NULL) == SQLITE_OK) {
            for (int i = 0; ordered[i]; i++) sqlite3_bind_text(st, i + 1, ordered[i], -1, SQLITE_TRANSIENT);
            while (sqlite3_step(st) == SQLITE_ROW) {
                const char *id = (const char *)sqlite3_column_text(st, 0);
                if (!id) continue;
                found_created[fc++] = strdup(id);
                const char *cb = (const char *)sqlite3_column_text(st, 1);
                for (int i = 0; ordered[i]; i++) if (strcmp(ordered[i], id) == 0) { if (cb) cby[i] = strdup(cb); break; }
            }
            sqlite3_finalize(st);
        }
    }
    found_created[fc] = NULL;

    int *child_arr = NULL;
    if (completing_found) {
        int cn = 0;
        char **kids = kdb_child_ids(conn, completing_task_id, &cn);
        child_arr = calloc((size_t)(oc + 1), sizeof(int));
        for (int i = 0; ordered[i]; i++)
            for (int k = 0; kids && kids[k]; k++)
                if (strcmp(ordered[i], kids[k]) == 0) { child_arr[i] = 1; break; }
        kdb_child_ids_free(kids);
    }

    for (int i = 0; ordered[i]; i++) {
        int exists = 0;
        for (int j = 0; found_created[j]; j++) if (strcmp(found_created[j], ordered[i]) == 0) { exists = 1; break; }
        if (!exists) { phantom[pc++] = strdup(ordered[i]); continue; }
        char *created_by = cby[i];
        int ok = 0;
        if (completing_found && created_by && completing_assignee && strcmp(created_by, completing_assignee) == 0) ok = 1;
        else if (created_by && strcmp(created_by, completing_task_id) == 0) ok = 1;
        else if (child_arr && child_arr[i]) ok = 1;
        if (ok) verified[vc++] = strdup(ordered[i]);
        else phantom[pc++] = strdup(ordered[i]);
    }
    verified[vc] = NULL; phantom[pc] = NULL;

    size_t cap = 64, len = 0;
    char *vj = malloc(cap);
    len += (size_t)snprintf(vj + len, cap - len, "[");
    for (int i = 0; verified[i]; i++) {
        if (i) vj[len++] = ',';
        size_t need = len + strlen(verified[i]) + 4;
        if (need >= cap) { cap = need * 2; vj = realloc(vj, cap); }
        len += (size_t)snprintf(vj + len, cap - len, "\"%s\"", verified[i]);
    }
    vj[len++] = ']'; vj[len] = 0;
    char *pj = malloc(cap = 64); len = 0;
    len += (size_t)snprintf(pj + len, cap - len, "[");
    for (int i = 0; phantom[i]; i++) {
        if (i) pj[len++] = ',';
        size_t need = len + strlen(phantom[i]) + 4;
        if (need >= cap) { cap = need * 2; pj = realloc(pj, cap); }
        len += (size_t)snprintf(pj + len, cap - len, "\"%s\"", phantom[i]);
    }
    pj[len++] = ']'; pj[len] = 0;

    *out_verified = vj; *out_phantom = pj;

    for (int i = 0; verified[i]; i++) free(verified[i]);
    for (int i = 0; phantom[i]; i++) free(phantom[i]);
    free(verified); free(phantom);
    for (int i = 0; found_created[i]; i++) free(found_created[i]); free(found_created);
    for (int i = 0; cby[i]; i++) free(cby[i]); free(cby);
    free(child_arr);
    free(ordered); free(seen);
    if (completing_assignee) free(completing_assignee);
    return 1;
}

/* PoP: kdb_scan_prose_for_phantom_ids @ hermes_cli/kanban_db.py:_scan_prose_for_phantom_ids */
char *kdb_scan_prose_for_phantom_ids(sqlite3 *conn, const char *text)
{
    char *out = malloc(64); size_t cap = 64, len = 0;
    len += (size_t)snprintf(out + len, cap - len, "[");
    if (!conn || !text || !*text) { out[len++] = ']'; out[len] = 0; return out; }

    char **unique = NULL; int uc = 0, ucap = 0;
    const char *p = text;
    while (*p) {
        if ((p == text || !(is_alnum_kanban(*(p-1)) || *(p-1)=='_')) && p[0]=='t' && p[1]=='_') {
            const char *q = p + 2;
            int hn = 0;
            while (is_hex(q[hn])) hn++;
            if (hn >= 8) {
                char tok[32]; snprintf(tok, sizeof(tok), "t_%.*s", hn, q);
                int dup = 0;
                for (int i = 0; i < uc; i++) if (strcmp(unique[i], tok) == 0) { dup = 1; break; }
                if (!dup) {
                    if (uc >= ucap) { ucap = ucap ? ucap*2 : 8; unique = realloc(unique, sizeof(char*)*(size_t)(ucap+1)); }
                    unique[uc++] = strdup(tok);
                }
                p = q + hn; continue;
            }
        }
        p++;
    }
    if (uc) unique[uc] = NULL;

    char **existing = NULL; int ec = 0;
    if (uc) {
        existing = malloc(sizeof(char*) * (size_t)(uc + 1));
        char q[256]; int off = snprintf(q, sizeof(q), "SELECT id FROM tasks WHERE id IN (");
        for (int i = 0; i < uc; i++) off += (int)snprintf(q + off, sizeof(q) - (size_t)off, i ? ",?" : "?");
        snprintf(q + off, sizeof(q) - (size_t)off, ")");
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(conn, q, -1, &st, NULL) == SQLITE_OK) {
            for (int i = 0; i < uc; i++) sqlite3_bind_text(st, i + 1, unique[i], -1, SQLITE_TRANSIENT);
            while (sqlite3_step(st) == SQLITE_ROW) {
                const char *id = (const char*)sqlite3_column_text(st, 0);
                if (!id) continue;
                existing[ec++] = strdup(id);
            }
            sqlite3_finalize(st);
        }
        existing[ec] = NULL;
    }

    int first = 1;
    for (int i = 0; i < uc; i++) {
        int exists = 0;
        for (int j = 0; existing && existing[j]; j++) if (strcmp(existing[j], unique[i]) == 0) { exists = 1; break; }
        if (!exists) {
            if (!first) out[len++] = ',';
            size_t need = len + strlen(unique[i]) + 4;
            if (need >= cap) { cap = need * 2; out = realloc(out, cap); }
            len += (size_t)snprintf(out + len, cap - len, "\"%s\"", unique[i]);
            first = 0;
        }
    }
    out[len++] = ']'; out[len] = 0;
    for (int i = 0; unique && unique[i]; i++) free(unique[i]); free(unique);
    for (int i = 0; existing && existing[i]; i++) free(existing[i]); free(existing);
    return out;
}

/* ------------------------------------------------------------------ */
/* scratch-path guard                                                 */
/* ------------------------------------------------------------------ */

/* PoP: kdb_is_managed_scratch_path @ hermes_cli/kanban_db.py:_is_managed_scratch_path */
int kdb_is_managed_scratch_path(const char *path)
{
    if (!path || !*path) return 0;
    char resolved[PATH_MAX];
    if (realpath(path, resolved) == NULL) {
        if (realpath(".", resolved) == NULL) return 0;
        size_t L = strlen(resolved);
        if (L + 1 + strlen(path) >= PATH_MAX) return 0;
        resolved[L] = '/'; resolved[L+1] = 0;
        strncat(resolved, path, PATH_MAX - L - 2);
    }

    char **roots = NULL; int rc = 0;
    const char *override = getenv("HERMES_KANBAN_WORKSPACES_ROOT");
    if (override && override[0]) {
        char ov[PATH_MAX];
        if (realpath(override, ov) != NULL) { roots = realloc(roots, sizeof(char*)*2); roots[rc++] = strdup(ov); }
    }
    char *home = kanban_home();
    if (home) {
        char wr[PATH_MAX];
        snprintf(wr, sizeof(wr), "%s/kanban/workspaces", home);
        char wrc[PATH_MAX];
        if (realpath(wr, wrc) != NULL) { roots = realloc(roots, sizeof(char*)*(size_t)(rc+1)); roots[rc++] = strdup(wrc); }
        char bp[PATH_MAX];
        snprintf(bp, sizeof(bp), "%s/kanban/boards", home);
        char bpc[PATH_MAX];
        if (realpath(bp, bpc) != NULL) {
            DIR *d = opendir(bpc);
            if (d) {
                struct dirent *e;
                while ((e = readdir(d))) {
                    if (e->d_name[0] == '.') continue;
                    char eb[PATH_MAX];
                    snprintf(eb, sizeof(eb), "%s/%s/workspaces", bpc, e->d_name);
                    char ebc[PATH_MAX];
                    if (realpath(eb, ebc) != NULL) { roots = realloc(roots, sizeof(char*)*(size_t)(rc+1)); roots[rc++] = strdup(ebc); }
                }
                closedir(d);
            }
        }
        free(home);
    }
    int result = 0;
    for (int i = 0; i < rc; i++) {
        size_t rl = strlen(roots[i]);
        if (strcmp(resolved, roots[i]) == 0) continue;
        if (strncmp(resolved, roots[i], rl) == 0 && resolved[rl] == '/') { result = 1; break; }
    }
    for (int i = 0; i < rc; i++) free(roots[i]);
    free(roots);
    return result;
}

/* ------------------------------------------------------------------ */
/* workspace resolution                                               */
/* ------------------------------------------------------------------ */

/* PoP: kdb_resolve_workspace @ hermes_cli/kanban_db.py:resolve_workspace */
char *kdb_resolve_workspace(const char *kind, const char *workspace_path,
                             const char *task_id, const char *board)
{
    const char *k = kind && *kind ? kind : "scratch";
    if (strcmp(k, "scratch") == 0) {
        char *p = NULL;
        if (workspace_path && workspace_path[0]) {
            char tmp[PATH_MAX];
            if (workspace_path[0] != '/') return NULL;
            strncpy(tmp, workspace_path, PATH_MAX - 1); tmp[PATH_MAX-1] = 0;
            p = strdup(tmp);
        } else {
            char *wr = workspaces_root(board);
            size_t L = strlen(wr) + strlen(task_id) + 2;
            p = malloc(L);
            snprintf(p, L, "%s/%s", wr, task_id ? task_id : "task");
            free(wr);
        }
        mkdirs(p);
        return p;
    }
    if (strcmp(k, "dir") == 0) {
        if (!workspace_path || !workspace_path[0]) return NULL;
        if (workspace_path[0] != '/') return NULL;
        char *p = strdup(workspace_path);
        mkdirs(p);
        return p;
    }
    if (strcmp(k, "worktree") == 0) {
        if (workspace_path && workspace_path[0]) {
            char *p = strdup(workspace_path);
            mkdirs(p);
            return p;
        }
        char *wr = workspaces_root(board);
        size_t L = strlen(wr) + strlen(task_id) + 2;
        char *p = malloc(L);
        snprintf(p, L, "%s/%s", wr, task_id ? task_id : "task");
        free(wr);
        mkdirs(p);
        return p;
    }
    return NULL;
}

/* Make all components of a path (like mkdir -p). */
static void mkdirs(const char *path)
{
    if (!path) return;
    char tmp[PATH_MAX];
    strncpy(tmp, path, PATH_MAX - 1); tmp[PATH_MAX - 1] = 0;
    for (char *pp = tmp + 1; *pp; pp++) {
        if (*pp == '/') { *pp = 0; mkdir(tmp, 0755); *pp = '/'; }
    }
    mkdir(tmp, 0755);
}

/* PoP: kdb_set_workspace_path @ hermes_cli/kanban_db.py:set_workspace_path */
int kdb_set_workspace_path(sqlite3 *conn, const char *task_id, const char *path)
{
    if (!conn || !task_id) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE tasks SET workspace_path = ? WHERE id = ?", -1, &st, NULL) != SQLITE_OK)
        return 0;
    sqlite3_bind_text(st, 1, path ? path : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, task_id, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE;
}

/* ------------------------------------------------------------------ */
/* spawnable health queries                                           */
/* ------------------------------------------------------------------ */

/* PoP: kdb_has_spawnable_ready @ hermes_cli/kanban_db.py:has_spawnable_ready */
int kdb_has_spawnable_ready(sqlite3 *conn)
{
    if (!conn) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT DISTINCT assignee FROM tasks WHERE status = 'ready' "
            "AND assignee IS NOT NULL AND claim_lock IS NULL", -1, &st, NULL) != SQLITE_OK)
        return 0;
    int found = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *a = (const char *)sqlite3_column_text(st, 0);
        if (a && profile_exists(a)) { found = 1; break; }
    }
    sqlite3_finalize(st);
    return found;
}

/* PoP: kdb_has_spawnable_review @ hermes_cli/kanban_db.py:has_spawnable_review */
int kdb_has_spawnable_review(sqlite3 *conn)
{
    if (!conn) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT DISTINCT assignee FROM tasks WHERE status = 'review' "
            "AND assignee IS NOT NULL AND claim_lock IS NULL", -1, &st, NULL) != SQLITE_OK)
        return 0;
    int found = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *a = (const char *)sqlite3_column_text(st, 0);
        if (a && profile_exists(a)) { found = 1; break; }
    }
    sqlite3_finalize(st);
    return found;
}
