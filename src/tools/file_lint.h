#ifndef SLERMES_FILE_LINT_H
#define SLERMES_FILE_LINT_H

#include <stdbool.h>

/*
 * file_lint.h — in-process syntax linters for file content.
 *
 * Self-contained module: no god headers, no cross-module struct access.
 * Each linter returns a malloc'd JSON string {"valid":bool,"error":str}
 * (caller frees) or NULL on allocation failure. This mirrors the Python
 * helpers in tools/file_operations.py (_lint_*_inproc) which return
 * (ok, error_message) tuples.
 */

/* Opaque lint context. Holds the Python interpreter used by the
 * Python syntax check (ast.parse has no pure-C equivalent). */
typedef struct file_lint file_lint_t;

/* Create a lint context. python_bin is the interpreter used for the Python
 * syntax check (e.g. "python3"); pass NULL to default to "python3".
 * Returns NULL on allocation failure. */
file_lint_t *file_lint_init(const char *python_bin);

/* Free a lint context (safe with NULL). */
void file_lint_free(file_lint_t *ctx);

/* JSON syntax lint. Stateless. */
char *file_lint_json(const char *content);

/* YAML syntax lint (via libyaml). Stateless. */
char *file_lint_yaml(const char *content);

/* TOML syntax lint (via libtoml). Stateless. */
char *file_lint_toml(const char *content);

/* Python syntax lint. Delegates to the configured interpreter's ast.parse
 * (real subprocess work — not a hardcoded const). Needs the context. */
char *file_lint_python(const file_lint_t *ctx, const char *content);

#endif /* SLERMES_FILE_LINT_H */
