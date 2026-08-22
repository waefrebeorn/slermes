/* subagent_worktree.h — C11 port of tools/subagent_worktree.py */
#ifndef SLERMES_SUBAGENT_WORKTREE_H
#define SLERMES_SUBAGENT_WORKTREE_H

#include "hermes_json.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const int subagent_worktree_GIT_TIMEOUT;
extern const char *subagent_worktree_WORKTREES_DIRNAME;
extern const char *subagent_worktree_BRANCH_NAMESPACE;

/* _run_git(args, cwd) -> (returncode, stdout, stderr). Injectable for tests. */
typedef int (*subagent_worktree_run_git_fn)(const char *const args[],
                                            const char *cwd,
                                            char **out_stdout,
                                            char **out_stderr);
typedef int (*subagent_worktree_local_backend_fn)(void);

extern subagent_worktree_run_git_fn subagent_worktree_run_git_hook;
extern subagent_worktree_local_backend_fn subagent_worktree_local_backend_hook;

int  subagent_worktree__run_git_real(const char *const args[], const char *cwd,
                                     char **out_stdout, char **out_stderr);

int  local_backend_active(void);

char *resolve_repo_root(const char *path);

void  subagent_worktree__ensure_gitignore_entry(const char *repo_root);

char *create_subagent_worktree(const char *parent_cwd, const char *subagent_id);

json_t *mark_worktree_payload_unproven(json_t *payload, const char *reason,
                                       const char *unmeasured);
json_t *unproven_worktree_payload(json_t *info, const char *reason);
char   *finalize_subagent_worktree(json_t *info, int prune);
char   *build_worktree_context_note(const char *path, const char *branch);

#ifdef __cplusplus
}
#endif
#endif /* SLERMES_SUBAGENT_WORKTREE_H */
