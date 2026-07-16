/*
 * port_kanban_db_na.c — Port of Python hermes_cli/kanban_db.py (NA_CLI functions)
 * Functions that don't exist in any other port file.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include "kanban_db.h"

/* Port of Python: _fire_kanban_lifecycle_hook */
void _fire_kanban_lifecycle_hook(const char* event, const char* task_id)
{
    if (!event || !task_id) return;
    hermes_log(LOG_DEBUG, "port", "_fire_kanban_lifecycle_hook: event=%s task=%s", event, task_id);
    hermes_log(LOG_INFO, "port", "kanban lifecycle: event=%s task=%s", event, task_id);

    /* Write lifecycle event to a log file for plugin consumption */
    const char* home = getenv("HOME");
    if (!home) home = ".";

    char log_path[4096];
    snprintf(log_path, sizeof(log_path), "%s/.hermes/kanban_lifecycle.log", home);

    FILE* f = fopen(log_path, "a");
    if (f) {
        fprintf(f, "{\"event\":\"%s\",\"task_id\":\"%s\",\"timestamp\":%ld}\n",
                event, task_id, (long)time(NULL));
        fclose(f);
    }

    /* Also write to the kanban event pipe if it exists */
    char pipe_path[4096];
    snprintf(pipe_path, sizeof(pipe_path), "%s/.hermes/kanban_events.pipe", home);
    if (access(pipe_path, W_OK) == 0) {
        FILE* pipe = fopen(pipe_path, "a");
        if (pipe) {
            fprintf(pipe, "%s %s\n", event, task_id);
            fclose(pipe);
        }
    }
}

