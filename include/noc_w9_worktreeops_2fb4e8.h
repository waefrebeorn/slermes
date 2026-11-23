#ifndef NOC_W9_WORKTREEOPS_2FB4E8_H
#define NOC_W9_WORKTREEOPS_2FB4E8_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_worktreeops_2fb4e8_normalize_git_bash_path @ hermes_cli/worktree_ops.py:_normalize_git_bash_path
 * Git-Bash path -> Windows drive path on win32.  Returns a JSON string. */
struct json_t *w9_worktreeops_2fb4e8_normalize_git_bash_path(const char *p);

/* PoP: w9_worktreeops_2fb4e8_prune_tier_hours @ hermes_cli/worktree_ops.py:prune_tier_hours
 * max_age_hours for hermes-/subagent- prefixed names, else triple. */
struct json_t *w9_worktreeops_2fb4e8_prune_tier_hours(const char *name,
                                                     double max_age_hours);

/* PoP: w9_worktreeops_2fb4e8_path_is_within_root @ hermes_cli/worktree_ops.py:_path_is_within_root
 * pathlib.Path.relative_to() semantics on Windows (case-insensitive, either
 * separator, trailing separators ignored).  Returns a JSON bool. */
struct json_t *w9_worktreeops_2fb4e8_path_is_within_root(const char *path,
                                                        const char *root);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W9_WORKTREEOPS_2FB4E8_H */
