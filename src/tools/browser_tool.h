#ifndef SLERMES_PORT_BROWSER_TOOL_H
#define SLERMES_PORT_BROWSER_TOOL_H

#include <stdbool.h>

/* Aggregate opaque state for the browser-tool concern modules.
 * Concrete sub-contexts live in browser_tool_{env,platform,eval,install,path,cdp}.h */
typedef struct port_browser_tool_state port_browser_tool_state_t;

port_browser_tool_state_t *port_browser_tool_init(void);
void port_browser_tool_cleanup(port_browser_tool_state_t *state);

#endif /* SLERMES_PORT_BROWSER_TOOL_H */

/* get_browser_snapshot_threshold — browser.snapshot_threshold from raw
 * profile-aware config, clamped to MIN 1000, cached until reset. */
int tools_browser_tool_get_snapshot_threshold(void);
void tools_browser_tool_reset_snapshot_threshold(void);
