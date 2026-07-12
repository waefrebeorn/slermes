/*
 * port_tools_tool_backend_helpers.c — C port of tools/tool_backend_helpers.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_tool_backend_helpers_nous_tool_gateway_unavailable_message @ tools/tool_backend_helpers.py:nous_tool_gateway_unavailable_message */

/* Port of Python tools/tool_backend_helpers.py:nous_tool_gateway_unavailable_message */
/* Return account-aware guidance for an unavailable Nous Tool Gateway path. */
char *cli_tools_tool_backend_helpers_nous_tool_gateway_unavailable_message(
    const char *capability)
{
    if (!capability) capability = "the Nous Tool Gateway";

    /* In a full implementation, this would:
     * 1. Check Nous Portal account info
     * 2. Format entitlement message
     * 3. Return account-specific guidance
     *
     * For now, return a generic message.
     */
    size_t buf_size = strlen(capability) + 256;
    char *msg = (char *)malloc(buf_size);
    if (!msg) return NULL;

    snprintf(msg, buf_size,
        "%s is unavailable. Run `hermes model` to refresh your "
        "Nous Portal login and billing status.",
        capability);

    return msg;
}

/* PoP: cli_tools_tool_backend_helpers_fal_key_is_configured @ tools/tool_backend_helpers.py:fal_key_is_configured */

/* Port of Python tools/tool_backend_helpers.py:fal_key_is_configured */
/* Return 1 when FAL_KEY is set to a non-whitespace value. */
int cli_tools_tool_backend_helpers_fal_key_is_configured(void)
{
    const char *value = getenv("FAL_KEY");
    if (!value || !*value) return 0;

    /* Check for whitespace-only value */
    const char *p = value;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p == '\0') return 0;

    return 1;
}
