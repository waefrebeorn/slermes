/*
 * gitlock.h — C11 port of hermes_cli/gitlock.py.
 *
 * Stale git lock-file recovery for update/check paths. A crashed `git fetch`
 * on a shallow clone leaves .git/shallow.lock behind, wedging
 * `hermes update --check`. Two defensive helpers:
 *   - clear_stale_git_locks (age + live-git-process guarded)
 *   - is_ancestor_of_head   (git merge-base --is-ancestor <rev> HEAD)
 */
#ifndef HERMES_CLI_GITLOCK_H
#define HERMES_CLI_GITLOCK_H

#include <json.h>

#ifdef __cplusplus
extern "C" {
#endif

/* hermes_cli/gitlock.py module constants */
#define GITLOCK_STALE_LOCK_MIN_AGE_SECONDS (10 * 60)
extern const char *GITLOCK_LOCK_NAMES[4];

/* PoP: _git_proc_running @ hermes_cli/gitlock.py:_git_proc_running */
/* True when a `git` process is currently running (pgrep -x git). */
int _git_proc_running(void);

/* Deterministic seam mirroring clear_stale_git_locks' two environmental
 * inputs (the wall clock and the process probe) so the oracle can inject
 * them. Same logic; the public wrapper supplies time(NULL) + probe. */
json_t *gitlock_clear_stale_git_locks_checked(const char *repo_root,
                                              long long min_age_seconds,
                                              long long now_epoch_s,
                                              int git_running);

/* PoP: clear_stale_git_locks @ hermes_cli/gitlock.py:clear_stale_git_locks */
/* min_age_seconds < 0 => STALE_LOCK_MIN_AGE_SECONDS default. Returns the
 * JSON array of removed lock paths. Never raises. Caller frees. */
json_t *clear_stale_git_locks(const char *repo_root, long long min_age_seconds);

/* PoP: is_ancestor_of_head @ hermes_cli/gitlock.py:is_ancestor_of_head */
int is_ancestor_of_head(const char *repo_root, const char *rev);

#endif /* HERMES_CLI_GITLOCK_H */
