/*
 * path_security.c — Path security checking tool for Hermes C.
 * Checks for path traversal, containment, and normalization.
 *
 * Port of Python tools/path_security.py.
 */
#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"path\":{\"type\":\"string\",\"description\":\"Path to check\"},"
      "\"root\":{\"type\":\"string\",\"description\":\"Root directory for containment check (optional)\"}"
    "},"
    "\"required\":[\"path\"]"
"}";

/* Port of Python: path_security handler */
static char *path_security_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *path = json_object_get_string(args, "path", NULL);
    const char *root = json_object_get_string(args, "root", NULL);

    if (!path) {
        json_free(args);
        return strdup("{\"error\":\"Missing path\"}");
    }

    json_node_t *result = json_new_object();
    json_object_set(result, "path", json_new_string(path));

    /* Check traversal */
    bool has_traversal = path_has_traversal(path);
    json_object_set(result, "has_traversal", json_new_bool(has_traversal));

    /* Normalize */
    char *normalized = path_normalize(path);
    json_object_set(result, "normalized", json_new_string(normalized ? normalized : path));
    free(normalized);

    /* Check if absolute */
    json_object_set(result, "is_absolute", json_new_bool(path_is_absolute(path)));

    /* Exists? */
    json_object_set(result, "exists", json_new_bool(path_exists(path)));

    /* Is file or dir? */
    if (path_exists(path)) {
        json_object_set(result, "is_file", json_new_bool(path_is_file(path)));
        json_object_set(result, "is_dir", json_new_bool(path_is_dir(path)));
    }

    /* Containment check */
    if (root && root[0]) {
        char *within = path_within_dir(path, root);
        json_object_set(result, "within_root", json_new_bool(within == NULL));
        if (within) {
            json_object_set(result, "security_warning", json_new_string(within));
            free(within);
        }
        json_object_set(result, "root", json_new_string(root));
    }

    json_free(args);
    char *json_out = json_serialize(result);
    json_free(result);
    return json_out;
}

/* Port of Python: tool registration */
void registry_init_path_security(void) {
    registry_register("path_security",
        "Check path security: detect traversal (..), normalize path, "
        "verify containment within a root directory, "
        "check if path exists and type (file/dir).",
        SCHEMA, path_security_handler);
}

/* === merged from port_tools_path_security.c === */
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

