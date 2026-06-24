/*
 * port_tools_openrouter_client.c — C port of tools/openrouter_client.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_openrouter_client_check_api_key @ tools/openrouter_client.py:check_api_key */

/* Port of Python tools/openrouter_client.py:check_api_key */
/* Check whether the OpenRouter API key is present. */
int cli_tools_openrouter_client_check_api_key(void)
{
    const char *key = getenv("OPENROUTER_API_KEY");
    return (key && *key) ? 1 : 0;
}

/* PoP: cli_tools_openrouter_client_get_async_client @ tools/openrouter_client.py:get_async_client */

/* Port of Python tools/openrouter_client.py:get_async_client */
/* Return a shared async OpenAI-compatible client for OpenRouter. */
/* In C, we return a handle to the provider client if the key is set. */
/* Full async client requires the auxiliary_client module integration. */
void *cli_tools_openrouter_client_get_async_client(void)
{
    /* Check if API key is available */
    if (!cli_tools_openrouter_client_check_api_key()) {
        hermes_log(LOG_WARNING, "openrouter", "OPENROUTER_API_KEY not set");
        return NULL;
    }

    /* In a full implementation, this would create/reuse an async OpenAI client
     * via the provider router. For now, return a non-NULL sentinel indicating
     * the client could be created (key is present). */
    hermes_log(LOG_DEBUG, "openrouter", "get_async_client: key available, returning handle");
    return (void *)(uintptr_t)1; /* sentinel: client available */
}
