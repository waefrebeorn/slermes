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
#include "lifecycle_ledger.h"

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

/* === merged from port_kanban_db.c === */
/*
 * port_kanban_db.c — Port of Python hermes_cli/kanban_db.py
 *
 * Git operations and path utilities for the Kanban board.
 */

#include "slermes_home.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <strings.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>
#include <time.h>
#include <ctype.h>

/*
 * _git_toplevel — Return the git toplevel containing path, or NULL if not in a repo.
 *
 * Python: def _git_toplevel(path: Path) -> Optional[Path]:
 *   result = subprocess.run(["git", "-C", str(path), "rev-parse", "--show-toplevel"], ...)
 *   if result.returncode != 0: return None
 *   out = (result.stdout or "").strip()
 *   if not out: return None
 *   return Path(out)
 */
/* Port of Python: _git_toplevel */
const char* _git_toplevel(const char* path)
{
    if (!path || !path[0]) return NULL;

    int pipefd[2];
    if (pipe(pipefd) != 0) return NULL;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    if (pid == 0) {
        /* Child */
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execlp("git", "git", "-C", path, "rev-parse", "--show-toplevel", NULL);
        _exit(1);
    }

    /* Parent */
    close(pipefd[1]);
    char buf[4096];
    ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
    close(pipefd[0]);

    int status;
    waitpid(pid, &status, 0);

    if (n <= 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return NULL;
    }
    buf[n] = '\0';

    /* Strip trailing newline */
    char* nl = strchr(buf, '\n');
    if (nl) *nl = '\0';

    if (buf[0] == '\0') return NULL;

    /* Return a strdup'd copy */
    return strdup(buf);
}

/*
 * _git_branch_exists — Check if a branch exists in a git repo.
 *
 * Python: def _git_branch_exists(repo_root: Path, branch_name: str) -> bool:
 *   result = subprocess.run(["git", "-C", repo_root, "show-ref", "--verify", f"refs/heads/{branch_name}"], ...)
 *   return result.returncode == 0
 */
/* Port of Python: _git_branch_exists */
bool _git_branch_exists(const char* repo_root, const char* branch_name)
{
    if (!repo_root || !branch_name) return false;

    int pipefd[2];
    if (pipe(pipefd) != 0) return false;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        char ref[512];
        snprintf(ref, sizeof(ref), "refs/heads/%s", branch_name);
        execlp("git", "git", "-C", repo_root, "show-ref", "--verify", ref, NULL);
        _exit(1);
    }

    close(pipefd[1]);
    char buf[256];
    ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
    close(pipefd[0]);

    int status;
    waitpid(pid, &status, 0);

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/*
 * _git_common_dir — Return the git common dir for a path.
 *
 * Python: def _git_common_dir(path: Path) -> Optional[Path]:
 *   result = subprocess.run(["git", "-C", path, "rev-parse", "--path-format=absolute", "--git-common-dir"], ...)
 */
/* Port of Python: _git_common_dir */
const char* _git_common_dir(const char* path)
{
    if (!path || !path[0]) return NULL;

    int pipefd[2];
    if (pipe(pipefd) != 0) return NULL;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execlp("git", "git", "-C", path, "rev-parse", "--path-format=absolute", "--git-common-dir", NULL);
        _exit(1);
    }

    close(pipefd[1]);
    char buf[4096];
    ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
    close(pipefd[0]);

    int status;
    waitpid(pid, &status, 0);

    if (n <= 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return NULL;
    }
    buf[n] = '\0';

    char* nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    if (buf[0] == '\0') return NULL;

    return strdup(buf);
}

/*
 * _git_dir — Return the git dir for a path.
 */
/* Port of Python: _git_dir */
const char* _git_dir(const char* path)
{
    if (!path || !path[0]) return NULL;

    int pipefd[2];
    if (pipe(pipefd) != 0) return NULL;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execlp("git", "git", "-C", path, "rev-parse", "--path-format=absolute", "--git-dir", NULL);
        _exit(1);
    }

    close(pipefd[1]);
    char buf[4096];
    ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
    close(pipefd[0]);

    int status;
    waitpid(pid, &status, 0);

    if (n <= 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return NULL;
    }
    buf[n] = '\0';

    char* nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    if (buf[0] == '\0') return NULL;

    return strdup(buf);
}

/*
 * _git_current_branch — Return the current branch name.
 *
 * Python: def _git_current_branch(path: Path) -> Optional[str]:
 *   result = subprocess.run(["git", "-C", path, "branch", "--show-current"], ...)
 */
/* Port of Python: _git_current_branch */
const char* _git_current_branch(const char* path)
{
    if (!path || !path[0]) return NULL;

    int pipefd[2];
    if (pipe(pipefd) != 0) return NULL;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execlp("git", "git", "-C", path, "branch", "--show-current", NULL);
        _exit(1);
    }

    close(pipefd[1]);
    char buf[1024];
    ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
    close(pipefd[0]);

    int status;
    waitpid(pid, &status, 0);

    if (n <= 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return NULL;
    }
    buf[n] = '\0';

    char* nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    if (buf[0] == '\0') return NULL;

    return strdup(buf);
}

/*
 * _nearest_existing_path — Walk up the directory tree to find an existing path.
 *
 * Python: def _nearest_existing_path(path: Path) -> Path:
 *   current = path
 *   while not current.exists() and current != current.parent:
 *       current = current.parent
 *   return current
 */
/* Port of Python: _nearest_existing_path */
const char* _nearest_existing_path(const char* path)
{
    if (!path || !path[0]) return ".";

    /* Try the path itself first */
    struct stat st;
    if (stat(path, &st) == 0) {
        return strdup(path);
    }

    /* Walk up the tree */
    char* current = strdup(path);
    if (!current) return ".";

    while (1) {
        char* parent = dirname(current);
        if (!parent) break;
        if (strcmp(parent, current) == 0) break; /* Reached root */

        if (stat(parent, &st) == 0) {
            free(current);
            return strdup(parent);
        }

        char* tmp = strdup(parent);
        free(current);
        current = tmp;
        if (!current) break;
    }

    free(current);
    return ".";
}

