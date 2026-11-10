/*
 * port_worktree_ops.h -- pure helpers ported from hermes_cli/worktree_ops.py.
 *
 * The git execution / worktree setup / maintenance half stays in Python.
 */
#ifndef PORT_WORKTREE_OPS_H
#define PORT_WORKTREE_OPS_H

#include <stddef.h>

/* _normalize_git_bash_path -- translate /c/.., /cygdrive/c/.., /mnt/c/.. to
 * C:\.. on Windows. `is_windows` stands in for sys.platform == "win32".
 * Caller frees. */
char *wo_normalize_bash_path(const char *p, int is_windows);

/* _path_is_within_root -- true when a resolved path stays within the root.
 * A path that merely shares a PREFIX is not within the root. */
int wo_within_root(const char *path, const char *root);

/* _worktreeinclude_entries -- non-blank, non-comment entries of
 * .worktreeinclude. `lines` is a NULL-terminated array; `had_bom` says whether
 * the file began with a UTF-8 BOM (stripped from the first line).
 * Caller frees each string and the array. */
char **wo_include_entries(const char *const *lines, int had_bom, int *out_count);

#endif /* PORT_WORKTREE_OPS_H */
