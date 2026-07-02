/*
 * port_agent_web_search_registry.c — C port of agent/web_search_registry.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include "web_search_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Port of Python agent/web_search_registry.py:get_active_search_provider */
const web_search_provider_t *cli_agent_web_search_registry_get_active_search_provider(void)
{
    /* Delegate to the web_search module's active provider lookup.
     * Returns the currently configured search provider, or NULL if none. */
    return web_search_get_active("search");
}

/* Port of Python agent/web_search_registry.py:get_active_extract_provider */
const web_search_provider_t *cli_agent_web_search_registry_get_active_extract_provider(void)
{
    /* Delegate to the web_search module's active provider lookup.
     * Returns the currently configured extract provider, or NULL if none. */
    return web_search_get_active("extract");
}