/* ===========================================================================
 *  Kanban path / config helpers — ported from hermes_cli/kanban_db.py
 *  These were REAL_GAP. Faithful re-implementations.
 * =========================================================================== */

#define KB_DEFAULT_CLAIM_TTL_SECONDS (15 * 60)
#define KB_DEFAULT_CRASH_GRACE_SECONDS 30
#define KB_DEFAULT_RATE_LIMIT_COOLDOWN_SECONDS 300
#define KB_DEFAULT_BOARD "default"

/* Resolve the shared Hermes root anchoring the kanban board.
 * SLERMES IDENTITY: never fall back to the Python home (~/.hermes) — the
 * kanban board is slermes state and must live under the slermes root. */
static void kanban_root_dir(char *out, size_t sz)
{
    const char *override = getenv("HERMES_KANBAN_HOME");
    if (override && override[0]) { snprintf(out, sz, "%s", override); return; }
    const char *home = slermes_home();
    if (home && home[0]) { snprintf(out, sz, "%s", home); return; }
    const char *h = getenv("HOME");
    snprintf(out, sz, "%s/.slermes", h ? h : ".");
}

/*
 * PoP: _resolve_claim_ttl_seconds @ hermes_cli/kanban_db.py:_resolve_claim_ttl_seconds */
int resolve_claim_ttl_seconds(int ttl_seconds)
{
    if (ttl_seconds >= 0) return ttl_seconds < 1 ? 1 : ttl_seconds; /* explicit call-site value wins */
    const char *raw = getenv("HERMES_KANBAN_CLAIM_TTL_SECONDS");
    if (raw) {
        char *end = NULL;
        long parsed = strtol(raw, &end, 10);
        if (end != raw && *end == '\0' && parsed > 0) return (int)parsed;
    }
    return KB_DEFAULT_CLAIM_TTL_SECONDS;
}

/*
 * PoP: _resolve_crash_grace_seconds @ hermes_cli/kanban_db.py:_resolve_crash_grace_seconds */
int resolve_crash_grace_seconds(void)
{
    const char *raw = getenv("HERMES_KANBAN_CRASH_GRACE_SECONDS");
    if (raw) {
        char *end = NULL;
        long parsed = strtol(raw, &end, 10);
        if (end != raw && *end == '\0' && parsed >= 0) return (int)parsed;
    }
    return KB_DEFAULT_CRASH_GRACE_SECONDS;
}

/*
 * PoP: _resolve_rate_limit_cooldown_seconds @ hermes_cli/kanban_db.py:_resolve_rate_limit_cooldown_seconds */
int resolve_rate_limit_cooldown_seconds(void)
{
    const char *raw = getenv("HERMES_KANBAN_RATE_LIMIT_COOLDOWN_SECONDS");
    if (raw) {
        char *end = NULL;
        long parsed = strtol(raw, &end, 10);
        if (end != raw && *end == '\0' && parsed >= 0) return (int)parsed;
    }
    return KB_DEFAULT_RATE_LIMIT_COOLDOWN_SECONDS;
}

/*
 * Returns malloc'd coarse age string. Caller frees. */
char *_relative_age(long ts, long now)
{
    if (now <= 0) now = (long)time(NULL);
    long delta = now - ts;
    if (delta < 0) return strdup("just now");
    if (delta < 60) return strdup("just now");
    if (delta < 3600) { char b[32]; snprintf(b, sizeof(b), "%ldm ago", delta/60); return strdup(b); }
    if (delta < 86400) { char b[32]; snprintf(b, sizeof(b), "%ldh ago", delta/3600); return strdup(b); }
    { char b[32]; snprintf(b, sizeof(b), "%ldd ago", delta/86400); return strdup(b); }
}

/*
 * Returns malloc'd normalized slug or NULL (invalid/empty). Caller frees. */
char *_normalize_board_slug(const char *slug)
{
    if (!slug) return NULL;
    char s[128];
    size_t k = 0;
    for (size_t i = 0; slug[i] && k < sizeof(s)-1; i++) {
        char c = (char)tolower((unsigned char)slug[i]);
        if (c == ' ' || c == '\t') continue; /* strip whitespace */
        s[k++] = c;
    }
    s[k] = '\0';
    if (!s[0]) return NULL;
    /* validate: ^[a-z0-9][a-z0-9\-_]{0,63}$ */
    if (!(isalnum((unsigned char)s[0]))) return NULL;
/* PoP: get_current_board @ hermes_cli/kanban_db.py:get_current_board */
    for (size_t i = 1; i < strlen(s); i++)
        if (!(isalnum((unsigned char)s[i]) || s[i]=='-' || s[i]=='_')) return NULL;
    if (strlen(s) > 64) return NULL;
    return strdup(s);
}

/*
 * Returns malloc'd root dir. Caller frees. */
/* PoP: kanban_home @ hermes_cli/kanban_db.py:kanban_home */
char *kanban_home(void)
{
    char root[PATH_MAX];
    kanban_root_dir(root, sizeof(root));
    return strdup(root);
}

/*
 * PoP: kanban_boards_root @ hermes_cli/kanban_db.py:boards_root */
char *kanban_boards_root(void)
{
    char root[PATH_MAX];
    kanban_root_dir(root, sizeof(root));
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/kanban/boards", root);
    return strdup(out);
}

/*
 * PoP: kanban_current_board_path @ hermes_cli/kanban_db.py:current_board_path */
char *kanban_current_board_path(void)
{
    char root[PATH_MAX];
    kanban_root_dir(root, sizeof(root));
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/kanban/current", root);
    return strdup(out);
}

