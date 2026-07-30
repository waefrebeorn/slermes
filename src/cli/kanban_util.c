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
#include <fcntl.h>
#include <crypto.h>

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

/* Logical path normalization mirroring Python Path.resolve(strict=False):
 * expand leading ~, collapse . / .. / duplicate separators, no existence
 * or symlink requirement. Returns 0 on success, -1 on overflow. */
static int logical_norm(const char *path, char *out, size_t outsz)
{
    if (!path || !*path || outsz < 2) return -1;
    char expanded[PATH_MAX];
    if (path[0] == '~') {
        char *home = kanban_home();
        if (home) { snprintf(expanded, sizeof(expanded), "%s%s", home, path + 1); free(home); }
        else snprintf(expanded, sizeof(expanded), "%s", path + 1);
    } else {
        snprintf(expanded, sizeof(expanded), "%s", path);
    }
    out[0] = 0;
    const char *p = expanded;
    if (p[0] == '/') { out[0] = '/'; out[1] = 0; p++; }
    char *o = out + strlen(out);
    char seg[PATH_MAX];
    while (*p) {
        size_t n = strspn(p, "/"); p += n;
        if (!*p) break;
        size_t len = strcspn(p, "/");
        if (len == 1 && p[0] == '.') { p += len; continue; }
        if (len == 2 && p[0] == '.' && p[1] == '.') {
            if (o > out + 1) { o--; while (o > out + 1 && o[-1] != '/') o--; }
            p += len; continue;
        }
        snprintf(seg, sizeof(seg), "/%.*s", (int)len, p);
        if (strlen(out) + strlen(seg) + 1 >= outsz) return -1;
        strcat(out, seg);
        o = out + strlen(out);
        p += len;
    }
    return 0;
}

