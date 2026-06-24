/**
 * port_write_approval.c — Port of Python: tools/write_approval.py
 *
 * Real C implementations for write approval helpers.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

/* Port of Python: _pending_dir */
char *pending_dir(const char *subsystem)
{
    if (!subsystem) {
        hermes_log(LOG_WARNING, "port", "pending_dir: null subsystem");
        return strdup("/tmp/.hermes/approvals");
    }
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    char *path = malloc(4096);
    if (!path) return NULL;
    snprintf(path, 4096, "%s/approvals/%s", home, subsystem);
    struct stat st;
    if (stat(path, &st) != 0) {
        hermes_log(LOG_DEBUG, "port", "pending_dir: creating %s", path);
    }
    hermes_log(LOG_DEBUG, "port", "pending_dir: subsystem=%s path=%s", subsystem, path);
    return path;
}
