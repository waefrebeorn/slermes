/*
 * port_tools_path_security.c — C port of tools/path_security.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>

/* PoP: cli_tools_path_security_has_traversal_component @ tools/path_security.py:has_traversal_component */

/* Port of Python tools/path_security.py:has_traversal_component */
/* Return 1 if path_str contains ".." traversal components. */
int cli_tools_path_security_has_traversal_component(const char *path_str)
{
    if (!path_str) return 0;

    const char *p = path_str;
    while (*p) {
        /* Skip non-slash characters */
        if (*p == '.' && p[1] == '.') {
            /* Check that ".." is a complete path component */
            if ((p == path_str || p[-1] == '/') && (p[2] == '/' || p[2] == '\0')) {
                return 1;
            }
        }
        p++;
    }
    return 0;
}

/* PoP: cli_tools_path_security_validate_within_dir @ tools/path_security.py:validate_within_dir */

/* Port of Python tools/path_security.py:validate_within_dir */
/* Ensure path resolves to a location within root. */
/* Returns error message string if validation fails, or NULL if path is safe. */
char *cli_tools_path_security_validate_within_dir(const char *path_str, const char *root_str)
{
    if (!path_str || !root_str) {
        return strdup("Path or root is NULL");
    }

    /* Quick check for traversal components */
    if (cli_tools_path_security_has_traversal_component(path_str)) {
        return strdup("Path escapes allowed directory: traversal component detected");
    }

    /* Resolve paths using realpath */
    char resolved_path[PATH_MAX];
    char resolved_root[PATH_MAX];

    if (!realpath(path_str, resolved_path)) {
        /* If path doesn't exist, try resolving the parent directory */
        char *copy = strdup(path_str);
        if (!copy) return strdup("Memory allocation failed");
        char *last_slash = strrchr(copy, '/');
        if (last_slash && last_slash != copy) {
            *last_slash = '\0';
            if (!realpath(copy, resolved_path)) {
                free(copy);
                return strdup("Path escapes allowed directory: cannot resolve path");
            }
            /* Reconstruct full path */
            snprintf(resolved_path + strlen(resolved_path),
                PATH_MAX - strlen(resolved_path), "/%s", last_slash + 1);
        } else {
            free(copy);
            return strdup("Path escapes allowed directory: cannot resolve path");
        }
        free(copy);
    }

    if (!realpath(root_str, resolved_root)) {
        return strdup("Path escapes allowed directory: cannot resolve root");
    }

    /* Check that resolved_path starts with resolved_root */
    size_t root_len = strlen(resolved_root);
    if (strncmp(resolved_path, resolved_root, root_len) != 0) {
        return strdup("Path escapes allowed directory: path is outside root");
    }

    /* Ensure the match is at a path boundary (not a partial directory name) */
    if (resolved_path[root_len] != '\0' && resolved_path[root_len] != '/') {
        return strdup("Path escapes allowed directory: partial directory match");
    }

    return NULL; /* path is safe */
}
