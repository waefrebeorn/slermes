/*
 * gitlock.c — C11 port of hermes_cli/gitlock.py.
 *
 * Stale git lock-file recovery. Lock files younger than
 * STALE_LOCK_MIN_AGE_SECONDS are presumed live; a lock is removed only when
 * it is old AND no git process is running. is_ancestor_of_head wraps
 * `git merge-base --is-ancestor <rev> HEAD` so a local cherry-pick on top of
 * the remote tip doesn't report a false "update available".
 */
#define _POSIX_C_SOURCE 200809L
#include "hermes_posix_compat.h"
#include "gitlock.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

const char *GITLOCK_LOCK_NAMES[4] = {
    "shallow.lock", "index.lock", "HEAD.lock", "MERGE_HEAD.lock"
};

/* PoP: git_proc_running @ hermes_cli/gitlock.py:_git_proc_running */
int git_proc_running(void)
{
    /* Conservative answer on any probe failure: False (Python's except path
     * returns False, treating the lock as possibly-live is the caller's job —
     * here Python returns False meaning "no git seen"; pgrep rc!=0 => none). */
    FILE *p = popen("pgrep -x git 2>/dev/null", "r");
    if (!p)
        return 0;
    int rc = pclose(p);
    return (rc != -1 && WIFEXITED(rc) && WEXITSTATUS(rc) == 0) ? 1 : 0;
}

/* Run argv[0] with cwd=repo_root, return exit status (or -1 on spawn failure).
 * Mirrors subprocess.run(..., cwd=repo_root). stdout/stderr are discarded,
 * matching capture_output=True usage where only returncode is read. */
static int gitlock_run_git(const char *repo_root, char *const argv[])
{
    /* Was: fork / chdir / freopen("/dev/null") / execvp / waitpid. That is the exact shape
     * 52 files in this tree repeat, so it lives in the platform seam now -- CreateProcess
     * with lpCurrentDirectory and NUL handles on Windows, fork+execvp elsewhere. */
    fflush(NULL);
    hermes_pid_t pid = hermes_spawn_in(repo_root, argv, 1);
    if (pid == (hermes_pid_t)-1)
        return -1;
    int status = 0;
    if (hermes_wait(pid, -1, &status) != HERMES_WAIT_EXITED)
        return -1;
    return status;
}

/* PoP: clear_stale_git_locks @ hermes_cli/gitlock.py:clear_stale_git_locks */
json_t *gitlock_clear_stale_git_locks_checked(const char *repo_root,
                                              long long min_age_seconds,
                                              long long now_epoch_s,
                                              int git_running)
{
    json_t *removed = json_array();
    if (!repo_root || !*repo_root)
        return removed;
    char git_dir[8192];
    snprintf(git_dir, sizeof(git_dir), "%s/.git", repo_root);
    struct stat st;
    if (stat(git_dir, &st) != 0 || !S_ISDIR(st.st_mode))
        return removed;                       /* not a git repo worktree */
    if (git_running)
        return removed;                       /* live fetch guard */
    /* Python: cutoff = now - (min_age if min_age is not None else DEFAULT).
     * The -1 sentinel is resolved by the public wrapper only; this seam
     * applies min_age verbatim, exactly like Python. */
    long long cutoff = now_epoch_s - min_age_seconds;
    for (int i = 0; i < 4; i++) {
        char lock_path[9000];
        snprintf(lock_path, sizeof(lock_path), "%s/%s", git_dir,
                 GITLOCK_LOCK_NAMES[i]);
        struct stat lst;
        /* Python: lock_path.is_file() and stat().st_mtime < cutoff */
        if (lstat(lock_path, &lst) != 0 || !S_ISREG(lst.st_mode))
            continue;
        if ((long long)SLERMES_ST_MTIM_SEC(lst) >= cutoff)
            continue;
        if (unlink(lock_path) == 0)
            json_append(removed, json_string(lock_path));
        /* OSError on unlink => skip (never raises) */
    }
    return removed;
}

/* PoP: clear_stale_git_locks @ hermes_cli/gitlock.py:clear_stale_git_locks */
json_t *clear_stale_git_locks(const char *repo_root, long long min_age_seconds)
{
    if (min_age_seconds < 0)
        min_age_seconds = GITLOCK_STALE_LOCK_MIN_AGE_SECONDS;
    return gitlock_clear_stale_git_locks_checked(repo_root, min_age_seconds,
                                                 (long long)time(NULL),
                                                 git_proc_running());
}

/* PoP: is_ancestor_of_head @ hermes_cli/gitlock.py:is_ancestor_of_head */
int is_ancestor_of_head(const char *repo_root, const char *rev)
{
    if (!repo_root || !rev || !*rev)
        return 0;
    char rev_copy[4096];
    snprintf(rev_copy, sizeof(rev_copy), "%s", rev);
    char *argv[] = { "git", "merge-base", "--is-ancestor", rev_copy,
                     "HEAD", NULL };
    return gitlock_run_git(repo_root, argv) == 0 ? 1 : 0;
}
