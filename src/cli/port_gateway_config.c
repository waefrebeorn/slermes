/*
 * port_gateway_config.c — C port of gateway/config.py
 */

#include "hermes_logger.h"
#include "hermes_gateway_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Port of Python gateway/config.py:_missing_ */
/* Resolve a platform name string to enum value. Returns -1 if unknown. */
int cli_gateway_config__missing_(const char *value)
{
    /* Delegate to the gateway_config platform lookup.
     * Iterates the platform table matching the value string. */
    return gateway_config_platform_missing(value);
}

/* Port of Python gateway/config.py:_scan_bundled_plugin_platforms */
int cli_gateway_config__scan_bundled_plugin_platforms(char *names[], int max_names)
{
    /* Delegate to the gateway_config bundled platform scanner.
     * Scans the plugins directory for bundled platform plugins. */
    return gateway_config_scan_bundled_plugin_platforms(names, max_names);
}
