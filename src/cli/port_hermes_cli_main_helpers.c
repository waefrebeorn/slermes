/*
 * port_hermes_cli_main_helpers.c — C port of selected helpers from
 * hermes_cli/main.py.
 *
 * Only dependency-light, faithful re-implementations are ported here.
 * Functions that require the config/auth/network subsystems are deferred.
 */

#include "hermes_logger.h"
#include "libjson/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <sys/stat.h>

/* ------------------------------------------------------------------ */
/* Path concatenation helper (no allocation beyond the buffer).        */
/* ------------------------------------------------------------------ */
static void join_path(char *out, size_t sz, const char *a, const char *b)
{
    snprintf(out, sz, "%s/%s", a, b);
}

/* PoP: _read_packed_ref @ hermes_cli/main.py:_read_packed_ref
 * Look up a ref in .git/packed-refs without spawning git.
 * Returns malloc'd sha, or NULL. Caller frees. */
char *read_packed_ref(const char *common_dir, const char *ref)
{
    if (!common_dir || !ref) return NULL;
    char path[PATH_MAX];
    join_path(path, sizeof(path), common_dir, "packed-refs");
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char line[1024];
    char *result = NULL;
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n-1]=='\n'||line[n-1]=='\r')) line[--n]=0;
        if (!line[0] || line[0]=='#' || line[0]=='^') continue;
        char *sp = strchr(line, ' ');
        if (!sp) continue;
        *sp = '\0';
        char *r = sp + 1;
        while (*r == ' ') r++;
        if (strcmp(r, ref) == 0) {
            result = strdup(line);
            break;
        }
    }
    fclose(f);
    return result;
}

/* PoP: _read_git_revision_fingerprint @ hermes_cli/main.py:_read_git_revision_fingerprint
 * Cheap checkout fingerprint without spawning git.
 * Returns malloc'd "git:<ref>:<sha>" string, or NULL. Caller frees. */
char *read_git_revision_fingerprint(const char *repo_root)
{
    if (!repo_root) return NULL;
    char git_dir[PATH_MAX];
    join_path(git_dir, sizeof(git_dir), repo_root, ".git");

    struct stat st;
    /* .git may be a file (gitdir: <path>) for worktrees/submodules. */
    if (stat(git_dir, &st) == 0 && S_ISREG(st.st_mode)) {
        FILE *f = fopen(git_dir, "r");
        if (f) {
            char line[1024];
            while (fgets(line, sizeof(line), f)) {
                char *colon = strchr(line, ':');
                if (colon && strncmp(line, "gitdir", 6) == 0) {
                    *colon = '\0';
                    char *val = colon + 1;
                    while (*val==' '||*val=='\t') val++;
                    size_t n = strlen(val);
                    while (n>0 && (val[n-1]=='\n'||val[n-1]=='\r')) val[--n]=0;
                    if (val[0]) { join_path(git_dir, sizeof(git_dir), repo_root, val); break; }
                }
            }
            fclose(f);
        }
    }

    /* Resolve commondir (worktree refs live in main repo gitdir). */
    char common_dir[PATH_MAX];
    snprintf(common_dir, sizeof(common_dir), "%s", git_dir);
    char commondir_path[PATH_MAX];
    join_path(commondir_path, sizeof(commondir_path), git_dir, "commondir");
    FILE *cf = fopen(commondir_path, "r");
    if (cf) {
        char line[1024];
        if (fgets(line, sizeof(line), cf)) {
            size_t n = strlen(line);
            while (n>0 && (line[n-1]=='\n'||line[n-1]=='\r')) line[--n]=0;
            if (line[0]) join_path(common_dir, sizeof(common_dir), git_dir, line);
        }
        fclose(cf);
    }

    /* Read HEAD. */
    char head_path[PATH_MAX];
    join_path(head_path, sizeof(head_path), git_dir, "HEAD");
    FILE *hf = fopen(head_path, "r");
    if (!hf) return NULL;
    char head[512];
    if (!fgets(head, sizeof(head), hf)) { fclose(hf); return NULL; }
    fclose(hf);
    size_t n = strlen(head);
    while (n>0 && (head[n-1]=='\n'||head[n-1]=='\r')) head[--n]=0;

    if (strncmp(head, "ref:", 4) == 0) {
        char *ref = head + 4;
        while (*ref==' '||*ref=='\t') ref++;
        /* Loose ref: check git_dir then common_dir */
        char ref_file[PATH_MAX];
        for (int i = 0; i < 2; i++) {
            const char *base = (i==0) ? git_dir : common_dir;
            join_path(ref_file, sizeof(ref_file), base, ref);
            FILE *rf = fopen(ref_file, "r");
            if (rf) {
                char sha[256];
                if (fgets(sha, sizeof(sha), rf)) {
                    size_t m = strlen(sha);
                    while (m>0 && (sha[m-1]=='\n'||sha[m-1]=='\r')) sha[--m]=0;
                    char *out = malloc(strlen(ref)+strlen(sha)+8);
                    sprintf(out, "git:%s:%s", ref, sha);
                    fclose(rf);
                    return out;
                }
                fclose(rf);
            }
        }
        /* Packed ref */
        char *packed = read_packed_ref(common_dir, ref);
        if (packed) {
            char *out = malloc(strlen(ref)+strlen(packed)+8);
            sprintf(out, "git:%s:%s", ref, packed);
            free(packed);
            return out;
        }
        char *out = malloc(strlen(ref)+16);
        sprintf(out, "git:%s:unresolved", ref);
        return out;
    }
    char *out = malloc(strlen(head)+16);
    sprintf(out, "git:HEAD:%s", head);
    return out;
}

