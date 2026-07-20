/*
 * path_security.h — minimal declaration surface for the deterministic
 * path-safety helpers ported from tools/path_security.py in
 * src/cli/port_tools_path_security.c. Opaque / minimal: no god-header.
 */

#ifndef SLERMES_PATH_SECURITY_H
#define SLERMES_PATH_SECURITY_H

/* Port of tools/path_security.py:has_traversal_component. Pure (no FS). */
int cli_tools_path_security_has_traversal_component(const char *path_str);

/* Port of tools/path_security.py:validate_within_dir. Uses realpath.
 * Returns a malloc'd error message, or NULL if safe. Caller frees. */
char *cli_tools_path_security_validate_within_dir(const char *path_str,
                                                  const char *root_str);

#endif /* SLERMES_PATH_SECURITY_H */
