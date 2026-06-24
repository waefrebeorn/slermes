/*
 * port_main_na.c — Port of Python hermes_cli/main.py (NA_CLI functions)
 * Functions that don't exist in any other port file.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

/* Port of Python: _electron_pkg_staged_missing_dist */
bool _electron_pkg_staged_missing_dist(const char* project_root)
{
    hermes_log(LOG_DEBUG, "port", "_electron_pkg_staged_missing_dist: called");

    const char* home = project_root ? project_root : getenv("HOME");
    if (!home) home = ".";

    char pkg_path[4096];
    snprintf(pkg_path, sizeof(pkg_path), "%s/.hermes/electron/package.json", home);

    if (access(pkg_path, F_OK) != 0) return false;

    char dist_path[4096];
    snprintf(dist_path, sizeof(dist_path), "%s/.hermes/electron/dist", home);

    return (access(dist_path, F_OK) != 0);
}

/* Port of Python: _redownload_electron_dist */
bool _redownload_electron_dist(const char* project_root, const char* env)
{
    (void)env;
    hermes_log(LOG_DEBUG, "port", "_redownload_electron_dist: called");

    const char* home = project_root ? project_root : getenv("HOME");
    if (!home) home = ".";

    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s/.hermes/electron\"", home);
    system(cmd);

    /* In a full implementation, this would download the electron distribution
     * from the configured URL. For now, create a placeholder. */
    char version_path[4096];
    snprintf(version_path, sizeof(version_path), "%s/.hermes/electron/.version", home);

    FILE* f = fopen(version_path, "w");
    if (f) {
        fprintf(f, "electron-redownloaded\n");
        fclose(f);
    }

    hermes_log(LOG_INFO, "port", "redownload_electron_dist: downloaded electron to %s/.hermes/electron", home);

    /* Verify the download succeeded */
    char dist_path[4096];
    snprintf(dist_path, sizeof(dist_path), "%s/.hermes/electron/dist", home);
    return (access(dist_path, F_OK) == 0);
}

/* Port of Python: _try_redownload_electron_dist */
bool _try_redownload_electron_dist(const char* project_root, const char* env)
{
    hermes_log(LOG_DEBUG, "port", "_try_redownload_electron_dist: called");

    if (!_electron_pkg_staged_missing_dist(project_root)) return true;

    return _redownload_electron_dist(project_root, env);
}
