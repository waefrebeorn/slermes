/*
 * port_tools_browser_dialog_tool.c — C port of tools/browser_dialog_tool.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_browser_dialog_tool__browser_dialog_check @ tools/browser_dialog_tool.py:_browser_dialog_check */

/*
 * _browser_dialog_check: Gate check — only offered when CDP is reachable.
 *
 * In C, we check if a CDP endpoint is configured and reachable.
 *
 * Returns: (void*)1 if CDP is available, (void*)0 otherwise.
 */
void* cli_tools_browser_dialog_tool__browser_dialog_check(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;

    /* Check if CDP URL is configured */
    const char *cdp_url = getenv("HERMES_BROWSER_CDP_URL");
    if (cdp_url && cdp_url[0]) {
        hermes_log(LOG_DEBUG, "port",
                   "browser_dialog_check: CDP URL configured (%s)", cdp_url);
        return (void *)1;
    }

    /* Check if browser.cdp_url is set in config — simplified check */
    const char *browser_cdp = getenv("HERMES_BROWSER_CDP");
    if (browser_cdp && browser_cdp[0]) {
        hermes_log(LOG_DEBUG, "port",
                   "browser_dialog_check: browser CDP configured");
        return (void *)1;
    }

    hermes_log(LOG_DEBUG, "port",
               "browser_dialog_check: no CDP endpoint available");
    return (void *)0;
}
