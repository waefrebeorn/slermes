/*
 * port_hermes_cli_backup.c — C port of selected helpers from
 * hermes_cli/backup.py.
 *
 * Only dependency-light, faithful re-implementations are ported here.
 * Zip/SQLite-backup paths are deferred to their respective subsystems.
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

/* --- Excluded directory names (any level) --- */
static const char *EXCLUDED_DIRS[] = {
    "hermes-agent", "__pycache__", ".git", "node_modules", "backups",
    "checkpoints", ".venv", "venv", "site-packages", ".cache", ".tox",
    ".nox", ".pytest_cache", ".mypy_cache", ".ruff_cache", NULL,
};
/* --- Excluded file basenames --- */
static const char *EXCLUDED_NAMES[] = {
    "gateway.pid", "cron.pid", "gateway_state.json", "gateway.lock",
    "processes.json", NULL,
};
/* --- Excluded file-name suffixes --- */
static const char *EXCLUDED_SUFFIXES[] = {
    ".pyc", ".pyo", ".db-wal", ".db-shm", ".db-journal", NULL,
};

/* Count path components (split on '/'). */
static int path_part_count(const char *p)
{
    int n = 0;
    if (p && *p) n = 1;
    for (const char *c = p; *c; c++) if (*c == '/') n++;
    return n;
}

/* Extract the i-th path component (0-based) into out (caller-sized). */
static void path_part_at(const char *p, int idx, char *out, size_t sz)
{
    int cur = 0;
    const char *start = p;
    while (cur < idx) {
        while (*start && *start != '/') start++;
        if (!*start) { out[0] = '\0'; return; }
        start++; /* skip '/' */
        cur++;
    }
    const char *end = start;
    while (*end && *end != '/') end++;
    size_t len = (size_t)(end - start);
    if (len >= sz) len = sz - 1;
    memcpy(out, start, len);
    out[len] = '\0';
}

/* PoP: _should_exclude @ hermes_cli/backup.py:_should_exclude
 * Returns 1 if rel_path (relative to hermes root) should be skipped. */
int backup_should_exclude(const char *rel_path)
{
    if (!rel_path) return 0;
    int nparts = path_part_count(rel_path);
    for (int i = 0; i < nparts; i++) {
        char part[256];
        path_part_at(rel_path, i, part, sizeof(part));
        for (int k = 0; EXCLUDED_DIRS[k]; k++) {
            if (strcmp(part, EXCLUDED_DIRS[k]) != 0) continue;
            /* "hermes-agent" only matches at the root level (first component). */
            if (strcmp(part, "hermes-agent") == 0 && i != 0) continue;
            return 1;
        }
    }
    /* basename */
    const char *slash = strrchr(rel_path, '/');
    const char *name = slash ? slash + 1 : rel_path;
    for (int k = 0; EXCLUDED_NAMES[k]; k++)
        if (strcmp(name, EXCLUDED_NAMES[k]) == 0) return 1;
    for (int k = 0; EXCLUDED_SUFFIXES[k]; k++) {
        size_t nl = strlen(name), sl = strlen(EXCLUDED_SUFFIXES[k]);
        if (nl >= sl && strcmp(name + nl - sl, EXCLUDED_SUFFIXES[k]) == 0) return 1;
    }
    return 0;
}

/* PoP: _should_skip_backup_file @ hermes_cli/backup.py:_should_skip_backup_file
 * Returns 1 when a candidate file should not be written to a backup. */
int backup_should_skip_file(const char *abs_path, const char *rel_path, const char *out_path)
{
    if (backup_should_exclude(rel_path)) return 1;

    /* zipfile.write() follows symlinks — skip links to avoid copying data
     * from outside HERMES_HOME. */
    struct stat st;
    if (lstat(abs_path, &st) == 0 && S_ISLNK(st.st_mode)) return 1;

    /* Don't write the backup archive into itself. */
    char ra[PATH_MAX], ro[PATH_MAX];
    if (realpath(abs_path, ra) && realpath(out_path, ro) && strcmp(ra, ro) == 0)
        return 1;
    return 0;
}
