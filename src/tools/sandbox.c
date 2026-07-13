/*
 * sandbox.c — path-security sandbox, extracted from tools/file.c monolith.
 *
 * Real implementation of the sandbox_* API. Public decls live in
 * include/sandbox.h; callers (file.c, file_batch.c) are unchanged.
 */

#define _GNU_SOURCE
#include "sandbox.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include <fnmatch.h>
#include <ctype.h>


    /* ================================================================
     *  P168: File Sandbox — Allowed directories & symlink attack prevention
     * ================================================================ */

#define MAX_SANDBOX_DIRS 32

static char *g_allowed_dirs[MAX_SANDBOX_DIRS];
static int g_allowed_dir_count = 0;
static bool g_sandbox_enabled = false;
static bool g_symlink_check_enabled = true;

/* Initialize sandbox with default allowed directories */
void sandbox_init(void) {
    const char *home = getenv("HOME");
    if (home) {
        sandbox_add_allowed_dir(home);
    }
    sandbox_add_allowed_dir("/tmp");
    sandbox_add_allowed_dir(getenv("SLERMES_HOME") ? getenv("SLERMES_HOME") : "");
    g_sandbox_enabled = true;
}

/* Enable/disable the sandbox */
void sandbox_enable(bool enabled) {
    g_sandbox_enabled = enabled;
}

/* Add an allowed directory */
bool sandbox_add_allowed_dir(const char *dir) {
    if (!dir || !*dir || g_allowed_dir_count >= MAX_SANDBOX_DIRS)
        return false;

    /* Resolve to absolute path */
    char resolved[4096];
    if (dir[0] == '~') {
        const char *home = getenv("HOME");
        if (!home) return false;
        snprintf(resolved, sizeof(resolved), "%s%s", home, dir + 1);
    } else {
        snprintf(resolved, sizeof(resolved), "%s", dir);
    }

    /* Canonicalize via realpath if path exists */
    char *real = realpath(resolved, NULL);
    if (real) {
        /* Check for duplicate */
        for (int i = 0; i < g_allowed_dir_count; i++) {
            if (g_allowed_dirs[i] && strcmp(g_allowed_dirs[i], real) == 0) {
                free(real);
                return true;
            }
        }
        g_allowed_dirs[g_allowed_dir_count++] = real;
        return true;
    }

    /* Path doesn't exist yet, add as-is */
    for (int i = 0; i < g_allowed_dir_count; i++) {
        if (g_allowed_dirs[i] && strcmp(g_allowed_dirs[i], resolved) == 0)
            return true;
    }
    g_allowed_dirs[g_allowed_dir_count++] = strdup(resolved);
    return true;
}

/* Remove an allowed directory */
bool sandbox_remove_allowed_dir(const char *dir) {
    if (!dir) return false;
    for (int i = 0; i < g_allowed_dir_count; i++) {
        if (g_allowed_dirs[i] && strcmp(g_allowed_dirs[i], dir) == 0) {
            free(g_allowed_dirs[i]);
            g_allowed_dirs[i] = g_allowed_dirs[--g_allowed_dir_count];
            g_allowed_dirs[g_allowed_dir_count] = NULL;
            return true;
        }
    }
    return false;
}

/* Clear all sandbox dirs */
void sandbox_clear(void) {
    for (int i = 0; i < g_allowed_dir_count; i++) {
        free(g_allowed_dirs[i]);
        g_allowed_dirs[i] = NULL;
    }
    g_allowed_dir_count = 0;
}

/* Check if a path is a symlink (outside allowed dirs) */
static bool is_unsafe_symlink(const char *path) {
    if (!g_symlink_check_enabled) return false;

    struct stat st;
    if (lstat(path, &st) != 0) return false; /* Doesn't exist, allow through */

    if (!S_ISLNK(st.st_mode)) return false; /* Not a symlink */

    /* It's a symlink — check if target is within allowed dirs */
    char target[4096];
    ssize_t tlen = readlink(path, target, sizeof(target) - 1);
    if (tlen < 0) return false;

    target[tlen] = '\0';

    /* Resolve relative symlinks */
    char *resolved = realpath(path, NULL);
    if (!resolved) return true; /* Can't resolve — block */

    /* Check if resolved path is in allowed dirs */
    bool safe = false;
    for (int i = 0; i < g_allowed_dir_count; i++) {
        if (g_allowed_dirs[i] && strncmp(resolved, g_allowed_dirs[i],
                                          strlen(g_allowed_dirs[i])) == 0) {
            safe = true;
            break;
        }
    }
    free(resolved);
    return !safe;
}