/* PoP: _relative_time @ hermes_cli/main.py:_relative_time */
char *relative_time(long ts)
{
    if (!ts) return strdup("?");
    long delta = (long)time(NULL) - ts;
    if (delta < 60) return strdup("just now");
    if (delta < 3600) { char b[32]; snprintf(b,sizeof(b),"%ldm ago",delta/60); return strdup(b); }
    if (delta < 86400) { char b[32]; snprintf(b,sizeof(b),"%ldh ago",delta/3600); return strdup(b); }
    if (delta < 172800) return strdup("yesterday");
    if (delta < 604800) { char b[32]; snprintf(b,sizeof(b),"%ldd ago",delta/86400); return strdup(b); }
    /* fall back to date string */
    time_t t = (time_t)ts;
    struct tm *tm = localtime(&t);
    char b[32];
    strftime(b, sizeof(b), "%Y-%m-%d", tm);
    return strdup(b);
}

/* PoP: _workspace_root @ hermes_cli/main.py:_workspace_root
 * Returns malloc'd workspace root path. Caller frees. */
char *workspace_root(const char *dir)
{
    if (!dir) return NULL;
    char pkg_json[PATH_MAX], lock_here[PATH_MAX], lock_parent[PATH_MAX];
    join_path(pkg_json, sizeof(pkg_json), dir, "package.json");
    join_path(lock_here, sizeof(lock_here), dir, "package-lock.json");
    char parent[PATH_MAX];
    snprintf(parent, sizeof(parent), "%s", dir);
    char *slash = strrchr(parent, '/');
    if (slash) *slash = '\0';
    join_path(lock_parent, sizeof(lock_parent), parent, "package-lock.json");

    struct stat a, b1, b2;
    if (stat(pkg_json, &a)==0 && !S_ISDIR(a.st_mode) &&
        stat(lock_here, &b1)!=0 &&
        stat(lock_parent, &b2)==0) {
        /* parent is the workspace root */
        char *rslash = strrchr(parent, '/');
        if (rslash) return strdup(parent);
    }
    return strdup(dir);
}

/* PoP: _read_cgroup_memory_limit @ hermes_cli/main.py:_read_cgroup_memory_limit
 * Returns container memory limit in bytes, or -1 if unconstrained/unavailable. */
long read_cgroup_memory_limit(void)
{
    const char *candidates[2] = {
        "/sys/fs/cgroup/memory.max",            /* cgroup v2 */
        "/sys/fs/cgroup/memory/memory.limit_in_bytes", /* cgroup v1 */
    };
    for (int i = 0; i < 2; i++) {
        FILE *f = fopen(candidates[i], "r");
        if (!f) continue;
        char raw[64];
        if (!fgets(raw, sizeof(raw), f)) { fclose(f); continue; }
        fclose(f);
        size_t n = strlen(raw);
        while (n>0 && (raw[n-1]=='\n'||raw[n-1]=='\r')) raw[--n]=0;
        if (strcmp(raw, "max") == 0) return -1;
        if (!raw[0]) continue; /* empty file: try next candidate */
        char *end = NULL;
        long limit = strtol(raw, &end, 10);
        if (end == raw || *end != '\0') continue;
        if (limit <= 0) continue;
        if (limit >= (1L << 50)) return -1; /* effectively unlimited */
        return limit;
    }
    return -1;
}

/* PoP: _read_tui_active_session_file @ hermes_cli/main.py:_read_tui_active_session_file
 * Reads a JSON file and extracts the "session_id" field. Returns malloc'd
 * session id, or NULL. Caller frees. */
char *read_tui_active_session_file(const char *path)
{
    if (!path) return NULL;
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char buf[8192];
    size_t total = 0;
    char *blob = NULL;
    while (fgets(buf, sizeof(buf), f)) {
        size_t n = strlen(buf);
        char *nb = realloc(blob, total + n + 1);
        if (!nb) { free(blob); fclose(f); return NULL; }
        blob = nb;
        memcpy(blob + total, buf, n);
        total += n;
        blob[total] = '\0';
    }
    fclose(f);
    if (!blob) return NULL;
    json_t *root = json_parse(blob, NULL);
    free(blob);
    if (!root || root->type != JSON_OBJECT) { json_free(root); return NULL; }
    json_t *sid = json_obj_get(root, "session_id");
    char *result = NULL;
    if (sid && sid->type == JSON_STRING && sid->str_val && sid->str_val[0]) {
        char *s = sid->str_val;
        while (*s==' '||*s=='\t') s++;
        if (*s) result = strdup(s);
    }
    json_free(root);
    return result;
}
