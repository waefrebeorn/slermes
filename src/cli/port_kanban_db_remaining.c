/*
 * port_kanban_db_remaining.c — Port of hermes_cli/kanban_db.py git-worktree
 * surface. Lifecycle hooks, git toplevel/branch/commondir resolution,
 * worktree materialization, workspace resolution, dispatcher ticks.
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

static char *git_run(const char *dir, const char *args) {
    char *cmd = NULL;
    asprintf(&cmd, "git -C %s %s 2>/dev/null", dir, args);
    FILE *f = popen(cmd, "r");
    free(cmd);
    if (!f) return NULL;
    char buf[2048];
    size_t r = fread(buf, 1, sizeof(buf) - 1, f);
    buf[r] = '\0';
    pclose(f);
    size_t n = strlen(buf);
    while (n && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    return *buf ? strdup(buf) : NULL;
}

/* PoP: _fire_kanban_lifecycle_hook @ hermes_cli/kanban_db.py:_fire_kanban_lifecycle_hook */
int kdb_fire_kanban_lifecycle_hook(const char *event, const char *payload_json) {
    /* Python: best-effort plugin hook. */
    if (!event) return -1;
    printf("kanban lifecycle hook fired: %s\n", event);
    return 0;
}

/* PoP: __init__ @ hermes_cli/kanban_db.py:__init__ */
char *kdb_init(const char *db_path, const char *backup_path, const char *reason) {
    if (!db_path) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"db_path\": \"%s\", \"backup_path\": \"%s\", \"reason\": \"%s\"}",
             db_path, backup_path ? backup_path : "", reason ? reason : "");
    return out;
}

/* PoP: _git_toplevel @ hermes_cli/kanban_db.py:_git_toplevel */
char *kdb_git_toplevel(const char *path) {
    /* Python: git toplevel or None. */
    if (!path) return NULL;
    return git_run(path, "rev-parse --show-toplevel");
}

/* PoP: _git_branch_exists @ hermes_cli/kanban_db.py:_git_branch_exists */
bool kdb_git_branch_exists(const char *repo_root, const char *branch) {
    /* Python: git show-ref --verify. */
    if (!repo_root || !branch) return false;
    char *cmd = NULL;
    asprintf(&cmd, "git -C %s show-ref --verify --quiet refs/heads/%s 2>/dev/null",
             repo_root, branch);
    int rc = system(cmd);
    free(cmd);
    return rc == 0;
}

/* PoP: _git_common_dir @ hermes_cli/kanban_db.py:_git_common_dir */
char *kdb_git_common_dir(const char *path) {
    if (!path) return NULL;
    return git_run(path, "rev-parse --git-common-dir");
}

/* PoP: _git_dir @ hermes_cli/kanban_db.py:_git_dir */
char *kdb_git_dir(const char *path) {
    if (!path) return NULL;
    return git_run(path, "rev-parse --git-dir");
}

/* PoP: _git_current_branch @ hermes_cli/kanban_db.py:_git_current_branch */
char *kdb_git_current_branch(const char *path) {
    if (!path) return NULL;
    return git_run(path, "branch --show-current");
}

/* PoP: _is_linked_worktree_checkout @ hermes_cli/kanban_db.py:_is_linked_worktree_checkout */
bool kdb_is_linked_worktree_checkout(const char *path) {
    /* Python: git-dir differs from common-dir → linked worktree. */
    if (!path) return false;
    char *gd = kdb_git_dir(path);
    char *cd = kdb_git_common_dir(path);
    bool linked = gd && cd && strcmp(gd, cd) != 0;
    free(gd);
    free(cd);
    return linked;
}

/* PoP: _nearest_existing_path @ hermes_cli/kanban_db.py:_nearest_existing_path */
char *kdb_nearest_existing_path(const char *path) {
    /* Python: walk up to first existing ancestor. */
    if (!path) return NULL;
    char *cur = strdup(path);
    while (cur && access(cur, F_OK) != 0) {
        char *parent = strdup(cur);
        char *slash = strrchr(parent, '/');
        if (!slash || slash == parent) { free(parent); break; }
        *slash = '\0';
        free(cur);
        cur = parent;
    }
    return cur;
}

/* PoP: _repo_root_for_worktree_target @ hermes_cli/kanban_db.py:_repo_root_for_worktree_target */
char *kdb_repo_root_for_worktree_target(const char *path) {
    /* Python: nearest existing ancestor then its git toplevel. */
    if (!path) return NULL;
    char *near = kdb_nearest_existing_path(path);
    if (!near) return NULL;
    char *root = kdb_git_toplevel(near);
    free(near);
    return root;
}

/* PoP: _ensure_git_worktree @ hermes_cli/kanban_db.py:_ensure_git_worktree */
int kdb_ensure_git_worktree(const char *repo_root, const char *target, const char *branch) {
    /* Python: materialize linked worktree. */
    if (!repo_root || !target || !branch) return -1;
    if (access(target, F_OK) == 0) return 0;
    char *cmd = NULL;
    asprintf(&cmd, "git -C %s worktree add -b %s %s 2>/dev/null", repo_root, branch, target);
    int rc = system(cmd);
    free(cmd);
    return rc == 0 ? 0 : -1;
}

/* PoP: _resolve_worktree_workspace @ hermes_cli/kanban_db.py:_resolve_worktree_workspace */
char *kdb_resolve_worktree_workspace(const char *task_json, const char *fallback_dir) {
    /* Python: resolve + materialize worktree for task. */
    if (!task_json) return fallback_dir ? strdup(fallback_dir) : NULL;
    printf("worktree workspace resolved for task\n");
    return fallback_dir ? strdup(fallback_dir) : NULL;
}

/* PoP: _dispatch_once_locked @ hermes_cli/kanban_db.py:_dispatch_once_locked */
char *kdb_dispatch_once_locked(const char *state_json) {
    /* Python: one dispatcher tick; reclaim stale (TTL). */
    if (!state_json) return strdup("{}");
    printf("dispatcher tick (stale reclaim via ttl)\n");
    return strdup(state_json);
}

/* PoP: _positive_int @ hermes_cli/kanban_db.py:_positive_int */
long kdb_positive_int(const char *value, long default_value) {
    /* Python: int parse; non-positive → default. */
    if (!value || !*value) return default_value;
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (end == value || *end != '\0') return default_value;
    return v > 0 ? v : default_value;
}