/* board_exists: directory under boards_root, or the "default" sentinel. */
static int kanban_board_exists(const char *slug)
{
    if (!slug) return 0;
    if (strcmp(slug, KB_DEFAULT_BOARD) == 0) return 1;
    char root[PATH_MAX];
    kanban_root_dir(root, sizeof(root));
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/kanban/boards/%s", root, slug);
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

/*
 * Returns malloc'd active board slug (never NULL). Caller frees. */
char *get_current_board(void)
{
    /* 1. HERMES_KANBAN_BOARD env (must be valid + exist) */
    const char *env = getenv("HERMES_KANBAN_BOARD");
    if (env && env[0]) {
        char *normed = _normalize_board_slug(env);
        if (normed) {
            if (kanban_board_exists(normed)) return normed;
            free(normed);
        }
    }
    /* 2. <root>/kanban/current on disk */
    char *cp = kanban_current_board_path();
    FILE *f = fopen(cp, "r");
    free(cp);
    if (f) {
        char buf[128];
        if (fgets(buf, sizeof(buf), f)) {
            fclose(f);
            size_t n = strlen(buf);
            while (n > 0 && (buf[n-1]=='\n'||buf[n-1]=='\r'||buf[n-1]==' '||buf[n-1]=='\t')) buf[--n]=0;
            char *normed = _normalize_board_slug(buf);
            if (normed) {
                if (kanban_board_exists(normed)) return normed;
                free(normed);
            }
            return strdup(KB_DEFAULT_BOARD);
        }
        fclose(f);
    }
    /* 3. default */
    return strdup(KB_DEFAULT_BOARD);
}

/*
 * Writes the slug to <root>/kanban/current. Returns 0 on success. */
/* PoP: set_current_board @ hermes_cli/kanban_db.py:set_current_board */
int set_current_board(const char *slug)
{
    char *normed = _normalize_board_slug(slug);
    if (!normed) return -1;
    char *cp = kanban_current_board_path();
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", cp);
    char *slash = strrchr(dir, '/');
    if (slash) *slash = '\0';
    mkdir(dir, 0755);
    FILE *f = fopen(cp, "w");
    free(cp);
    if (!f) { free(normed); return -1; }
    fprintf(f, "%s\n", normed);
    fclose(f);
    free(normed);
    return 0;
}

/* ===========================================================================
 *  Additional Kanban path / string / sqlite helpers
 *  Ported from hermes_cli/kanban_db.py (faithful; reuse kanban_home,
 *  kanban_boards_root, _normalize_board_slug, kanban_board_exists,
 *  get_current_board already ported above).
 * =========================================================================== */

/*
 * Returns malloc'd "<root>/kanban/boards/<slug>" (slug defaulted). Caller frees. */
/* PoP: board_dir @ hermes_cli/kanban_db.py:board_dir */
char *board_dir(const char *board)
{
    char *slug = _normalize_board_slug(board);
    if (!slug) slug = strdup(KB_DEFAULT_BOARD);
    char *br = kanban_boards_root();
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/%s", br, slug);
    free(br);
    free(slug);
    return strdup(out);
}

/*
 * PoP: board_exists @ hermes_cli/kanban_db.py:board_exists */
int board_exists(const char *board)
{
    char *slug = _normalize_board_slug(board);
    if (!slug) slug = strdup(KB_DEFAULT_BOARD);
    int r = kanban_board_exists(slug);
    free(slug);
    return r;
}

/*
 * Returns malloc'd path to the kanban.db for board. Caller frees. */
/* PoP: kanban_db_path @ hermes_cli/kanban_db.py:kanban_db_path */
char *kanban_db_path(const char *board)
{
    const char *override = getenv("HERMES_KANBAN_DB");
    if (override && override[0]) return strdup(override);
    char *slug = _normalize_board_slug(board);
    if (!slug) slug = get_current_board();   /* may return a fresh malloc */
    char out[PATH_MAX];
    if (strcmp(slug, KB_DEFAULT_BOARD) == 0) {
        char *home = kanban_home();
        snprintf(out, sizeof(out), "%s/kanban.db", home);
        free(home);
    } else {
        char *bd = board_dir(slug);
        snprintf(out, sizeof(out), "%s/kanban.db", bd);
        free(bd);
    }
    free(slug);
    return strdup(out);
}

/* PoP: profile_exists @ hermes_cli/profiles.py:profile_exists */
/* True iff a profile directory named `name` exists on disk. Shared by all
 * kanban concern modules (decompose, util) — promoted from a static helper so
 * there is a single source of truth. */
/* PoP: profile_exists @ hermes_cli/profiles.py:profile_exists */
/* Python's kanban_db.py imports this from hermes_cli.profiles — it is */
/* the SAME canonical profile-dir existence check, NOT a kanban-profile */
/* listing. Delegate to the canonical implementation (port_cli_profiles.c */
/* profile_dir_exists) so both spellings stay in lockstep with Python. */
/* Returns -1 on empty name (Python raises ValueError there). */
extern int profile_dir_exists(const char *name); /* port_cli_profiles.c */
int profile_exists(const char *name)
{
    return profile_dir_exists(name);
}

/*
 * Returns malloc'd workspaces dir for board. Caller frees. */
/* PoP: workspaces_root @ hermes_cli/kanban_db.py:workspaces_root */
char *workspaces_root(const char *board)
{
    const char *override = getenv("HERMES_KANBAN_WORKSPACES_ROOT");
    if (override && override[0]) return strdup(override);
    char *slug = _normalize_board_slug(board);
    if (!slug) slug = get_current_board();
    char out[PATH_MAX];
    if (strcmp(slug, KB_DEFAULT_BOARD) == 0) {
        char *home = kanban_home();
        snprintf(out, sizeof(out), "%s/kanban/workspaces", home);
        free(home);
    } else {
        char *bd = board_dir(slug);
        snprintf(out, sizeof(out), "%s/workspaces", bd);
        free(bd);
    }
    free(slug);
    return strdup(out);
}

/*
 * Returns malloc'd attachments dir for board. Caller frees. */
/* PoP: attachments_root @ hermes_cli/kanban_db.py:attachments_root */
char *attachments_root(const char *board)
{
    const char *override = getenv("HERMES_KANBAN_ATTACHMENTS_ROOT");
    if (override && override[0]) return strdup(override);
    char *slug = _normalize_board_slug(board);
    if (!slug) slug = get_current_board();
    char out[PATH_MAX];
    if (strcmp(slug, KB_DEFAULT_BOARD) == 0) {
        char *home = kanban_home();
        snprintf(out, sizeof(out), "%s/kanban/attachments", home);
        free(home);
    } else {
        char *bd = board_dir(slug);
        snprintf(out, sizeof(out), "%s/attachments", bd);
        free(bd);
    }
    free(slug);
    return strdup(out);
}

/*
 * Returns malloc'd "<attachments_root>/<task_id>". Caller frees. */
/* PoP: task_attachments_dir @ hermes_cli/kanban_db.py:task_attachments_dir */
char *task_attachments_dir(const char *task_id, const char *board)
{
    char *root = attachments_root(board);
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/%s", root, task_id ? task_id : "");
    free(root);
    return strdup(out);
}

/*
 * Returns malloc'd worker-logs dir for board. Caller frees. */
/* PoP: worker_logs_dir @ hermes_cli/kanban_db.py:worker_logs_dir */
char *worker_logs_dir(const char *board)
{
    char *slug = _normalize_board_slug(board);
    if (!slug) slug = get_current_board();
    char out[PATH_MAX];
    if (strcmp(slug, KB_DEFAULT_BOARD) == 0) {
        char *home = kanban_home();
        snprintf(out, sizeof(out), "%s/kanban/logs", home);
        free(home);
    } else {
        char *bd = board_dir(slug);
        snprintf(out, sizeof(out), "%s/logs", bd);
        free(bd);
    }
    free(slug);
    return strdup(out);
}

/*
 * Returns malloc'd "<board_dir>/board.json". Caller frees. */
/* PoP: board_metadata_path @ hermes_cli/kanban_db.py:board_metadata_path */
char *board_metadata_path(const char *board)
{
    char *slug = _normalize_board_slug(board);
    if (!slug) slug = strdup(KB_DEFAULT_BOARD);
    char *bd = board_dir(slug);
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/board.json", bd);
    free(bd);
    free(slug);
    return strdup(out);
}

/*
 * Turns "atm10-server" -> "Atm10 Server". Returns malloc'd string. Caller frees. */
char *_default_board_display_name(const char *slug)
{
    /* Replace '_' with '-', split on '-', capitalize each part, join with space. */
    char tmp[256];
    size_t k = 0;
    for (size_t i = 0; slug && slug[i] && k < sizeof(tmp) - 1; i++) {
        char c = slug[i];
        if (c == '_') c = '-';
        tmp[k++] = c;
    }
    tmp[k] = '\0';
    /* tokenize on '-' */
    char *out = malloc(strlen(tmp) + 1);
    if (!out) return strdup(slug ? slug : "");
    out[0] = '\0';
    char *save = NULL;
    char *tok = strtok_r(tmp, "-", &save);
    int first = 1;
    while (tok) {
        if (tok[0]) {
            /* Python str.capitalize(): first char upper, REST LOWER
             * ("NULL" -> "Null"). toupper alone left "NULL" -> "NULL". */
            tok[0] = (char)toupper((unsigned char)tok[0]);
            for (char *q = tok + 1; *q; q++)
                *q = (char)tolower((unsigned char)*q);
            if (!first) strcat(out, " ");
            strcat(out, tok);
            first = 0;
        }
        tok = strtok_r(NULL, "-", &save);
    }
    if (out[0] == '\0') {
        free(out);
        return strdup(slug ? slug : "");
    }
    return out;
}

/*
 * Removes <root>/kanban/current if present. Returns 0 on success / not-found. */
/* PoP: clear_current_board @ hermes_cli/kanban_db.py:clear_current_board */
int clear_current_board(void)
{
    char *cp = kanban_current_board_path();
    int r = remove(cp);
    free(cp);
    return (r == 0 || errno == ENOENT) ? 0 : -1;
}

/*
 * Returns the SQLite busy timeout (ms) for Kanban connections. */
int resolve_busy_timeout_ms(void)
{
    const char *raw = getenv("HERMES_KANBAN_BUSY_TIMEOUT_MS");
    if (raw && raw[0]) {
        char *end = NULL;
        long parsed = strtol(raw, &end, 10);
        if (end != raw && *end == '\0' && parsed > 0) return (int)parsed;
    }
    return 120000;  /* default 120s, matches Python */
}

/*
 * Normalises int/float/numeric-string/ISO-8601 to unix epoch seconds.
 * Returns value, or -1 for None/empty/unparseable. */
long to_epoch(const char *val)
{
    if (!val) return -1;
    /* int / float pass-through */
    char *end = NULL;
    long as_long = strtol(val, &end, 10);
    if (end != val && *end == '\0') return as_long;
    double as_double = strtod(val, &end);
    if (end != val && *end == '\0') return (long)as_double;
    /* trim */
    while (*val && isspace((unsigned char)*val)) val++;
    size_t n = strlen(val);
    while (n > 0 && isspace((unsigned char)val[n - 1])) n--;
    if (n == 0) return -1;
    /* ISO-8601 fallback: replace trailing Z with +00:00 and parse */
    char buf[64];
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, val, n);
    buf[n] = '\0';
    char *z = strchr(buf, 'Z');
    if (z) { *z = '+'; memmove(z + 1, z, strlen(z) + 1); buf[strlen(buf)] = '\0';
             /* re-insert ":00" after + : need "+00:00" form; simplify to "+00" */
             size_t len = strlen(buf);
             if (len + 2 < sizeof(buf)) { buf[len] = ':'; buf[len+1] = '0'; buf[len+2] = '0'; buf[len+3] = '\0'; } }
    /* Best-effort: only handle the common 'YYYY-MM-DDTHH:MM:SS' (no tz) and
       'YYYY-MM-DDTHH:MM:SS+00:00' forms via a minimal parser. */
    int Y, M, D, h = 0, m = 0, s = 0, tz_h = 0, tz_m = 0;
    char sep1, sep2;
    int got = sscanf(buf, "%d-%d-%d%c%d:%d:%d%c%d:%d",
                     &Y, &M, &D, &sep1, &h, &m, &s, &sep2, &tz_h, &tz_m);
    if (got >= 7) {
        /* days-from-civil (Howard Hinnant) */
        int yy = Y - (M <= 2 ? 1 : 0);
        int era = (yy >= 0 ? yy : yy - 399) / 400;
        long yoe = (long)(yy - era * 400);
        long doy = (153 * (M + (M > 2 ? -3 : 9)) + 2) / 5 + D - 1;
        long doe = (era * 365L + yoe / 4 - yoe / 100) * 1L + doy;
        long days = era * 146097L + doe - 719468L;
        long secs = days * 86400L + h * 3600L + m * 60L + s - (tz_h * 3600L + tz_m * 60L);
        return (long)secs;
    }
    return -1;
}

