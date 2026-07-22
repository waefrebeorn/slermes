/*
 * port_web_git_routes.h — Slermes C11 port of the git route handlers in
 * hermes_cli/web_server.py (/api/git/* endpoints). Thin wrappers over the
 * backend git operations in port_web_git.h. See port_web_git_routes.c.
 */

#ifndef PORT_WEB_GIT_ROUTES_H
#define PORT_WEB_GIT_ROUTES_H

#include <stdbool.h>
#include "libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *git_status_route(const char *path);
json_t *git_worktrees_route(const char *path);
json_t *git_branches_route(const char *path);
json_t *git_review_list_route(const char *path, const char *scope, const char *base);
json_t *git_review_diff_route(const char *path, const char *file, const char *scope,
                              const char *base, bool staged);
json_t *git_file_diff_route(const char *path, const char *file);
json_t *git_commit_context_route(const char *path);
json_t *git_rev_parse_route(const char *path, const char *ref);
json_t *git_ship_info_route(const char *path);
json_t *git_stage_route(const char *path, const char *file);
json_t *git_unstage_route(const char *path, const char *file);
json_t *git_revert_route(const char *path, const char *file);
json_t *git_commit_route(const char *path, const char *message, bool push);
json_t *git_push_route(const char *path);
json_t *git_create_pr_route(const char *path);
json_t *git_worktree_add_route(const char *path, const char *name, const char *branch,
                               const char *base, const char *existing_branch);
json_t *git_worktree_remove_route(const char *path, const char *worktree_path, bool force);
json_t *git_branch_switch_route(const char *path, const char *branch);

#ifdef __cplusplus
}
#endif

#endif /* PORT_WEB_GIT_ROUTES_H */
