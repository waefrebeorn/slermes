#ifndef SLERMES_PORT_TOOLS_BROWSER_CDP_TOOL_H
#define SLERMES_PORT_TOOLS_BROWSER_CDP_TOOL_H

#include <stddef.h>

/*
 * port_tools_browser_cdp_tool.h — public API for the browser CDP client.
 *
 * Focused module header (no god header): declares only the two CDP primitives
 * the rest of the tree needs to drive a live browser over DevTools WebSocket.
 */

/* Resolve the live CDP WebSocket endpoint. Returns a malloc'd URL string, or
 * NULL if none is configured (set BROWSER_CDP_URL or config browser.cdp_url).
 * Caller frees. */
char *browser_cdp_tool__resolve_cdp_endpoint(void);

/* Issue one CDP command over WebSocket. ws_url is the DevTools endpoint,
 * method is e.g. "Runtime.evaluate", params is a JSON object string (or ""/{}
 * for none), target_id scopes the command to a session (OOPIF frame) or NULL.
 * timeout is in seconds. Returns a malloc'd JSON string
 * {"success":bool,"method":...,"result":<raw CDP response>} on success, or
 * NULL on transport failure. Caller frees. */
char *browser_cdp_tool__cdp_call(const char *ws_url, const char *method,
                                  const char *params, const char *target_id,
                                  double timeout);

#endif /* SLERMES_PORT_TOOLS_BROWSER_CDP_TOOL_H */