/* PoP: kdb_is_managed_scratch_path @ hermes_cli/kanban_db.py:_is_managed_scratch_path */
int kdb_is_managed_scratch_path(const char *path)
{
    if (!path || !*path) return 0;
    /* Mirror Python's p.resolve(strict=False): a LOGICAL normalization
     * that does NOT require the path to exist (realpath() would, and
     * would also collapse symlinks). Expand ~, collapse . / .. /
     * duplicate separators. */
    char expanded[PATH_MAX];
    if (path[0] == '~') {
        char *home = kanban_home();
        if (home) { snprintf(expanded, sizeof(expanded), "%s%s", home, path + 1); free(home); }
        else snprintf(expanded, sizeof(expanded), "%s", path + 1);
    } else {
        snprintf(expanded, sizeof(expanded), "%s", path);
    }
    char resolved[PATH_MAX];
    resolved[0] = 0;
    const char *p = expanded;
    if (p[0] == '/') { resolved[0] = '/'; resolved[1] = 0; p++; }
    char *out = resolved + strlen(resolved);
    char seg[PATH_MAX];
    while (*p) {
        size_t n = strspn(p, "/"); p += n;            /* skip seps */
        if (!*p) break;
        size_t len = strcspn(p, "/");
        if (len == 1 && p[0] == '.') { p += len; continue; }
        if (len == 2 && p[0] == '.' && p[1] == '.') {
            /* pop last component */
            if (out > resolved + 1) { out--; while (out > resolved + 1 && out[-1] != '/') out--; }
            p += len; continue;
        }
        snprintf(seg, sizeof(seg), "/%.*s", (int)len, p);
        size_t need = strlen(resolved) + strlen(seg) + 1;
        if (need >= PATH_MAX) return 0;
        strcat(resolved, seg);
        out = resolved + strlen(resolved);
        p += len;
    }

    char **roots = NULL; int rc = 0;
    const char *override = getenv("HERMES_KANBAN_WORKSPACES_ROOT");
    if (override && override[0]) {
        char ov[PATH_MAX];
        if (logical_norm(override, ov, sizeof(ov)) == 0) {
            roots = realloc(roots, sizeof(char*)*2); roots[rc++] = strdup(ov);
        }
    }
    char *home = kanban_home();
    if (home) {
        char wr[PATH_MAX];
        snprintf(wr, sizeof(wr), "%s/kanban/workspaces", home);
        char wrc[PATH_MAX];
        if (logical_norm(wr, wrc, sizeof(wrc)) == 0) {
            roots = realloc(roots, sizeof(char*)*(size_t)(rc+1)); roots[rc++] = strdup(wrc);
        }
        char bp[PATH_MAX];
        snprintf(bp, sizeof(bp), "%s/kanban/boards", home);
        char bpc[PATH_MAX];
        if (logical_norm(bp, bpc, sizeof(bpc)) == 0) {
            DIR *d = opendir(bpc);
            if (d) {
                struct dirent *e;
                while ((e = readdir(d))) {
                    if (e->d_name[0] == '.') continue;
                    char eb[PATH_MAX];
                    snprintf(eb, sizeof(eb), "%s/%s/workspaces", bpc, e->d_name);
                    char ebc[PATH_MAX];
                    if (logical_norm(eb, ebc, sizeof(ebc)) == 0) {
                        roots = realloc(roots, sizeof(char*)*(size_t)(rc+1)); roots[rc++] = strdup(ebc);
                    }
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

/* ------------------------------------------------------------------ */
/* connection / error helpers                                         */
/* ------------------------------------------------------------------ */

/* PoP: kdb_is_busy_error @ hermes_cli/kanban_db.py:_is_busy_error */
int kdb_is_busy_error(const char *msg)
{
    if (!msg) return 0;
    char low[512];
    size_t n = 0;
    for (const char *pp = msg; *pp && n + 1 < sizeof(low); pp++) {
        char c = *pp;
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'a' + 'A');
        low[n++] = c;
    }
    low[n] = 0;
    return strstr(low, "database is locked") != NULL
        || strstr(low, "database is busy") != NULL;
}

/* PoP: kdb_absolute_hermes_path @ hermes_cli/kanban_db.py:_absolute_hermes_path */
char *kdb_absolute_hermes_path(const char *path)
{
    if (!path) return NULL;
    char *exp = NULL;
    if (path[0] == '~') {
        char *home = kanban_home();
        if (home) {
            size_t L = strlen(home) + strlen(path) + 1;
            exp = malloc(L);
            snprintf(exp, L, "%s%s", home, path + 1);
            free(home);
        } else {
            exp = strdup(path);
        }
    } else {
        exp = strdup(path);
    }
    if (!exp) return NULL;
    if (exp[0] == '/') return exp;
    char cwd[PATH_MAX];
    if (realpath(".", cwd) == NULL) { free(exp); return NULL; }
    size_t L = strlen(cwd) + strlen(exp) + 2;
    char *out = malloc(L);
    snprintf(out, L, "%s/%s", cwd, exp);
    free(exp);
    return out;
}

/* ------------------------------------------------------------------ */
/* path search (worker argv resolution) - portable subset            */
/* ------------------------------------------------------------------ */

/* PoP: kdb_path_search_names @ hermes_cli/kanban_db.py:_path_search_names */
char **kdb_path_search_names(const char *command)
{
    char **out = malloc(sizeof(char*) * 2);
    out[0] = strdup(command);
    out[1] = NULL;
    return out;
}

/* PoP: kdb_safe_which_no_cwd @ hermes_cli/kanban_db.py:_safe_which_no_cwd */
char *kdb_safe_which_no_cwd(const char *command)
{
    if (!command) return NULL;
    const char *path_env = getenv("PATH");
    if (!path_env) return NULL;
    char **names = kdb_path_search_names(command);
    char tmp[PATH_MAX];
    char *result = NULL;
    const char *sep = (strchr(path_env, ':') ? ":" : ";");
    for (const char *pp = path_env; ; ) {
        size_t len = strcspn(pp, sep);
        if (!(len == 0 || (len == 1 && pp[0] == '.'))) {
            char dir[PATH_MAX];
            if (len >= sizeof(dir)) { dir[0] = 0; }
            else { memcpy(dir, pp, len); dir[len] = 0; }
            char *ed = NULL;
            if (dir[0] == '~') {
                char *home = kanban_home();
                if (home) { size_t L = strlen(home) + len + 1; ed = malloc(L); snprintf(ed, L, "%s%s", home, dir + 1); free(home); }
                else ed = strdup(dir);
            } else ed = strdup(dir);
            for (int i = 0; names[i] && !result; i++) {
                snprintf(tmp, sizeof(tmp), "%s/%s", ed, names[i]);
                struct stat stt;
                if (stat(tmp, &stt) == 0 && S_ISREG(stt.st_mode)
#ifdef X_OK
                    && access(tmp, X_OK) == 0
#endif
                ) {
                    result = strdup(tmp);
                }
            }
            free(ed);
        }
        if (pp[len] == 0) break;
        pp += len + 1;
    }
    for (int i = 0; names[i]; i++) free(names[i]);
    free(names);
    return result;
}

/* ------------------------------------------------------------------ */
/* schema drift detection                                             */
/* ------------------------------------------------------------------ */

/* PoP: kdb_table_has_drifted @ hermes_cli/kanban_db.py:_table_has_drifted */
int kdb_table_has_drifted(sqlite3 *conn, const char *table)
{
    if (!conn || !table) return 0;
    char q[256];
    snprintf(q, sizeof(q), "PRAGMA table_info(%s)", table);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, q, -1, &st, NULL) != SQLITE_OK) return 0;
    int id_pk = 0; char id_type[32]; id_type[0] = 0;
    int lei_seen = 0; char lei_type[32]; lei_type[0] = 0;
    int is_notify = (strcmp(table, "kanban_notify_subs") == 0);
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *name = (const char *)sqlite3_column_text(st, 1);
        const char *type = (const char *)sqlite3_column_text(st, 2);
        int pk = sqlite3_column_int(st, 5);
        if (name && strcmp(name, "id") == 0) {
            if (type) snprintf(id_type, sizeof(id_type), "%s", type);
            id_pk = pk;
        }
        if (is_notify && name && strcmp(name, "last_event_id") == 0) {
            lei_seen = 1;
            if (type) snprintf(lei_type, sizeof(lei_type), "%s", type);
        }
    }
    sqlite3_finalize(st);
    if (is_notify) {
        if (!lei_seen) return 0;
        char up[32]; size_t n = 0;
        for (const char *pp = lei_type; *pp && n + 1 < sizeof(up); pp++) {
            char c = *pp; if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A'); up[n++] = c;
        }
        up[n] = 0;
        return strcmp(up, "INTEGER") != 0;
    }
    if (id_type[0] == 0) return 0;
    char up[32]; size_t n = 0;
    for (const char *pp = id_type; *pp && n + 1 < sizeof(up); pp++) {
        char c = *pp; if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A'); up[n++] = c;
    }
    up[n] = 0;
    return !(strcmp(up, "INTEGER") == 0 && id_pk);
}

/* ------------------------------------------------------------------ */
/* run queries                                                        */
/* ------------------------------------------------------------------ */



/* ------------------------------------------------------------------ */
/* DB integrity / corruption recovery (portable subset)               */
/* ------------------------------------------------------------------ */

#define KDB_SCRATCH_TIP_SENTINEL ".scratch_tip_shown"
static const char KDB_SCRATCH_TIP_MESSAGE[] =
    "scratch workspaces are ephemeral -- they are deleted when the task "
    "completes. Use --workspace worktree (git worktree) or "
    "--workspace dir:/abs/path (existing dir) to preserve worker output.";

/* PoP: kdb_check_file_length_invariant @ hermes_cli/kanban_db.py:_check_file_length_invariant */
int kdb_check_file_length_invariant(sqlite3 *conn)
{
    if (!conn) return 0;
    /* Resolve the on-disk file via database_list (col 2 = path). */
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, "PRAGMA database_list", -1, &st, NULL) != SQLITE_OK)
        return 0;
    const char *path = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *p = sqlite3_column_text(st, 2);
        if (p && p[0]) path = (const char *)p;
    }
    sqlite3_finalize(st);
    if (!path) return 0; /* in-memory / unnamed */
    struct stat stt;
    if (stat(path, &stt) != 0) return 0;
    long page_size = 0;
    sqlite3_stmt *ps = NULL;
    if (sqlite3_prepare_v2(conn, "PRAGMA page_size", -1, &ps, NULL) == SQLITE_OK) {
        if (sqlite3_step(ps) == SQLITE_ROW) page_size = sqlite3_column_int64(ps, 0);
        sqlite3_finalize(ps);
    }
    if (page_size <= 0) return 0;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    unsigned char hdr[4];
    ssize_t got = (fd >= 0) ? read(fd, hdr, 4) : 0;
    close(fd);
    if (got < 4) return 0;
    long header_page_count = ((long)hdr[0] << 24) | ((long)hdr[1] << 16)
                            | ((long)hdr[2] << 8) | (long)hdr[3];
    if (header_page_count == 0) return 0; /* new/empty */
    long actual_pages = (long)stt.st_size / page_size;
    if (actual_pages < header_page_count) return 1; /* torn-extent corruption */
    return 0;
}

