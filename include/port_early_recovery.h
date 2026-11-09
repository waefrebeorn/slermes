/*
 * port_early_recovery.h -- pure helpers ported from
 * hermes_cli/_early_recovery.py.
 *
 * The filesystem move / backoff half stays in Python.
 */
#ifndef PORT_EARLY_RECOVERY_H
#define PORT_EARLY_RECOVERY_H

#include <stddef.h>

/* is_object_id -- exactly 40 (sha1) or 64 (sha256) lowercase hex chars. */
int er_is_object_id(const char *value);

/* _git_dir -- root/.git, or where a linked worktree's ".git" FILE points.
 * `dot_git_is_file` / `dot_git_text` stand in for the stat and the read.
 * Caller frees. */
char *er_git_dir(const char *root, int dot_git_is_file, const char *dot_git_text);

/* recovery_closure_dir -- <gitdir>/hermes-update-recovery/<pre>, rejecting any
 * `pre` that is not a commit id. *err receives the ValueError text. */
char *er_recovery_closure_dir(const char *root, const char *pre,
                              int dot_git_is_file, const char *dot_git_text,
                              char **err);

/* interrupted_pull_marker -- <gitdir>/hermes-update-pull. Caller frees. */
char *er_interrupted_marker(const char *root, int dot_git_is_file, const char *dot_git_text);

#endif /* PORT_EARLY_RECOVERY_H */
