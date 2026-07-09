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
    /* A real redownload fetches the electron distribution over HTTP from the
     * configured URL. That network fetch is not implemented in the C port, so
     * we must not claim success or fabricate a version marker. */
    hermes_log(LOG_WARNING, "port",
        "_redownload_electron_dist: electron distribution download not implemented in C port");
    return false;
}

/* Port of Python: _try_redownload_electron_dist */
bool _try_redownload_electron_dist(const char* project_root, const char* env)
{
    hermes_log(LOG_DEBUG, "port", "_try_redownload_electron_dist: called");

    if (!_electron_pkg_staged_missing_dist(project_root)) return true;

    return _redownload_electron_dist(project_root, env);
}
