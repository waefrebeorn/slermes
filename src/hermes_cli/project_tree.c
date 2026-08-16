/*
 * port_project_tree.c — pure path/lane-id helpers ported from
 * tui_gateway/project_tree.py. Self-contained; no god header. Only the pure
 * leaf helpers are ported (the build_tree orchestration is stateful/integration
 * work and belongs in its own pass). Faithful to the Python regex + segment
 * semantics so emitted ids stay byte-compatible with the renderer.
 *
 *   _segments            -> project_tree_segments
 *   base_name            -> project_tree_base_name
 *   kanban_worktree_dir  -> project_tree_kanban_worktree_dir
 *   _is_path_under       -> project_tree_is_path_under
 *   _with_base_name      -> project_tree_with_base_name
 *   _branch_lane_id      -> project_tree_branch_lane_id
 *   _kanban_lane_id      -> project_tree_kanban_lane_id
 */

#include "project_tree_helpers.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* Split a path on / or \, drop trailing separators, drop empties.
 * (PoP: project_tree._segments) Returns malloc'd array + *out_n; free with
 * project_tree_free_segments. */
char **project_tree_segments(const char *path, int *out_n)
{
    *out_n = 0;
    int cap = 8, n = 0;
    char **out = calloc((size_t)cap, sizeof(char *));
    if (!path || !*path) return out;

    /* strip trailing separators */
    size_t L = strlen(path);
    while (L > 0 && (path[L - 1] == '/' || path[L - 1] == '\\'))
        L--;
    char *work = malloc(L + 1);
    memcpy(work, path, L);
    work[L] = '\0';

    size_t start = 0;
    for (size_t i = 0; i <= L; i++) {
        if (i == L || work[i] == '/' || work[i] == '\\') {
            if (i > start) {
                size_t len = i - start;
                if (n >= cap) { cap *= 2; out = realloc(out, (size_t)cap * sizeof(char *)); }
                out[n] = malloc(len + 1);
                memcpy(out[n], work + start, len);
                out[n][len] = '\0';
                n++;
            }
            start = i + 1;
        }
    }
    free(work);
    *out_n = n;
    return out;
}

void project_tree_free_segments(char **segs, int n)
{
    if (!segs) return;
    for (int i = 0; i < n; i++) free(segs[i]);
    free(segs);
}

/* Last path segment, or "" when empty. (PoP: project_tree.base_name) */
void project_tree_base_name(const char *path, char *out, size_t out_cap)
{
    if (out && out_cap) out[0] = '\0';
    int n = 0;
    char **segs = project_tree_segments(path, &n);
    if (n > 0 && out && out_cap) {
        strncpy(out, segs[n - 1], out_cap - 1);
        out[out_cap - 1] = '\0';
    }
    project_tree_free_segments(segs, n);
}

/* If path is a .../.worktrees/t_<hex> dir, return the "<repo>/.worktrees" dir
 * (malloc'd), else NULL. (PoP: project_tree.kanban_worktree_dir) */
char *project_tree_kanban_worktree_dir(const char *path)
{
    if (!path || !*path) return NULL;
    /* regex: ^(.*[/\\].worktrees)[/\\]t_[0-9a-f]+[/\\]?$ */
    /* find the marker ".worktrees" */
    const char *wt = NULL;
    for (size_t i = 0; path[i]; i++) {
        if (path[i] == '.' && strncmp(path + i, ".worktrees", 10) == 0) {
            wt = path + i;
            break;
        }
    }
    if (!wt) return NULL;
    /* before the marker there must be a separator (so repo_root is the parent) */
    if (wt == path) return NULL; /* ".worktrees" with no leading separator/parent */
    if (wt[-1] != '/' && wt[-1] != '\\') return NULL;
    const char *after = wt + 10; /* past ".worktrees" */
    if (*after != '/' && *after != '\\') return NULL;
    after++; /* past the separator */
    /* after must be t_<hex> then optional trailing separator and end */
    if (after[0] != 't' || after[1] != '_') return NULL;
    const char *p = after + 2;
    if (!*p) return NULL;
    while (*p && *p != '/' && *p != '\\') {
        if (!isxdigit((unsigned char)*p)) return NULL;
        p++;
    }
    /* allow a single trailing separator, then end */
    if (*p == '/' || *p == '\\') {
        p++;
        if (*p) return NULL;
    } else if (*p) {
        return NULL;
    }
    /* matched: return everything up to (and including) ".worktrees" */
    size_t prefix_len = (size_t)(wt + 10 - path); /* includes ".worktrees" */
    char *res = malloc(prefix_len + 1);
    memcpy(res, path, prefix_len);
    res[prefix_len] = '\0';
    return res;
}

/* True when target == folder or target is nested under folder (segment-wise).
 * (PoP: project_tree._is_path_under) */
int project_tree_is_path_under(const char *folder, const char *target)
{
    if (!folder || !target) return 0;
    int nf = 0, nt = 0;
    char **f = project_tree_segments(folder, &nf);
    char **t = project_tree_segments(target, &nt);
    int r = 0;
    if (nf > 0 && nf <= nt) {
        r = 1;
        for (int i = 0; i < nf; i++) {
            if (strcmp(f[i], t[i]) != 0) { r = 0; break; }
        }
    }
    project_tree_free_segments(f, nf);
    project_tree_free_segments(t, nt);
    return r;
}

/* Replace the last path segment with `name`. (PoP: project_tree._with_base_name)
 * Returns malloc'd string; caller frees. */
char *project_tree_with_base_name(const char *path, const char *name)
{
    if (!path) return NULL;
    size_t L = strlen(path);
    while (L > 0 && (path[L - 1] == '/' || path[L - 1] == '\\'))
        L--;
    if (L == 0) return strdup("");
    /* find last separator in [0, L) */
    int last = -1;
    for (int i = (int)L - 1; i >= 0; i--) {
        if (path[i] == '/' || path[i] == '\\') { last = i; break; }
    }
    const char *n = name ? name : "";
    size_t nl = strlen(n);
    size_t reslen = (size_t)last + nl + 1;
    char *res = malloc(reslen + 1);
    memcpy(res, path, (size_t)(last + 1));
    memcpy(res + last + 1, n, nl);
    res[reslen] = '\0';
    return res;
}

/* f"{repo_root}::branch::{branch.strip()}". (PoP: project_tree._branch_lane_id)
 * Returns malloc'd string; caller frees. */
char *project_tree_branch_lane_id(const char *repo_root, const char *branch)
{
    const char *b = branch ? branch : "";
    /* strip leading/trailing whitespace from branch */
    while (*b == ' ' || *b == '\t') b++;
    size_t bl = strlen(b);
    while (bl > 0 && (b[bl - 1] == ' ' || b[bl - 1] == '\t')) bl--;
    const char *rr = repo_root ? repo_root : "";
    size_t len = strlen(rr) + 11 + bl; /* "::branch::" = 10 + nul... +1 */
    char *res = malloc(len + 1);
    snprintf(res, len + 1, "%s::branch::%.*s", rr, (int)bl, b);
    return res;
}

/* f"{repo_root}::kanban". (PoP: project_tree._kanban_lane_id)
 * Returns malloc'd string; caller frees. */
char *project_tree_kanban_lane_id(const char *repo_root)
{
    const char *rr = repo_root ? repo_root : "";
    size_t len = strlen(rr) + 9; /* "::kanban" = 8 + nul */
    char *res = malloc(len + 1);
    snprintf(res, len + 1, "%s::kanban", rr);
    return res;
}
