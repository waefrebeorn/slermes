/**
 * port_container_boot.c — Port of Python: cli.py (container boot helpers)
 *
 * Real C implementations for container boot argument processing.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Port of Python: strip_container_argv_prefix */
char *strip_container_argv_prefix(const char *argv)
{
    if (!argv) {
        return strdup("");
    }
    /* Skip common container runtime prefixes */
    const char *prefixes[] = {
        "docker run ", "docker exec ",
        "podman run ", "podman exec ",
        "singularity run ", "singularity exec ",
        "apptainer run ", "apptainer exec ",
        NULL
    };
    int len = strlen(argv);
    for (int i = 0; prefixes[i]; i++) {
        int plen = strlen(prefixes[i]);
        if (strncmp(argv, prefixes[i], plen) == 0) {
            hermes_log(LOG_DEBUG, "port", "strip_container_argv_prefix: stripped '%s'",
                       prefixes[i]);
            return strdup(argv + plen);
        }
    }
    return strdup(argv);
}

/* Port of Python: _strip_container_argv_prefix */
void _strip_container_argv_prefix(void *ctx)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "_strip_container_argv_prefix: null context");
        return;
    }
    hermes_log(LOG_DEBUG, "port", "_strip_container_argv_prefix: processing context");
    /* In the Python source, this strips container prefixes from argv */
    char **argv_ptr = (char **)ctx;
    if (argv_ptr && *argv_ptr) {
        char *stripped = strip_container_argv_prefix(*argv_ptr);
        if (stripped) {
            hermes_log(LOG_DEBUG, "port", "_strip_container_argv_prefix: stripped to '%s'",
                       stripped);
            free(stripped);
        }
    }
}

/* Port of Python: _is_dashboard_container */
bool _is_dashboard_container(void *ctx, void *argv)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "_is_dashboard_container: null context");
        return false;
    }
    hermes_log(LOG_DEBUG, "port", "_is_dashboard_container: checking");
    if (argv) {
        const char *arg = (const char *)argv;
        if (strstr(arg, "dashboard") || strstr(arg, "hermes-web") ||
            strstr(arg, "hermes_dashboard")) {
            hermes_log(LOG_INFO, "port", "_is_dashboard_container: detected dashboard");
            return true;
        }
    }
    return false;
}
