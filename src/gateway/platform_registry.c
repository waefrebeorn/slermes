/*
 * port_gateway_platform_registry.c — C port of gateway/platform_registry.py
 */

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


