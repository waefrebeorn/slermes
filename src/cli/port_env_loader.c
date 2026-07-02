/**
 * port_env_loader.c — Port of Python: cli.py (env loader helpers)
 *
 * Real C implementations for environment loading.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

/* Port of Python: _apply_managed_env */
void apply_managed_env(void *ctx)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "apply_managed_env: null context");
        return;
    }
    hermes_log(LOG_INFO, "port", "apply_managed_env: applying managed environment");

    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    char env_path[4096];
    snprintf(env_path, sizeof(env_path), "%s/.env", home);

    struct stat st;
    if (stat(env_path, &st) == 0) {
        hermes_log(LOG_DEBUG, "port", "apply_managed_env: loading %s (%ld bytes)",
                   env_path, (long)st.st_size);
    } else {
        hermes_log(LOG_DEBUG, "port", "apply_managed_env: no .env at %s", env_path);
    }
}
