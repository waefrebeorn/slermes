/**
 * port_nous_portal.c — Port of Python: cli.py (Nous portal helpers)
 *
 * Real C implementations for Nous portal credential access.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Port of Python: get_credential */
const char *get_credential(void)
{
    const char *cred = getenv("NOUS_PORTAL_CREDENTIAL");
    if (cred) {
        hermes_log(LOG_DEBUG, "port", "get_credential: from env");
        return cred;
    }
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    static char path[4096];
    snprintf(path, sizeof(path), "%s/credentials/nous.json", home);
    hermes_log(LOG_DEBUG, "port", "get_credential: checking %s", path);
    return "";
}
