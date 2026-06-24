#ifndef SRC_CLI_PORT_MEMORY_PROVIDERS_C
#define SRC_CLI_PORT_MEMORY_PROVIDERS_C

#include "hermes.h"
#include "hermes_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Port of Python: is_secret */
bool is_secret(void *ctx)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "is_secret: null context");
        return false;
    }
    const char *name = (const char *)ctx;
    const char *val = getenv(name);
    bool secret = (val != NULL && val[0] != '\0');
    hermes_log(LOG_DEBUG, "port", "is_secret: %s=%s", name, secret ? "true" : "false");
    return secret;
}

#endif /* SRC_CLI_PORT_MEMORY_PROVIDERS_C */
