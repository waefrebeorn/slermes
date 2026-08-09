/*
 * port_web_git_routes.c — Faithful C11 port of the git route handlers in
 * hermes_cli/web_server.py (the /api/git/* FastAPI endpoints).
 *
 * These handlers are thin HTTP wrappers: they harden the incoming `path`
 * via _fs_path(), then delegate to the backend git operations in
 * port_web_git.c (web_git_*), exactly mirroring the Python _git_op + _web_git
 * calls. The _git_op() helper runs a blocking op and maps a RuntimeError to a
 * 400; in C the web_git_* functions return their own error JSON shapes, so we
 * propagate those directly (the dashboard serializes them unchanged).
 *
 * Self-contained: depends only on port_web_git.h (backend) and the path
 * resolver ws_fs_path() from port_web_server_schema_path.c. No god headers.
 */

#include "port_web_git.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Forward declaration: path hardener ported from web_server.py:_fs_path(). */
extern char *ws_fs_path(const char *raw_path);

/* ── _git_op error shape ────────────────────────────────────────────────────
 * Python: raise HTTPException(400, detail=str(exc)). We return a JSON object
 * {"error": <msg>} so the dashboard renders the failure. Mirrors the Python
 * behaviour of surfacing the failure to the client with a 4xx-class code. */
static json_t *git_op_error(const char *detail) {
    json_t *e = json_object();
    json_set(e, "error", json_string(detail ? detail : "git operation failed"));
    return e;
}

/* ── git_status_route ───────────────────────────────────────────────────────
 * PoP: git_status_route @ hermes_cli/web_routers/git.py:git_status_route
 * Python: return await _git_op(_web_git.repo_status, _git_path(path)) */
json_t *git_status_route(const char *path) {
    char *cwd = ws_fs_path(path);
    if (!cwd) return git_op_error("invalid path");
    json_t *r = web_git_repo_status(cwd);
    free(cwd);
    return r ? r : git_op_error("git status failed");
}

/* ── git_worktrees_route ─────────────────────────────────────────────────────
 * PoP: git_worktrees_route @ hermes_cli/web_routers/git.py:git_worktrees_route
 * Python: return {"worktrees": await _git_op(_web_git.worktree_list, _git_path(path))} */
json_t *git_worktrees_route(const char *path) {
    char *cwd = ws_fs_path(path);
    if (!cwd) return git_op_error("invalid path");
    json_t *wt = web_git_worktree_list(cwd);
    free(cwd);
    json_t *r = json_object();
    json_set(r, "worktrees", wt ? wt : json_array());
    return r;
}

/* ── git_branches_route ───────────────────────────────────────────────────────
 * PoP: git_branches_route @ hermes_cli/web_routers/git.py:git_branches_route
 * Python: return {"branches": await _git_op(_web_git.branch_list, _git_path(path))} */
json_t *git_branches_route(const char *path) {
    char *cwd = ws_fs_path(path);
    if (!cwd) return git_op_error("invalid path");
    json_t *br = web_git_branch_list(cwd);
    free(cwd);
    json_t *r = json_object();
    json_set(r, "branches", br ? br : json_array());
    return r;
}

/* ── git_base_branches_route ─────────────────────────────────────────────────
 * PoP: git_base_branches_route @ hermes_cli/web_routers/git.py:git_base_branches_route
 * Python: return {"branches": await _git_op(_web_git.base_branch_list, _git_path(path))} */
json_t *git_base_branches_route(const char *path) {
    char *cwd = ws_fs_path(path);
    if (!cwd) return git_op_error("invalid path");
    json_t *br = web_git_base_branch_list(cwd);
    free(cwd);
    json_t *r = json_object();
    json_set(r, "branches", br ? br : json_array());
    return r;
}

/* ── git_review_list_route ────────────────────────────────────────────────────
 * PoP: git_review_list_route @ hermes_cli/web_routers/git.py:git_review_list_route
 * Python: return await _git_op(_web_git.review_list, _git_path(path), scope, base) */
json_t *git_review_list_route(const char *path, const char *scope, const char *base) {
    char *cwd = ws_fs_path(path);
    if (!cwd) return git_op_error("invalid path");
    json_t *r = web_git_review_list(cwd, scope ? scope : "uncommitted", base);
    free(cwd);
    return r ? r : git_op_error("git review list failed");
}