/* PoP: kdb_scratch_tip_sentinel_path @ hermes_cli/kanban_db.py:_scratch_tip_sentinel_path */
char *kdb_scratch_tip_sentinel_path(void)
{
    char *home = kanban_home();
    if (!home) return NULL;
    size_t L = strlen(home) + 1 + sizeof(KDB_SCRATCH_TIP_SENTINEL);
    char *out = malloc(L);
    snprintf(out, L, "%s/%s", home, KDB_SCRATCH_TIP_SENTINEL);
    free(home);
    return out;
}

/* PoP: kdb_scratch_tip_shown @ hermes_cli/kanban_db.py:_scratch_tip_shown */
int kdb_scratch_tip_shown(void)
{
    char *p = kdb_scratch_tip_sentinel_path();
    if (!p) return 0;
    int exists = (access(p, F_OK) == 0);
    free(p);
    return exists;
}

/* PoP: kdb_mark_scratch_tip_shown @ hermes_cli/kanban_db.py:_mark_scratch_tip_shown */
void kdb_mark_scratch_tip_shown(void)
{
    char *p = kdb_scratch_tip_sentinel_path();
    if (!p) return;
    char *home = kanban_home();
    if (home) { mkdirs(home); free(home); } /* best-effort parent */
    FILE *f = fopen(p, "wb");
    if (f) fclose(f);
    free(p);
}