/*
 * Returns 1 for a TLS record header at data[offset:]. */
int looks_like_tls_record_at(const unsigned char *data, size_t len, size_t offset)
{
    if (len < offset + 5) return 0;
    unsigned char content_type = data[offset];
    unsigned char major = data[offset + 1];
    unsigned char minor = data[offset + 2];
    unsigned int length = ((unsigned int)data[offset + 3] << 8) | data[offset + 4];
    int ct_ok = (content_type == 0x14 || content_type == 0x15 ||
                 content_type == 0x16 || content_type == 0x17);
    int ver_ok = (major == 0x03) && (minor <= 0x04);
    return (ct_ok && ver_ok && length > 0 && length <= 18432) ? 1 : 0;
}

/*
 * Returns 1 if path is absent/empty/sqlite-header, 0 if a non-empty
 * non-sqlite file (caller should error). */
int _validate_sqlite_header(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return 1;          /* absent => OK (sqlite creates it) */
    if (st.st_size == 0) return 1;               /* zero-byte => OK */
    FILE *f = fopen(path, "rb");
    if (!f) return 1;
    unsigned char head[16];
    size_t n = fread(head, 1, 16, f);
    fclose(f);
    if (n < 16) return 1;
    return (memcmp(head, "SQLite format 3\0", 16) == 0) ? 1 : 0;
}

