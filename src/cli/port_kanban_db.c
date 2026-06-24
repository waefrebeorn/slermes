/*
 * port_kanban_db.c — Port of Python hermes_cli/kanban_db.py
 *
 * Git operations and path utilities for the Kanban board.
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
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>

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
