/*
 * worktree_cmd.h — C11 port of hermes_cli/worktree_cmd.py
 *
 * `hermes worktree` — audit and reclaim accumulated git worktrees/branches.
 * Attended counterpart of the silent startup pruner.
 */
#ifndef SLERMES_WORKTREE_CMD_H
#define SLERMES_WORKTREE_CMD_H

#ifdef __cplusplus
extern "C" {
#endif

/* cmd_worktree @ hermes_cli/worktree_cmd.py:cmd_worktree
 * repo: explicit --repo path or NULL (auto-detect). action: "list" |
 * "prune". dry_run/trees_only/branches_only gate prune. Prints to stdout;
 * returns the command exit code. */
int worktree_cmd_cmd_worktree(const char *repo, const char *action,
                              int dry_run, int trees_only, int branches_only);

/* _fmt_size @ hermes_cli/worktree_cmd.py:_fmt_size — malloc'd "?"/"NNM"/"N.NG". */
char *worktree_cmd_fmt_size(int has_size, int size_mb);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_WORKTREE_CMD_H */
