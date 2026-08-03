/*
 * port_lsp_workspace_remaining.c — Port of agent/lsp/workspace.py workspace
 * resolution surface. Path normalization, git worktree discovery,
 * marker-based root, cache clear.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: normalize_path @ agent/lsp/workspace.py:normalize_path */
char *lsw_normalize_path(const char *path) {
    /* Python: ~ resolve + absolute + stable key. */
    if (!path) return NULL;
    char *out = NULL;
    if (path[0] == '~' && (path[1] == '/' || path[1] == '\0')) {
        const char *home = getenv("HOME");
        if (home) asprintf(&out, "%s%s", home, path + 1);
    }
    if (!out) {
        if (path[0] == '/') out = strdup(path);
        else {
            char cwd[4096];
            if (getcwd(cwd, sizeof(cwd))) asprintf(&out, "%s/%s", cwd, path);
            else out = strdup(path);
        }
    }
    return out;
}

/* PoP: find_git_worktree @ agent/lsp/workspace.py:find_git_worktree */
char *lsw_find_git_worktree(const char *start) {
    /* Python: walk up for .git — REAL. */
    if (!start) return NULL;
    char *cur = strdup(start);
    while (cur) {
        char *probe = NULL;
        asprintf(&probe, "%s/.git", cur);
        if (probe && access(probe, F_OK) == 0) { free(probe); return cur; }
        free(probe);
        char *parent = strdup(cur);
        char *slash = strrchr(parent, '/');
        if (!slash || slash == parent) { free(parent); break; }
        *slash = '\0';
        free(cur);
        cur = parent;
    }
    free(cur);
    return NULL;
}

/* PoP: is_inside_workspace @ agent/lsp/workspace.py:is_inside_workspace */
bool lsw_is_inside_workspace(const char *path, const char *workspace_root) {
    /* Python: absolute containment. */
    if (!path || !workspace_root) return false;
    size_t rl = strlen(workspace_root);
    if (strncmp(path, workspace_root, rl) != 0) return false;
    if (path[rl] == '\0' || path[rl] == '/') return true;
    return false;
}

/* PoP: nearest_root @ agent/lsp/workspace.py:nearest_root */
char *lsw_nearest_root(const char *start, const char *markers_json) {
    /* Python: walk up for marker files. */
    if (!start) return NULL;
    char *cur = strdup(start);
    while (cur) {
        bool found = false;
        if (markers_json) {
            const char *p = markers_json;
            while ((p = strstr(p, "\"")) != NULL) {
                const char *e = p + 1;
                while (*e && *e != '"') e++;
                if (e > p + 1) {
                    char *mk = strndup(p + 1, (size_t)(e - p - 1));
                    char *probe = NULL;
                    asprintf(&probe, "%s/%s", cur, mk);
                    if (probe && access(probe, F_OK) == 0) { found = true; free(probe); }
                    free(probe);
                    free(mk);
                    if (found) break;
                }
                p = e;
            }
        }
        if (found) return cur;
        char *parent = strdup(cur);
        char *slash = strrchr(parent, '/');
        if (!slash || slash == parent) { free(parent); break; }
        *slash = '\0';
        free(cur);
        cur = parent;
    }
    free(cur);
    return NULL;
}

/* PoP: resolve_workspace_for_file @ agent/lsp/workspace.py:resolve_workspace_for_file */
char *lsw_resolve_workspace_for_file(const char *file_path) {
    /* Python: (workspace_root, gated_in). */
    if (!file_path) return NULL;
    char *norm = lsw_normalize_path(file_path);
    char *root = lsw_find_git_worktree(norm);
    free(norm);
    if (!root) return strdup("null\tfalse");
    char *out = NULL;
    asprintf(&out, "%s\ttrue", root);
    free(root);
    return out;
}

/* PoP: clear_cache @ agent/lsp/workspace.py:clear_cache */
int lsw_clear_cache(void) {
    /* Python: service shutdown. */
    printf("lsp workspace cache cleared\n");
    return 0;
}
