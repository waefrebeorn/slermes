#ifndef SLERMES_FILE_OPS_LINT_H
#define SLERMES_FILE_OPS_LINT_H

#include <stdbool.h>

/*
 * file_ops_lint — lint-unusability guard, ported from tools/file_operations.py.
 * Focused module (extracted from port_file_operations.c) so the linter
 * detection lives in its own translation unit with no god-file coupling.
 *
 * Public API:
 *   file_ops_looks_like_linter_unusable — true iff `output` from linter
 *       `base_cmd` indicates the linter itself couldn't run (tooling gap),
 *       as opposed to a real lint error in the file being checked.
 */

/* PoP: file_ops_looks_like_linter_unusable @ tools/file_operations.py:_looks_like_linter_unusable */
bool file_ops_looks_like_linter_unusable(const char *base_cmd, const char *output);

#endif /* SLERMES_FILE_OPS_LINT_H */
