/*
 * slermes_init_shim.c — link-time resolution for slermes_init /
 * slermes_initialized without redefining slermes_home (already owned by
 * src/infrastructure_stubs.c in the full-link object set).
 *
 * Behavior mirrors src/slermes_home.c: ensure the home directory and its
 * standard subdirectories exist, once per process.
 */
#include "slermes_home.h"
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>

extern const char *slermes_home(void);

static bool s_ready = false;

bool slermes_initialized(void) { return s_ready; }

bool slermes_init(void) {
    if (s_ready) return true;
    const char *home = slermes_home();
    if (!home || !*home) return false;
    mkdir(home, 0755);
    static const char *dirs[] = {
        SLERMES_DIR_SESSIONS, SLERMES_DIR_SKILLS, SLERMES_DIR_CRON,
        SLERMES_DIR_PROFILES, SLERMES_DIR_CACHE, SLERMES_DIR_PLUGINS,
        SLERMES_DIR_LOGS, NULL
    };
    for (int i = 0; dirs[i]; i++) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", home, dirs[i]);
        mkdir(path, 0755);
    }
    s_ready = true;
    return true;
}
