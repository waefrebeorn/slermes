/*
 * port_web_git.h — Slermes C11 port of hermes_cli/web_git.py.
 *
 * Backend git operations for the dashboard coding rail + review pane. Every
 * function shells out to the system `git` (and `gh` for ship info / PRs),
 * mirroring the Python module exactly. Functions take an already
 * path-hardened `cwd` and return the same JSON shapes the Python functions
 * emit, so the existing dashboard routes can serialize them unchanged.
 *
 * Memory: json-returning functions return a json_t* owned by the caller
 * (json_free). string-returning functions return malloc'd strings or NULL.
 */

#ifndef PORT_WEB_GIT_H
#define PORT_WEB_GIT_H

#include <stdbool.h>
#include "libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Low-level: run `git <args...>` in cwd, capture stdout (malloc'd, caller
 * frees) and return the exit code. On spawn failure returns 1 and sets
 * *out to "". */
int web_git_run(const char *cwd, char **out, size_t *out_len, const char **args, size_t nargs);

/* ── status / review ──────────────────────────────────────────────────── */
json_t *web_git_repo_status(const char *cwd);
json_t *web_git_review_list(const char *cwd, const char *scope,
                            const char *base_ref);
char   *web_git_review_diff(const char *cwd, const char *file_path,
                            const char *scope, const char *base_ref,
                            bool staged);
char   *web_git_file_diff_vs_head(const char *cwd, const char *file_path);
json_t *web_git_review_commit_context(const char *cwd);
char   *web_git_review_rev_parse(const char *cwd, const char *ref);
json_t *web_git_review_stage(const char *cwd, const char *file_path);
json_t *web_git_review_unstage(const char *cwd, const char *file_path);
json_t *web_git_review_revert(const char *cwd, const char *file_path);
json_t *web_git_review_commit(const char *cwd, const char *message, bool push);
json_t *web_git_review_push(const char *cwd);

/* ── worktrees & branches ─────────────────────────────────────────────── */
json_t *web_git_worktree_list(const char *cwd);
json_t *web_git_branch_list(const char *cwd);
json_t *web_git_branch_switch(const char *cwd, const char *branch);
json_t *web_git_base_branch_list(const char *cwd);
json_t *web_git_worktree_add(const char *cwd, const char *existing_branch,
                              const char *name, const char *branch,
                              const char *base);
json_t *web_git_worktree_remove(const char *cwd, const char *worktree_path,
                                bool force);

/* ── ship flow (gh) ───────────────────────────────────────────────────── */
json_t *web_git_review_ship_info(const char *cwd);
json_t *web_git_review_create_pr(const char *cwd);

#ifdef __cplusplus
}
#endif

#endif /* PORT_WEB_GIT_H */