/* ── git_review_diff_route ─────────────────────────────────────────────────────
 * PoP: git_review_diff_route @ hermes_cli/web_routers/git.py:git_review_diff_route
 * Python: return {"diff": await _git_op(_web_git.review_diff, _git_path(path), file, scope, base, staged)} */
json_t *git_review_diff_route(const char *path, const char *file, const char *scope,
                              const char *base, bool staged) {
    char *cwd = ws_fs_path(path);
    if (!cwd) return git_op_error("invalid path");
    char *diff = web_git_review_diff(cwd, file, scope ? scope : "uncommitted", base, staged);
    free(cwd);
    json_t *r = json_object();
    json_set(r, "diff", diff ? json_string(diff) : json_string(""));
    free(diff);
    return r;
}

/* ── git_file_diff_route ───────────────────────────────────────────────────────
 * PoP: git_file_diff_route @ hermes_cli/web_routers/git.py:git_file_diff_route
 * Python: return {"diff": await _git_op(_web_git.file_diff_vs_head, _git_path(path), file)} */
json_t *git_file_diff_route(const char *path, const char *file) {
    char *cwd = ws_fs_path(path);
    if (!cwd) return git_op_error("invalid path");
    char *diff = web_git_file_diff_vs_head(cwd, file);
    free(cwd);
    json_t *r = json_object();
    json_set(r, "diff", diff ? json_string(diff) : json_string(""));
    free(diff);
    return r;
}

/* ── git_commit_context_route ───────────────────────────────────────────────────
 * PoP: git_commit_context_route @ hermes_cli/web_routers/git.py:git_commit_context_route
 * Python: return await _git_op(_web_git.review_commit_context, _git_path(path)) */
json_t *git_commit_context_route(const char *path) {
    char *cwd = ws_fs_path(path);
    if (!cwd) return git_op_error("invalid path");
    json_t *r = web_git_review_commit_context(cwd);
    free(cwd);
    return r ? r : git_op_error("git commit context failed");
}

/* ── git_rev_parse_route ────────────────────────────────────────────────────────
 * PoP: git_rev_parse_route @ hermes_cli/web_routers/git.py:git_rev_parse_route
 * Python: return {"sha": await _git_op(_web_git.review_rev_parse, _git_path(path), ref)} */
json_t *git_rev_parse_route(const char *path, const char *ref) {
    char *cwd = ws_fs_path(path);
    if (!cwd) return git_op_error("invalid path");
    char *sha = web_git_review_rev_parse(cwd, ref);
    free(cwd);
    json_t *r = json_object();
    json_set(r, "sha", sha ? json_string(sha) : json_string(""));
    free(sha);
    return r;
}

/* ── git_ship_info_route ────────────────────────────────────────────────────────
 * PoP: git_ship_info_route @ hermes_cli/web_routers/git.py:git_ship_info_route
 * Python: return await _git_op(_web_git.review_ship_info, _git_path(path)) */
json_t *git_ship_info_route(const char *path) {
    char *cwd = ws_fs_path(path);
    if (!cwd) return git_op_error("invalid path");
    json_t *r = web_git_review_ship_info(cwd);
    free(cwd);
    return r ? r : git_op_error("git ship info failed");
}

/* ── git_stage_route ────────────────────────────────────────────────────────────
 * PoP: git_stage_route @ hermes_cli/web_routers/git.py:git_stage_route
 * Python: return await _git_op(_web_git.review_stage, _git_path(body.path), body.file) */
json_t *git_stage_route(const char *path, const char *file) {
    char *cwd = ws_fs_path(path);
    if (!cwd) return git_op_error("invalid path");
    json_t *r = web_git_review_stage(cwd, file);
    free(cwd);
    return r ? r : git_op_error("git stage failed");
}

/* ── git_unstage_route ───────────────────────────────────────────────────────────
 * PoP: git_unstage_route @ hermes_cli/web_routers/git.py:git_unstage_route
 * Python: return await _git_op(_web_git.review_unstage, _git_path(body.path), body.file) */
json_t *git_unstage_route(const char *path, const char *file) {
    char *cwd = ws_fs_path(path);
    if (!cwd) return git_op_error("invalid path");
    json_t *r = web_git_review_unstage(cwd, file);
    free(cwd);
    return r ? r : git_op_error("git unstage failed");
}