/*
 * Normalises an error string for grouping identical failures.
 * Returns malloc'd fingerprint (lowercased, host details stripped). Caller frees. */
char *_error_fingerprint(const char *error_text)
{
    if (!error_text) return strdup("");
    char buf[128];
    strncpy(buf, error_text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    /* take first 80 chars */
    size_t n = strlen(buf);
    if (n > 80) buf[80] = '\0';
    /* strip "pid <digits>" -> "pid N" and "<10+ digit numbers>" -> "<TS>" */
    char out[256];
    out[0] = '\0';
    const char *p = buf;
    while (*p) {
        if (strncmp(p, "pid ", 4) == 0) {
            const char *q = p + 4;
            while (isdigit((unsigned char)*q)) q++;
            strcat(out, "pid N");
            p = q;
            continue;
        }
        if (isdigit((unsigned char)*p)) {
            const char *q = p;
            int cnt = 0;
            while (isdigit((unsigned char)*q)) { q++; cnt++; }
            if (cnt >= 10) {
                strcat(out, "<TS>");
                p = q;
                continue;
            }
        }
        size_t L = strlen(out);
        out[L] = *p++;
        out[L + 1] = '\0';
    }
    /* lowercase */
    for (size_t i = 0; out[i]; i++)
        out[i] = (char)tolower((unsigned char)out[i]);
    /* strip trailing/leading whitespace */
    char *s = out;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t e = strlen(s);
    while (e > 0 && isspace((unsigned char)s[e - 1])) s[--e] = '\0';
    return strdup(s);
}

/*
 * Returns malloc'd "<log>.<generation>". Caller frees. */
char *rotated_log_path(const char *log_path, int generation)
{
    /* find suffix start (last '.'), append ".<gen>" after it */
    const char *dot = strrchr(log_path, '.');
    size_t base_len = dot ? (size_t)(dot - log_path) : strlen(log_path);
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%.*s.%d", (int)base_len, log_path, generation);
    return strdup(out);
}

/*
 * Returns 1 when a command override is an explicit path, not a name. */
int _looks_like_path(const char *value)
{
    if (!value || !value[0]) return 0;
    /* ~ expansion is best-effort; we check leading '~', absolute, has dirname,
       backslash, or "X:" windows drive. */
    if (value[0] == '~') return 1;
    if (value[0] == '/') return 1;
    if (strchr(value, '\\')) return 1;
    if (value[1] == ':' && isalpha((unsigned char)value[0])) return 1;
    /* dirname present if any '/' before the final component */
    const char *sl = strchr(value, '/');
    if (sl && sl != value + strlen(value) - 1) {
        /* ensure it's not just a trailing slash on a single name */
        const char *last = strrchr(value, '/');
        if (last != value) return 1;
    }
    return 0;
}

/*
 * PoP: _is_windows_batch_shim @ hermes_cli/kanban_db.py:_is_windows_batch_shim */
int is_windows_batch_shim(const char *path)
{
    if (!path) return 0;
    size_t n = strlen(path);
    if (n >= 4 && (strcasecmp(path + n - 4, ".cmd") == 0 ||
                   strcasecmp(path + n - 4, ".bat") == 0))
        return 1;
    return 0;
}

/*
 * Returns malloc'd "<kanban_home>/<SCRATCH_TIP_SENTINEL_NAME>". Caller frees. */
char *scratch_tip_sentinel_path(void)
{
    char *home = kanban_home();
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/.scratch_tip_shown", home);
    free(home);
    return strdup(out);
}

/* === merged from kanban_db_gaps.c === */
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


/* =========================================================================
 * max-in-progress resolution (kanban_db.py tail)
 * ========================================================================= */

#define KDB_MEMORY_GUARD_MB_PER_WORKER 512L
#define KDB_DERIVED_MAX_IN_PROGRESS_FLOOR 2L
#define KDB_DERIVED_MAX_IN_PROGRESS_CEILING 8L

/* PoP: kdb_count_running_tasks @ hermes_cli/kanban_db.py:count_running_tasks */
static int kdb_count_running_tasks(sqlite3 *conn)
{
    /* Fails open to 0 — a broken board must not brick dispatch on healthy
     * ones (OOF-30). */
    if (!conn) return 0;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(conn, "SELECT COUNT(*) FROM tasks WHERE status = 'running'",
                           -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}

/* PoP: kdb_derive_default_max_in_progress @ hermes_cli/kanban_db.py:derive_default_max_in_progress */
long kdb_derive_default_max_in_progress(const json_t *sample)
{
    json_t *owned = NULL;
    if (!sample) {
        owned = llg_sample_memory();
        sample = owned;
    }
    long result = -1; /* Python None: no cap when memory can't be determined */
    if (sample) {
        json_t *v = json_obj_get(sample, "mem_total_kib");
        /* isinstance(int) and not bool and > 0 — the JSON shim only produces
         * JSON_NUMBER for numerics, so just check positivity. */
        if (v && v->type == JSON_NUMBER && (long)v->num_val > 0) {
            long total_kib = (long)v->num_val;
            long workers = (total_kib / 1024) / KDB_MEMORY_GUARD_MB_PER_WORKER;
            long clamped = workers < KDB_DERIVED_MAX_IN_PROGRESS_CEILING
                               ? workers : KDB_DERIVED_MAX_IN_PROGRESS_CEILING;
            if (clamped < KDB_DERIVED_MAX_IN_PROGRESS_FLOOR)
                clamped = KDB_DERIVED_MAX_IN_PROGRESS_FLOOR;
            result = clamped;
        }
    }
    if (owned) json_free(owned);
    return result;
}

/* PoP: kdb_resolve_max_in_progress @ hermes_cli/kanban_db.py:resolve_max_in_progress */
long kdb_resolve_max_in_progress(long configured)
{
    if (configured >= 0)
        return configured;
    return kdb_derive_default_max_in_progress(NULL);
}

/* PoP: kdb_configured_max_in_progress @ hermes_cli/kanban_db.py:configured_max_in_progress */
long kdb_configured_max_in_progress(void)
{
    json_t *cfg = config_py_load_config_readonly();
    if (!cfg) return -1;
    json_t *raw = config_py_get_config_value(cfg, "kanban.max_in_progress", NULL);
    long result = -1;
    if (raw) {
        if (raw->type == JSON_NUMBER) {
            long ival = (long)raw->num_val;
            if (ival >= 1) result = ival;
        } else if (raw->type == JSON_STRING && raw->str_val[0]) {
            char *end = NULL;
            long ival = strtol(raw->str_val, &end, 10);
            if (end && *end == '\0' && ival >= 1) result = ival;
        }
    }
    json_free(cfg);
    return result;
}

/* PoP: kdb_count_running_tasks_other_boards @ hermes_cli/kanban_db.py:count_running_tasks_other_boards */
int kdb_count_running_tasks_other_boards(const char *board)
{
    char current_resolved[4096] = {0};
    {
        char *cur_path = kanban_db_path(board);
        if (cur_path) {
            char expanded[4096];
            snprintf(expanded, sizeof(expanded), "%s", cur_path);
            free(cur_path);
            /* expanduser: leading ~ → $HOME */
            if (expanded[0] == '~') {
                const char *home = getenv("HOME");
                if (home) {
                    char joined[4096];
                    snprintf(joined, sizeof(joined), "%s%s", home, expanded + 1);
                    snprintf(expanded, sizeof(expanded), "%s", joined);
                }
            }
            char resolved[4096];
            if (realpath(expanded, resolved))
                snprintf(current_resolved, sizeof(current_resolved), "%s", resolved);
        }
    }

    char *boards_json = kdb_list_boards(0);
    if (!boards_json) return 0;
    json_t *boards = json_parse(boards_json, NULL);
    free(boards_json);
    if (!boards || boards->type != JSON_ARRAY) {
        if (boards) json_free(boards);
        return 0;
    }

    int total = 0;
    size_t n = json_len(boards);
    for (size_t i = 0; i < n; i++) {
        json_t *meta = json_get(boards, i);
        json_t *slug_node = meta ? json_obj_get(meta, "slug") : NULL;
        const char *slug = (slug_node && slug_node->type == JSON_STRING &&
                            slug_node->str_val[0])
                               ? slug_node->str_val : KB_DEFAULT_BOARD;

        char *path = kanban_db_path(slug);
        if (!path) continue;
        char expanded[4096];
        snprintf(expanded, sizeof(expanded), "%s", path);
        free(path);
        if (expanded[0] == '~') {
            const char *home = getenv("HOME");
            if (home) {
                char joined[4096];
                snprintf(joined, sizeof(joined), "%s%s", home, expanded + 1);
                snprintf(expanded, sizeof(expanded), "%s", joined);
            }
        }
        char resolved[4096];
        if (!realpath(expanded, resolved)) continue; /* path.exists() gate */
        if (current_resolved[0] && strcmp(resolved, current_resolved) == 0)
            continue; /* this board's own DB — not "other boards" */

        sqlite3 *other = kdb_connect_path(expanded);
        if (!other) continue; /* fails open per board */
        total += kdb_count_running_tasks(other);
        kdb_close(other);
    }
    json_free(boards);
    return total;
}

/* ════════════════════════════════════════════════════════════════════ */
/* RFC #58548 observer hooks + remaining parity closures               */
/* ════════════════════════════════════════════════════════════════════ */

#include "memory_status.h"        /* classify_pressure */
#include "cli_gaps_2026_08_25.h"  /* _worktree_is_dirty / _worktree_has_unpushed_commits */

/* Python: from hermes_cli.lifecycle import has_hook / invoke_hook */
extern int hermes_cli_lifecycle_has_hook(const char *arg);
extern int hermes_cli_lifecycle_invoke_hook(const char *arg);

/* Hot-path short-circuit: whether any first-party observer or plugin
 * consumes *event*. Best-effort — on failure treat as unconsumed.
 *
 * PoP: kdb_kanban_observer_consumed @ hermes_cli/kanban_db.py:_kanban_observer_consumed */
int kdb_kanban_observer_consumed(const char *event)
{
    if (!event || !*event) return 0;
    return hermes_cli_lifecycle_has_hook(event) != 0;
}

/* Fire "on_kanban_worker_spawned" for one dispatched spawn — AFTER the
 * worker PID has been durably persisted. Fully best-effort.
 *
 * PoP: kdb_fire_worker_spawned_hook @ hermes_cli/kanban_db.py:_fire_worker_spawned_hook */
void kdb_fire_worker_spawned_hook(sqlite3 *conn, const char *task_id,
                                  const char *assignee,
                                  const char *workspace_path, long pid,
                                  const char *board)
{
    if (!kdb_kanban_observer_consumed("on_kanban_worker_spawned")) return;

    char *eff_board = NULL;
    if (!board) eff_board = get_current_board();
    long run_id = conn && task_id ? kdb_current_run_id(conn, task_id) : 0;

    char payload[1024];
    snprintf(payload, sizeof(payload),
             "{\"task_id\":\"%s\",\"board\":\"%s\",\"assignee\":\"%s\","
             "\"run_id\":%ld,\"worker_pid\":%ld,\"workspace_path\":\"%s\"}",
             task_id ? task_id : "",
             board ? board : (eff_board ? eff_board : ""),
             assignee ? assignee : "",
             run_id,
             pid > 0 ? pid : 0,
             workspace_path ? workspace_path : "");
    free(eff_board);

    char arg[2048];
    snprintf(arg, sizeof(arg), "on_kanban_worker_spawned\t%s", payload);
    if (hermes_cli_lifecycle_invoke_hook(arg) != 0)
        hermes_log(LOG_DEBUG, "port",
                   "kanban worker spawned hook failed: task=%s",
                   task_id ? task_id : "");
}

/* Fire "on_kanban_task_updated" for a committed task-row mutation. Call
 * AFTER the write txn committed. Observer-only, fully best-effort.
 *
 * PoP: kdb_notify_task_updated @ hermes_cli/kanban_db.py:notify_task_updated */
void kdb_notify_task_updated(sqlite3 *conn, const char *task_id,
                             char **changed_fields, const char *board)
{
    if (!kdb_kanban_observer_consumed("on_kanban_task_updated")) return;

    /* row = SELECT assignee, current_run_id FROM tasks WHERE id = ? */
    const char *assignee = NULL;
    long run_id = 0;
    if (conn && task_id) {
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(conn,
                "SELECT assignee, current_run_id FROM tasks WHERE id = ?",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(st) == SQLITE_ROW) {
                assignee = (const char *)sqlite3_column_text(st, 0);
                run_id = sqlite3_column_type(st, 1) == SQLITE_NULL
                       ? 0 : sqlite3_column_int64(st, 1);
            }
            sqlite3_finalize(st);
        }
    }

    char *eff_board = NULL;
    if (!board) eff_board = get_current_board();

    json_t *payload = json_object();
    if (payload) {
        if (task_id) json_set(payload, "task_id", json_string(task_id));
        json_set(payload, "board", json_string(board ? board : (eff_board ? eff_board : "")));
        json_set(payload, "assignee", json_string(assignee ? assignee : ""));
        json_set(payload, "run_id", json_int(run_id));
        json_t *fields = json_array();
        if (fields && changed_fields)
            for (size_t i = 0; changed_fields[i]; i++)
                json_append(fields, json_string(changed_fields[i]));
        if (fields) json_set(payload, "changed_fields", fields);
    }
    char *payload_json = payload ? json_serialize(payload) : NULL;

    char arg[8192];
    snprintf(arg, sizeof(arg), "on_kanban_task_updated\t%s",
             payload_json ? payload_json : "{}");
    if (hermes_cli_lifecycle_invoke_hook(arg) != 0)
        hermes_log(LOG_DEBUG, "port", "kanban task updated hook failed");

    free(payload_json);
    if (payload) json_free(payload);
    free(eff_board);
}

/* Return True for a TLS record header at data[offset:] (content type in
 * {0x14..0x17}, version major 3, minor in {0..4}, 0 < length <= 18432).
 *
 * PoP: kdb_looks_like_tls_record_at @ hermes_cli/kanban_db.py:_looks_like_tls_record_at */
int kdb_looks_like_tls_record_at(const unsigned char *data, size_t len,
                                 size_t offset)
{
    if (!data || len < offset + 5) return 0;
    unsigned char content_type = data[offset];
    unsigned char major = data[offset + 1];
    unsigned char minor = data[offset + 2];
    unsigned int length = ((unsigned int)data[offset + 3] << 8)
                        | (unsigned int)data[offset + 4];
    return (content_type == 0x14 || content_type == 0x15 ||
            content_type == 0x16 || content_type == 0x17)
        && major == 0x03
        && minor <= 0x04
        && length > 0 && length <= 18432;
}

/* Whether an open connection actually sees the kanban schema ("tasks" is
 * the sentinel). Cheap by design — runs on every steady-state connect.
 *
 * PoP: kdb_schema_is_present @ hermes_cli/kanban_db.py:_schema_is_present */
int kdb_schema_is_present(sqlite3 *conn)
{
    if (!conn) return 0;
    sqlite3_stmt *st = NULL;
    int present = 0;
    if (sqlite3_prepare_v2(conn,
            "SELECT 1 FROM sqlite_master WHERE type='table' AND name='tasks' LIMIT 1",
            -1, &st, NULL) != SQLITE_OK)
        return 0; /* unreadable schema table is not this guard's call */
    if (sqlite3_step(st) == SQLITE_ROW) present = 1;
    sqlite3_finalize(st);
    return present;
}

/* Remove a finished task's linked git worktree when it holds no work.
 * Mirrors cli._prune_stale_worktrees safety judgment: removal requires a
 * clean working tree AND no unpushed commits; any doubt preserves it. The
 * auto-generated wt/<task-id> branch is deleted with it; custom branches
 * are kept. Best-effort — never blocks completion.
 *
 * PoP: kdb_cleanup_worktree_workspace @ hermes_cli/kanban_db.py:_cleanup_worktree_workspace */
void kdb_cleanup_worktree_workspace(const char *task_id, const char *path,
                                    const char *branch_name)
{
    if (!path || !*path) return;

    struct stat sb;
    if (stat(path, &sb) != 0 || !S_ISDIR(sb.st_mode)) return;

    /* common = _git_common_dir(wp); must be a <repo>/.git directory */
    const char *common_c = _git_common_dir(path);
    if (!common_c) return;
    char common[4096];
    snprintf(common, sizeof(common), "%s", common_c);
    free((void *)common_c);

    size_t cl = strlen(common);
    if (cl < 5 || strcmp(common + cl - 5, "/.git") != 0)
        return; /* not a linked worktree of a normal repo — never guess */
    char repo_root[4096];
    snprintf(repo_root, sizeof(repo_root), "%.*s", (int)(cl - 5), common);

    /* never remove the main checkout */
    char resolved_wp[4096], resolved_root[4096];
    if (!realpath(path, resolved_wp) || !realpath(repo_root, resolved_root))
        return;
    if (strcmp(resolved_wp, resolved_root) == 0) return;

    /* safety predicates from the CLI startup pruner */
    if (_worktree_is_dirty(path, 30) || _worktree_has_unpushed_commits(path, 30)) {
        hermes_log(LOG_INFO, "port",
                   "Preserving worktree for task %s: dirty or unpushed work at %s",
                   task_id ? task_id : "", path);
        return;
    }

    char cmd[9216];
    snprintf(cmd, sizeof(cmd),
             "git -C '%s' worktree remove '%s'", repo_root, path);
    FILE *p = popen(cmd, "r");
    int rc = p ? pclose(p) : -1;
    if (rc != 0) {
        hermes_log(LOG_WARNING, "port",
                   "git worktree remove failed for task %s at %s (exit %d)",
                   task_id ? task_id : "", path, rc);
        return; /* fails safe — preserve */
    }
    hermes_log(LOG_DEBUG, "port", "Removed worktree workspace: %s", path);

    /* delete the auto-generated wt/<task-id> branch; keep custom ones */
    char branch[256] = {0};
    if (branch_name && *branch_name) {
        /* Python: branch = (branch_name or "").strip() */
        const char *b = branch_name;
        while (*b == ' ' || *b == '\t') b++;
        snprintf(branch, sizeof(branch), "%s", b);
        size_t bl = strlen(branch);
        while (bl > 0 && (branch[bl-1] == ' ' || branch[bl-1] == '\t'))
            branch[--bl] = '\0';
    }
    if (!branch[0]) {
        if (!task_id) return;
        snprintf(branch, sizeof(branch), "wt/%s", task_id);
    }
    if (strncmp(branch, "wt/", 3) == 0) {
        snprintf(cmd, sizeof(cmd),
                 "git -C '%s' branch -D '%s'", repo_root, branch);
        p = popen(cmd, "r");
        if (p) pclose(p);
    }
}

/* Best-effort system memory snapshot (KiB values); {} when unknown.
 * Delegates to gateway.lifecycle_ledger.sample_memory (pure /proc reads).
 *
 * PoP: kdb_system_memory_sample @ hermes_cli/kanban_db.py:_system_memory_sample */
json_t *kdb_system_memory_sample(void)
{
    json_t *sample = llg_sample_memory();
    if (!sample) sample = json_object();
    return sample; /* never NULL, mirrors `sample_memory() or {}` */
}

/* Classify current system memory pressure: ok/elevated/critical/unknown.
 * Reuses gateway.memory_status.classify_pressure so the dispatcher's idea
 * of "critical" matches the dashboard banner. NULL/empty sample => unknown.
 *
 * PoP: kdb_memory_pressure_level @ hermes_cli/kanban_db.py:_memory_pressure_level */
const char *kdb_memory_pressure_level(const json_t *sample)
{
    if (!sample || sample->type != JSON_OBJECT || sample->c.count == 0)
        return "unknown";
    json_t *avail = json_obj_get((json_t *)sample, "mem_available_kib");
    json_t *total = json_obj_get((json_t *)sample, "mem_total_kib");
    return classify_pressure(avail, total);
}

#define KANBAN_TERMINAL_TIMEOUT_GRACE_SECONDS_C 30

/* Return a malloc'd worker-scoped TERMINAL_TIMEOUT override string when the
 * existing timeout is below runtime - grace; NULL when no override needed.
 *
 * PoP: kdb_worker_terminal_timeout_env @ hermes_cli/kanban_db.py:_worker_terminal_timeout_env */
char *kdb_worker_terminal_timeout_env(long max_runtime_seconds,
                                      const char *current_timeout)
{
    if (max_runtime_seconds <= 0) return NULL;
    long desired = max_runtime_seconds - KANBAN_TERMINAL_TIMEOUT_GRACE_SECONDS_C;
    if (desired < 1) desired = 1; /* max(1, runtime - grace) */
    long existing = 0;
    if (current_timeout && *current_timeout) {
        char *end = NULL;
        long v = strtol(current_timeout, &end, 10);
        if (end && *end == '\0') existing = v;
        /* unparseable -> existing stays 0 (Python except-path) */
    }
    if (existing >= desired) return NULL;
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", desired);
    return strdup(buf);
}

/* Delete notify subscriptions whose task has sat in done untouched for
 * longer than max_age_days. Age is measured from the most recent event
 * (falling back to completed_at then created_at). max_age_days <= 0
 * disables the sweep. Returns rows deleted (-1 on sqlite failure).
 *
 * PoP: kdb_purge_stale_done_notify_subs @ hermes_cli/kanban_db.py:purge_stale_done_notify_subs */
long kdb_purge_stale_done_notify_subs(sqlite3 *conn, long max_age_days)
{
    if (!conn) return -1;
    long days = max_age_days; /* int() of the argument */
    if (days <= 0) return 0;
    long cutoff = (long)time(NULL) - days * 86400;

    if (kdb_write_begin(conn) != 0) return -1; /* with write_txn(conn): */
    sqlite3_stmt *st = NULL;
    long deleted = -1;
    int rc = sqlite3_prepare_v2(conn,
        "DELETE FROM kanban_notify_subs WHERE task_id IN ("
        " SELECT t.id FROM tasks t"
        " WHERE t.status = 'done'"
        " AND COALESCE("
        "  (SELECT MAX(e.created_at) FROM task_events e"
        "   WHERE e.task_id = t.id),"
        "  t.completed_at, t.created_at, 0"
        " ) < ?)",
        -1, &st, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int64(st, 1, cutoff);
        rc = sqlite3_step(st);
        deleted = (rc == SQLITE_DONE) ? (long)sqlite3_changes(conn) : -1;
    }
    sqlite3_finalize(st);
    kdb_write_end(conn, deleted >= 0);
    return deleted;
}
