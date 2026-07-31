/*
 * port_web_server_paths.c — Shared path/install helpers for the
 * web server port.
 *
 * These functions are reused from cron/scheduler.py, hermes_cli/config.py,
 * and tools/tirith_security.py. Keeping them in a dedicated TU avoids
 * circular includes and makes the orchestrator file small again.
 */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <libgen.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>

/* PoP: get_hermes_home @ cron/scheduler.py:_get_hermes_home */
/* PoP: get_hermes_home @ tools/tirith_security.py:_get_hermes_home */
const char *get_hermes_home(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp/.hermes";
    return home;
}

/* PoP: is_container @ hermes_cli/config.py:_is_container */
bool is_container(void) {
    if (access("/.dockerenv", F_OK) == 0)
        return true;
    FILE *f = fopen("/proc/1/cgroup", "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "docker") || strstr(line, "containerd") ||
                strstr(line, "kubepods")) {
                fclose(f);
                return true;
            }
        }
        fclose(f);
    }
    return false;
}

/* PoP: detect_install_method @ hermes_cli/config.py:detect_install_method */
const char *detect_install_method(const char *project_root) {
    if (!project_root)
        return "unknown";
    char path[2048];
    snprintf(path, sizeof(path), "%s/.git", project_root);
    if (access(path, F_OK) == 0)
        return "git";
    return "pip";
}