/* ── git_revert_route ─────────────────────────────────────────────────────────────
 * PoP: git_revert_route @ hermes_cli/web_routers/git.py:git_revert_route
 * Python: return await _git_op(_web_git.review_revert, _git_path(body.path), body.file) */
json_t *git_revert_route(const char *path, const char *file) {
    char *cwd = ws_fs_path(path);
    if (!cwd) return git_op_error("invalid path");
    json_t *r = web_git_review_revert(cwd, file);
    free(cwd);
    return r ? r : git_op_error("git revert failed");
}

/* ── git_commit_route ─────────────────────────────────────────────────────────────
 * PoP: git_commit_route @ hermes_cli/web_routers/git.py:git_commit_route
 * Python: return await _git_op(_web_git.review_commit, _git_path(body.path), body.message, body.push) */
json_t *git_commit_route(const char *path, const char *message, bool push) {
    char *cwd = ws_fs_path(path);
    if (!cwd) return git_op_error("invalid path");
    json_t *r = web_git_review_commit(cwd, message, push);
    free(cwd);
    return r ? r : git_op_error("git commit failed");
}

/* ── git_push_route ────────────────────────────────────────────────────────────────
 * PoP: git_push_route @ hermes_cli/web_routers/git.py:git_push_route
 * Python: return await _git_op(_web_git.review_push, _git_path(body.path)) */
json_t *git_push_route(const char *path) {
    char *cwd = ws_fs_path(path);
    if (!cwd) return git_op_error("invalid path");
    json_t *r = web_git_review_push(cwd);
    free(cwd);
    return r ? r : git_op_error("git push failed");
}

/* ── git_create_pr_route ──────────────────────────────────────────────────────────
 * PoP: git_create_pr_route @ hermes_cli/web_routers/git.py:git_create_pr_route
 * Python: return await _git_op(_web_git.review_create_pr, _git_path(body.path)) */
json_t *git_create_pr_route(const char *path) {
    char *cwd = ws_fs_path(path);
    if (!cwd) return git_op_error("invalid path");
    json_t *r = web_git_review_create_pr(cwd);
    free(cwd);
    return r ? r : git_op_error("git create PR failed");
}

/* ── git_worktree_add_route ────────────────────────────────────────────────────────
 * PoP: git_worktree_add_route @ hermes_cli/web_routers/git.py:git_worktree_add_route
 * Python: opts = {k:v for k,v in {name,branch,base,existingBranch}.items() if v}
 *          return await _git_op(_web_git.worktree_add, _git_path(body.path), options) */
json_t *git_worktree_add_route(const char *path, const char *name, const char *branch,
                                const char *base, const char *existing_branch) {
    char *cwd = ws_fs_path(path);
    if (!cwd) return git_op_error("invalid path");
    json_t *r = web_git_worktree_add(cwd, existing_branch, name, branch, base);
    free(cwd);
    return r ? r : git_op_error("git worktree add failed");
}

/* ── git_worktree_remove_route ──────────────────────────────────────────────────────
 * PoP: git_worktree_remove_route @ hermes_cli/web_routers/git.py:git_worktree_remove_route
 * Python: await _git_op(_web_git.worktree_remove, _git_path(body.path), body.worktreePath, body.force) */
json_t *git_worktree_remove_route(const char *path, const char *worktree_path, bool force) {
    char *cwd = ws_fs_path(path);
    if (!cwd) return git_op_error("invalid path");
    json_t *r = web_git_worktree_remove(cwd, worktree_path, force);
    free(cwd);
    return r ? r : git_op_error("git worktree remove failed");
}

/* ── git_branch_switch_route ────────────────────────────────────────────────────────
 * PoP: git_branch_switch_route @ hermes_cli/web_routers/git.py:git_branch_switch_route
 * Python: await _git_op(_web_git.branch_switch, _git_path(body.path), body.branch) */
json_t *git_branch_switch_route(const char *path, const char *branch) {
    char *cwd = ws_fs_path(path);
    if (!cwd) return git_op_error("invalid path");
    json_t *r = web_git_branch_switch(cwd, branch);
    free(cwd);
    return r ? r : git_op_error("git branch switch failed");
}
