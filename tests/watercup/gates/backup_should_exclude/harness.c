/* gate: backup_should_exclude — traced C copy of
 * src/hermes_cli/backup.c::backup_should_exclude
 * (LIVE twin: hermes_cli/backup.py::_should_exclude).
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: null path
 *   per part loop (L ticks):
 *     B2: part in EXCLUDED_DIRS?
 *       B3: part == "hermes-agent" and not first component? -> skip (continue)
 *       else -> excluded (return true)
 *   V1: basename
 *   B4: basename in EXCLUDED_NAMES
 *   suffix loop:
 *     B5: name endswith suffix?
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

static const char *EXCLUDED_DIRS[] = {
    "hermes-agent", "__pycache__", ".git", "node_modules", "backups",
    "checkpoints", ".venv", "venv", "site-packages", ".cache", ".tox",
    ".nox", ".pytest_cache", ".mypy_cache", ".ruff_cache",
    "browser-profiles", "state-snapshots", NULL,
};
static const char *EXCLUDED_NAMES[] = {
    ".backup.lock", "gateway.pid", "cron.pid", NULL,
};
static const char *EXCLUDED_SUFFIXES[] = {
    ".pyc", ".pyo", ".db-wal", ".db-shm", ".db-journal", NULL,
};

static bool backup_should_exclude(const char *rel_path) {
    WCUP_BEGIN("should_exclude");
    bool result = false;
    if (!rel_path) { WCUP_BRANCH(1); goto out; }
    WCUP_BRANCH(0);
    {
        /* split on '/' and walk parts */
        const char *p = rel_path;
        int idx = 0;
        while (*p) {
            const char *slash = strchr(p, '/');
            size_t len = slash ? (size_t)(slash - p) : strlen(p);
            char part[256];
            if (len >= sizeof(part)) len = sizeof(part) - 1;
            memcpy(part, p, len); part[len] = '\0';
            WCUP_LOOP_TICK();
            int in_dirs = false;
            for (int k = 0; EXCLUDED_DIRS[k]; k++)
                if (strcmp(part, EXCLUDED_DIRS[k]) == 0) { in_dirs = true; break; }
            WCUP_BRANCH(in_dirs);                        /* B2 */
            if (in_dirs) {
                int nested_hermes = (strcmp(part, "hermes-agent") == 0 && idx != 0);
                WCUP_BRANCH(nested_hermes);              /* B3 */
                if (!nested_hermes) { result = true; goto out; }
            }
            p += len;
            if (*p == '/') p++;
            idx++;
        }
    }
    {
        const char *slash = strrchr(rel_path, '/');
        const char *name = slash ? slash + 1 : rel_path;
        WCUP_VALSTR(name);                               /* V1 */
        int in_names = false;
        for (int k = 0; EXCLUDED_NAMES[k]; k++)
            if (strcmp(name, EXCLUDED_NAMES[k]) == 0) { in_names = true; break; }
        WCUP_BRANCH(in_names);                           /* B4 */
        if (in_names) { result = true; goto out; }
        for (int k = 0; EXCLUDED_SUFFIXES[k]; k++) {
            size_t nl = strlen(name), sl = strlen(EXCLUDED_SUFFIXES[k]);
            int ends = (nl >= sl && strcmp(name + nl - sl, EXCLUDED_SUFFIXES[k]) == 0);
            WCUP_BRANCH(ends);                           /* B5 */
            if (ends) { result = true; goto out; }
        }
    }
out:
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("should_exclude");
    return result;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s should_exclude <rel_path>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) backup_should_exclude(argv[i]);
    return 0;
}
