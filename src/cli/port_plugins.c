#ifndef SRC_CLI_PORT_PLUGINS_C
#define SRC_CLI_PORT_PLUGINS_C

#include "hermes.h"
#include "hermes_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Port of Python: profile_name */
const char *profile_name(void *ctx)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "profile_name: null context");
        return NULL;
    }
    const char *profile = getenv("HERMES_PROFILE");
    if (!profile) profile = "default";
    hermes_log(LOG_DEBUG, "port", "profile_name: %s", profile);
    return profile;
}

#endif /* SRC_CLI_PORT_PLUGINS_C */
