#ifndef SRC_TOOLS_PORT_ENVIRONMENTS_BASE_C
#define SRC_TOOLS_PORT_ENVIRONMENTS_BASE_C

#include "hermes.h"
#include "hermes_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Port of Python: poll */
void env_poll(void* ctx)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "env_poll: null context");
        return;
    }
    hermes_log(LOG_DEBUG, "port", "env_poll called");
    /* TODO: implement poll logic */
    return;
}

/* Port of Python: kill */
void env_kill(void* ctx)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "env_kill: null context");
        return;
    }
    hermes_log(LOG_DEBUG, "port", "env_kill called");
    /* TODO: implement kill logic */
    return;
}

#endif /* SRC_TOOLS_PORT_ENVIRONMENTS_BASE_C */