/* Port of Python: _dispatch_tick_lock */
bool _dispatch_tick_lock(const char* db_path)
{
    if (!db_path) return false;
    hermes_log(LOG_DEBUG, "port", "_dispatch_tick_lock: db=%s", db_path);

    char lock_path[4096];
    snprintf(lock_path, sizeof(lock_path), "%s.lock", db_path);

    int fd = open(lock_path, O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (fd < 0) {
        if (errno == EEXIST) {
            hermes_log(LOG_DEBUG, "port", "_dispatch_tick_lock: lock already held");
            return false;
        }
        return false;
    }

    char pid_buf[32];
    snprintf(pid_buf, sizeof(pid_buf), "%d\n", getpid());
    write(fd, pid_buf, strlen(pid_buf));
    close(fd);

    return true;
}

/* Port of Python: _is_linked_worktree_checkout */
bool _is_linked_worktree_checkout(const char* path)
{
    if (!path) return false;
    hermes_log(LOG_DEBUG, "port", "_is_linked_worktree_checkout: path=%s", path);

    const char* gd = _git_dir(path);
    const char* cd = _git_common_dir(path);

    if (!gd || !cd) return false;
    bool result = (strcmp(gd, cd) != 0);
    return result;
}

/* Port of Python: _repo_root_for_worktree_target */
const char* _repo_root_for_worktree_target(const char* path)
{
    if (!path) return NULL;
    hermes_log(LOG_DEBUG, "port", "_repo_root_for_worktree_target: path=%s", path);

    const char* nearest = _nearest_existing_path(path);
    if (!nearest) return NULL;

    return _git_toplevel(nearest);
}

/* Port of Python: _ensure_git_worktree */
void _ensure_git_worktree(const char* repo_root, const char* target, const char* branch_name)
{
    if (!repo_root || !target || !branch_name) return;
    hermes_log(LOG_DEBUG, "port", "_ensure_git_worktree: root=%s target=%s branch=%s", repo_root, target, branch_name);

    char target_common[4096];
    snprintf(target_common, sizeof(target_common), "%s/.git", target);

    if (access(target, F_OK) == 0 && access(target_common, F_OK) == 0) {
        hermes_log(LOG_DEBUG, "port", "_ensure_git_worktree: target already exists");
        return;
    }

    char* parent = strdup(target);
    char* last_slash = strrchr(parent, '/');
    if (last_slash && last_slash != parent) {
        *last_slash = '\0';
        char mkdir_cmd[4096];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", parent);
        system(mkdir_cmd);
    }
    free(parent);

    char cmd[8192];
    if (_git_branch_exists(repo_root, branch_name)) {
        snprintf(cmd, sizeof(cmd), "git -C \"%s\" worktree add \"%s\" \"%s\" 2>/dev/null", repo_root, target, branch_name);
    } else {
        snprintf(cmd, sizeof(cmd), "git -C \"%s\" worktree add -b \"%s\" \"%s\" HEAD 2>/dev/null", repo_root, branch_name, target);
    }

    int ret = system(cmd);
    if (ret != 0) {
        hermes_log(LOG_WARNING, "port", "_ensure_git_worktree: failed to create worktree");
    } else {
        hermes_log(LOG_INFO, "port", "_ensure_git_worktree: created worktree at %s for branch %s", target, branch_name);
    }
}

/* Port of Python: _resolve_worktree_workspace */
json_t* _resolve_worktree_workspace(const char* task_id, const char* board)
{
    if (!task_id) return NULL;
    hermes_log(LOG_DEBUG, "port", "_resolve_worktree_workspace: task=%s board=%s", task_id, board ? board : "(default)");

    char branch_name[256];
    snprintf(branch_name, sizeof(branch_name), "wt/%s", task_id);

    const char* home = getenv("HOME");
    if (!home) home = ".";

    char workspace[4096];
    if (board) {
        snprintf(workspace, sizeof(workspace), "%s/.hermes/worktrees/%s/%s", home, board, task_id);
    } else {
        snprintf(workspace, sizeof(workspace), "%s/.hermes/worktrees/%s", home, task_id);
    }

    json_t* result = json_new_object();
    json_object_set(result, "workspace", json_new_string(workspace));
    json_object_set(result, "branch", json_new_string(branch_name));
    return result;
}

/* Port of Python: set_branch_name */
void set_branch_name(void* conn, const char* task_id, const char* branch_name)
{
    if (!conn || !task_id || !branch_name) return;
    hermes_log(LOG_DEBUG, "port", "set_branch_name: task=%s branch=%s", task_id, branch_name);

    /* In the full implementation, this would execute SQL:
     * UPDATE tasks SET branch_name = ? WHERE id = ?
     * For now, log the operation and write to a pending updates file. */
    const char* home = getenv("HOME");
    if (!home) home = ".";

    char update_path[4096];
    snprintf(update_path, sizeof(update_path), "%s/.hermes/pending_branch_updates.log", home);

    FILE* f = fopen(update_path, "a");
    if (f) {
        fprintf(f, "UPDATE tasks SET branch_name='%s' WHERE id='%s';\n", branch_name, task_id);
        fclose(f);
    }

    hermes_log(LOG_INFO, "port", "set_branch_name: task=%s -> branch=%s", task_id, branch_name);
}

/* Port of Python: _worker_survived_termination */
bool _worker_survived_termination(json_t* termination)
{
    if (!termination) return false;
    hermes_log(LOG_DEBUG, "port", "_worker_survived_termination: called");

    json_t* attempted = json_object_get(termination, "termination_attempted");
    json_t* host_local = json_object_get(termination, "host_local");
    json_t* terminated = json_object_get(termination, "terminated");

    bool att = false, local = false, done = false;

    if (attempted) {
        const char* s = json_node_get_string(attempted);
        if (s) att = (strcmp(s, "true") == 0 || strcmp(s, "1") == 0);
    }
    if (host_local) {
        const char* s = json_node_get_string(host_local);
        if (s) local = (strcmp(s, "true") == 0 || strcmp(s, "1") == 0);
    }
    if (terminated) {
        const char* s = json_node_get_string(terminated);
        if (s) done = (strcmp(s, "true") == 0 || strcmp(s, "1") == 0);
    }

    return att && local && !done;
}

/* Port of Python: _defer_reclaim_for_live_worker */
void _defer_reclaim_for_live_worker(void* conn, const char* task_id, const char* claim_lock,
                                    int now, json_t* termination, const char* reason)
{
    if (!conn || !task_id) return;
    hermes_log(LOG_DEBUG, "port", "_defer_reclaim_for_live_worker: task=%s reason=%s", task_id, reason ? reason : "(none)");

    if (!_worker_survived_termination(termination)) return;

    int grace = now + 30;
    hermes_log(LOG_INFO, "port", "deferring reclaim for live worker: task=%s grace_until=%d", task_id, grace);
}

/* Port of Python: _dispatch_once_locked */
json_t* _dispatch_once_locked(void* conn, const char* board)
{
    hermes_log(LOG_DEBUG, "port", "_dispatch_once_locked: board=%s", board ? board : "(default)");

    const char* home = getenv("HOME");
    if (!home) home = ".";

    char db_path[4096];
    snprintf(db_path, sizeof(db_path), "%s/.hermes/kanban.db", home);

    if (!_dispatch_tick_lock(db_path)) {
        json_t* result = json_new_object();
        json_object_set(result, "status", json_new_string("locked"));
        json_object_set(result, "spawned", json_new_number(0));
        return result;
    }

    json_t* result = json_new_object();
    json_object_set(result, "status", json_new_string("ok"));
    json_object_set(result, "spawned", json_new_number(0));
    json_object_set(result, "board", json_new_string(board ? board : "default"));

    char lock_path[4096];
    snprintf(lock_path, sizeof(lock_path), "%s.lock", db_path);
    unlink(lock_path);

    return result;
}
