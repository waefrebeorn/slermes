/*
 * port_tools_read_terminal_tool.c — C port of tools/read_terminal_tool.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_read_terminal_tool_read_terminal_tool @ tools/read_terminal_tool.py:read_terminal_tool */

/* Port of Python tools/read_terminal_tool.py:read_terminal_tool */
/* Return the in-app terminal's contents (+ line metadata) as a JSON string. */
/* Desktop GUI only -- requires HERMES_DESKTOP env var. */
char *cli_tools_read_terminal_tool_read_terminal_tool(
    int start_line, int count, int use_callback)
{
    (void)start_line; (void)count; (void)use_callback;

    /* The Hermes desktop app round-trips terminal reads through the GUI
     * renderer's callback. That renderer is not present in the C port, so
     * there is no terminal to read. Report it honestly rather than returning
     * empty/placeholder contents that would look like a successful read. */
    return strdup("{\"error\":\"read_terminal is only available in the Hermes desktop app.\"}");
}

/* PoP: cli_tools_read_terminal_tool_check_read_terminal_requirements @ tools/read_terminal_tool.py:check_read_terminal_requirements */

/* Port of Python tools/read_terminal_tool.py:check_read_terminal_requirements */
/* Desktop GUI only -- HERMES_DESKTOP is set on the gateway the app spawns. */
int cli_tools_read_terminal_tool_check_read_terminal_requirements(void)
{
    const char *desktop = getenv("HERMES_DESKTOP");
    if (!desktop || !*desktop) return 0;

    /* Check for truthy values */
    if (strcmp(desktop, "1") == 0 || strcasecmp(desktop, "true") == 0 ||
        strcasecmp(desktop, "yes") == 0) {
        return 1;
    }

    return 0;
}