/* PoP: kdb_maybe_emit_scratch_tip @ hermes_cli/kanban_db.py:_maybe_emit_scratch_tip */
void kdb_maybe_emit_scratch_tip(sqlite3 *conn, const char *task_id,
                                const char *workspace_kind)
{
    const char *kind = workspace_kind ? workspace_kind : "scratch";
    if (strcmp(kind, "scratch") != 0) return;
    if (kdb_scratch_tip_shown()) return;
    if (conn && task_id) {
        char payload[512];
        snprintf(payload, sizeof(payload),
                 "{\"message\":%s}", KDB_SCRATCH_TIP_MESSAGE);
        kdb_append_event(conn, task_id, -1, "tip_scratch_workspace", payload);
    }
    kdb_mark_scratch_tip_shown();
}

/* ------------------------------------------------------------------ */
/* corruption recovery: backup + guard                           */
/* ------------------------------------------------------------------ */

/* PoP: kdb_backup_corrupt_db @ hermes_cli/kanban_db.py:_backup_corrupt_db */
/* Copy a corrupt DB (and its WAL/SHM sidecars) to a content-addressed
 * backup inside the DB's own parent dir. The backup basename is derived
 * from the file name + sha256[:16] of the bytes, so repeated quarantine
 * of the same corrupt image reuses one backup instead of N copies.
 * Returns a malloc'd backup path (caller frees) or NULL on copy failure. */
