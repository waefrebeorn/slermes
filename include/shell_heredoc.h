/*
 * shell_heredoc.h — Conservative heredoc masking for shell-command scanners.
 * Faithful port of tools/shell_heredoc.py.
 *
 * Masks a heredoc BODY only when it is provably inert data (every opener
 * delimiter quoted, terminated by an exact delimiter line, single command
 * unit, no nested shell scope, inert non-shell consumer). Otherwise leaves
 * the command untouched (fail-closed: a scanner false positive is acceptable,
 * hiding real shell syntax is not). Masked bodies are replaced by an equal
 * number of newlines to preserve line structure.
 */

#ifndef SHELL_HEREDOC_H
#define SHELL_HEREDOC_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Mask heredoc bodies that are provably inert data; keep the rest.
 * Returns a newly-allocated string (caller frees) or NULL on allocation
 * failure. The result may be the original command duplicated. */
char *strip_inert_heredoc_bodies(const char *command);

#ifdef __cplusplus
}
#endif

#endif /* SHELL_HEREDOC_H */
