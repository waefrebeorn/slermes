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
    if (!use_callback) {
        return strdup("{\"error\":\"read_terminal is only available in the Hermes desktop app.\"}");
    }

    /* In a full implementation, this would call the desktop GUI callback
     * to get the terminal contents. For now, return a placeholder JSON. */
    char *json = (char *)malloc(512);
    if (!json) return NULL;

    snprintf(json, 512,
        "{\"total_lines\":0,\"start\":%d,\"end\":%d,\"viewport_rows\":0,"
        "\"cursor_row\":0,\"text\":\"\"}",
        start_line, start_line + count);

    return json;
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
