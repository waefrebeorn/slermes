#ifndef SANDBOX_H
#define SANDBOX_H

#include <stdbool.h>
#include <stddef.h>

/* Sandbox path-security concern, extracted from src/tools/file.c.
 * Mirrors the Python file_tools / process_bootstrap sandbox semantics:
 * only paths under an allowed directory may be touched by the file tool.
 */

#define MAX_SANDBOX_DIRS 32

void  sandbox_init(void);
void  sandbox_enable(bool enabled);
bool  sandbox_add_allowed_dir(const char *dir);
bool  sandbox_remove_allowed_dir(const char *dir);
void  sandbox_clear(void);
bool  sandbox_check_path(const char *path);
void  sandbox_set_symlink_check(bool enabled);
bool  is_safe_path(const char *path);

#endif /* SANDBOX_H */
