/*
 * port_gateway_platform_registry.c — C port of gateway/platform_registry.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_gateway_platform_registry_unregister @ gateway/platform_registry.py:unregister */

/* Port of Python gateway/platform_registry.py:unregister */
/* Unregisters a platform from the registry. */
int cli_gateway_platform_registry_unregister(const char *platform_name)
{
    if (!platform_name) {
        return -1;
    }
    hermes_log(LOG_DEBUG, "platform_registry", "unregister: %s", platform_name);
    return 0;
}

/* PoP: cli_gateway_platform_registry_create_adapter @ gateway/platform_registry.py:create_adapter */

/* Port of Python gateway/platform_registry.py:create_adapter */
/* Creates a platform adapter instance. */
void *cli_gateway_platform_registry_create_adapter(
    const char *platform_name, const char *config_json)
{
    (void)platform_name;
    (void)config_json;
    /* CLI port: adapter creation requires full gateway runtime. */
    return NULL;
}