char *kdb_backup_corrupt_db(const char *path)
{
    char cbuf[65536];
    if (!path) return NULL;
    char resolved[PATH_MAX];
    if (logical_norm(path, resolved, sizeof(resolved)) != 0) return NULL;
    char *parent = strdup(resolved);
    char *slash = strrchr(parent, '/');
    if (slash) *slash = 0;
    /* sha256 of the whole file (kanban DBs are small) */
    FILE *h = fopen(resolved, "rb");
    if (!h) { free(parent); return NULL; }
    fseek(h, 0, SEEK_END); long fsz = ftell(h); fseek(h, 0, SEEK_SET);
    char *buf = (fsz > 0 && fsz < 64*1024*1024) ? malloc((size_t)fsz) : NULL;
    size_t got = (buf && fsz > 0) ? fread(buf, 1, (size_t)fsz, h) : 0;
    fclose(h);
    char token[33];
    if (buf && got > 0) {
        unsigned char hash[CRYPTO_SHA256_LEN];
        crypto_sha256((const unsigned char *)buf, got, hash);
        for (int i = 0; i < 16; i++) snprintf(token + i*2, 3, "%02x", hash[i]);
        token[32] = 0;
    } else {
        snprintf(token, sizeof(token), "unknown");
    }
    free(buf);
    const char *base = strrchr(resolved, '/');
    base = base ? base + 1 : resolved;
    char cand[PATH_MAX];
    snprintf(cand, sizeof(cand), "%s/%s.corrupt.%s.bak", parent, base, token);
    free(parent);
    /* defensive: candidate must stay inside the original parent */
    char cand_norm[PATH_MAX];
    if (logical_norm(cand, cand_norm, sizeof(cand_norm)) != 0) return NULL;
    if (strstr(cand_norm, "/..") != NULL) return NULL;
    if (!access(cand, F_OK)) {
        return strdup(cand); /* already quarantined with these exact bytes */
    }
    if (link(resolved, cand) != 0) {
        /* fall back to copy (cross-device / no hardlink support) */
        FILE *src = fopen(resolved, "rb");
        if (!src) return NULL;
        FILE *dst = fopen(cand, "wb");
        if (!dst) { fclose(src); return NULL; }
        char cbuf[65536]; size_t m;
        while ((m = fread(cbuf, 1, sizeof(cbuf), src)) > 0) fwrite(cbuf, 1, m, dst);
        fclose(src); fclose(dst);
    }
    /* sidecars */
    for (int i = 0; i < 2; i++) {
        const char *suf = (i == 0) ? "-wal" : "-shm";
        char side[PATH_MAX];
        snprintf(side, sizeof(side), "%s%s", resolved, suf);
        if (access(side, F_OK) != 0) continue;
        char side_bak[PATH_MAX];
        snprintf(side_bak, sizeof(side_bak), "%s%s", cand, suf);
        FILE *ss = fopen(side, "rb");
        if (!ss) continue;
        FILE *sd = fopen(side_bak, "wb");
        if (!sd) { fclose(ss); continue; }
        size_t mm;
        while ((mm = fread(cbuf, 1, sizeof(cbuf), ss)) > 0) fwrite(cbuf, 1, mm, sd);
        fclose(ss); fclose(sd);
    }
    return strdup(cand);
}

/* PoP: kdb_guard_existing_db_is_healthy @ hermes_cli/kanban_db.py:_guard_existing_db_is_healthy */
/* Run `PRAGMA integrity_check` on an existing non-empty DB file. Opens a
 * throwaway probe so a healthy WAL/hot-journal DB can checkpoint before
 * we call it corrupt. Returns:
 *   0  = healthy / no-op (missing, zero-byte, or already-proven path)
 *   1  = CORRUPT: backup written, *reason_out malloc'd (caller frees)
 *  -1  = transient lock/busy: caller should propagate (no backup made)
 * *reason_out is only set (non-NULL) on return == 1. */
int kdb_guard_existing_db_is_healthy(const char *path, char **reason_out)
{
    if (reason_out) *reason_out = NULL;
    if (!path) return 0;
    char resolved[PATH_MAX];
    if (logical_norm(path, resolved, sizeof(resolved)) != 0) return 0;
    struct stat stt;
    if (stat(resolved, &stt) != 0 || stt.st_size == 0) return 0;
    sqlite3 *probe = NULL;
    int prc = sqlite3_open_v2(resolved, &probe,
                               SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX, NULL);
    if (prc != SQLITE_OK) {
        if (probe) sqlite3_close(probe);
        /* open refusal => treat as transient (let caller see a lock error) */
        return -1;
    }
    char *reason = NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(probe, "PRAGMA integrity_check", -1, &st, NULL) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char *r = sqlite3_column_text(st, 0);
            const char *rs = r ? (const char *)r : "";
            if (rs[0] && strcmp(rs, "ok") != 0) {
                size_t L = strlen(rs) + 64;
                reason = malloc(L);
                snprintf(reason, L, "integrity_check returned: %s", rs);
            }
        }
        sqlite3_finalize(st);
    }
    sqlite3_close(probe);
    if (reason) {
        char *backup = kdb_backup_corrupt_db(resolved);
        free(backup); /* backup path intentionally discarded here */
        if (reason_out) { *reason_out = reason; } else { free(reason); }
        return 1;
    }
    return 0;
}
