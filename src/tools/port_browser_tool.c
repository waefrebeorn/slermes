#ifndef SRC_TOOLS_PORT_BROWSER_TOOL_C
#define SRC_TOOLS_PORT_BROWSER_TOOL_C

#include "hermes.h"
#include "hermes_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Port of Python: browser_console */
const char* browser_console(void* ctx, void* clear, void* expression, void* task_id)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "browser_console: null context");
        return NULL;
    }
    hermes_log(LOG_DEBUG, "port", "browser_console called");
    bool has_clear = (clear != NULL);
    bool has_expr = (expression != NULL);
    bool has_task = (task_id != NULL);
    (void)has_task; /* unused but kept for clarity */
    /* Minimal implementation - browser console is a browser-specific feature
     * that requires WebDriver/CDP which is handled in browser_camofox.c */
    char *result = malloc(128);
    if (result) {
        snprintf(result, 128, "{\"console\":\"browser_console_not_implemented_in_port\",\"clear\":%s,\"expression\":%s}",
                 has_clear ? "true" : "false", has_expr ? "true" : "false");
    }
    return result;
}

#endif /* SRC_TOOLS_PORT_BROWSER_TOOL_C */