/* Check if a path is within an allowed directory */
bool sandbox_check_path(const char *path) {
    if (!path || !*path) return false;
    if (!g_sandbox_enabled) return true;

    /* Check for path traversal */
    if (strstr(path, "..")) return false;

    /* Resolve path */
    char *resolved = realpath(path, NULL);
    char check_path[4096];

    if (resolved) {
        snprintf(check_path, sizeof(check_path), "%s", resolved);
    } else {
        /* Path doesn't exist yet — check parent directory */
        snprintf(check_path, sizeof(check_path), "%s", path);
        /* Remove trailing slash */
        char *end = check_path + strlen(check_path) - 1;
        while (end > check_path && *end == '/') *end-- = '\0';

        /* Try parent */
        char *slash = strrchr(check_path, '/');
        if (slash && slash != check_path) {
            *slash = '\0';
        }
    }

    /* Check if the path (or its parent) is in allowed dirs */
    bool allowed = false;
    for (int i = 0; i < g_allowed_dir_count; i++) {
        if (!g_allowed_dirs[i]) continue;
        size_t dlen = strlen(g_allowed_dirs[i]);

        /* Allow exact match or subpath */
        if (strncmp(check_path, g_allowed_dirs[i], dlen) == 0) {
            if (check_path[dlen] == '\0' || check_path[dlen] == '/') {
                allowed = true;
                break;
            }
        }
    }

    if (resolved) free(resolved);

    /* Check symlink safety */
    if (allowed && !is_unsafe_symlink(path)) {
        return true;
    }

    return allowed;
}

/* Enable/disable symlink checking */
void sandbox_set_symlink_check(bool enabled) {
    g_symlink_check_enabled = enabled;
}

/* ================================================================
 *  Path security check (updated to use sandbox)
 * ================================================================ */

bool is_safe_path(const char *path) {
    /* P168: Use sandbox for path validation if enabled */
    if (g_sandbox_enabled) {
        return sandbox_check_path(path);
    }

    /* Fallback: basic checks */

    /* S04: Null byte injection bypass check */
    if (path && strlen(path) != strnlen(path, 4096)) return false;

    /* S04: URL-encoded traversal detection (before realpath resolves it) */
    const char *encoded_checks[] = {
        "%2e",  /* %2e = '.' in URL encoding */
        "%2E",  /* uppercase variant */
        "..",   /* literal traversal */
        NULL
    };
    bool has_dotdot = false;
    for (int chk = 0; encoded_checks[chk]; chk++) {
        const char *p = path;
        while ((p = strstr(p, encoded_checks[chk])) != NULL) {
            /* For literal '..', verify it's a path component boundary */
            if (encoded_checks[chk][0] == '.' && encoded_checks[chk][1] == '.') {
                if ((p == path || p[-1] == '/') &&
                    (p[2] == '/' || p[2] == '\0'))
                    has_dotdot = true;
            } else {
                has_dotdot = true;
            }
            p++;
        }
    }
    if (has_dotdot) return false;

    /* S04: Resolve real path for symlink bypass detection */
    char *resolved = realpath(path, NULL);
    if (resolved) {
        /* Resolved path must be under HOME, /tmp/, or /dev/ */
        const char *home = getenv("HOME");
        bool under_home = home && strncmp(resolved, home, strlen(home)) == 0;
        bool under_tmp = strncmp(resolved, "/tmp/", 5) == 0;
        bool under_dev = strncmp(resolved, "/dev/", 5) == 0;
        bool under_proc_self = strncmp(resolved, "/proc/self/", 11) == 0;
        free(resolved);
        if (!under_home && !under_tmp && !under_dev && !under_proc_self)
            return false;
        return true;  /* resolved path passes all checks */
    }

    /* Path doesn't exist yet (write/create) — fallback to static checks */
    /* Block absolute paths outside home */
    if (path[0] == '/' && strncmp(path, getenv("HOME"), strlen(getenv("HOME"))) != 0) {
        /* Allow /tmp/ and common paths */
        if (strncmp(path, "/tmp/", 5) != 0 && strncmp(path, "/dev/", 5) != 0)
            return false;
    }
    return true;
}
