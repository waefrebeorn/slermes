/**
 * port_doctor.c — Port of Python: cli.py (doctor helpers)
 *
 * Real C implementations for doctor/managed scope checks.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

/* Port of Python: managed_scope_check */
void managed_scope_check(void)
{
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    hermes_log(LOG_INFO, "port", "managed_scope_check: checking %s", home);

    char scope_path[4096];
    snprintf(scope_path, sizeof(scope_path), "%s/.env", home);
    struct stat st;
    if (stat(scope_path, &st) == 0) {
        hermes_log(LOG_DEBUG, "port", "managed_scope_check: .env exists (%ld bytes)",
                   (long)st.st_size);
    } else {
        hermes_log(LOG_DEBUG, "port", "managed_scope_check: no .env found");
    }

    char managed_path[4096];
    snprintf(managed_path, sizeof(managed_path), "%s/managed", home);
    if (stat(managed_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        hermes_log(LOG_INFO, "port", "managed_scope_check: managed directory exists");
    }
}
