/*
 * noc_w26_pluginsact_d4fbe5.h - C surface for upstream
 * hermes_cli/plugins_activation_live.py (gate w26_pluginsact_d4fbe5).
 *
 * What an installed plugin leaves live in an open chat: the note rendered on the
 * next tool result (`live_notice`), the tool-name prefix every MCP server's
 * registered tools share (`_tool_prefix`), and the resource/prompt wrapper
 * suffixes that are excluded from counts.  Connecting servers is a seam; only
 * the values those connections produce are ported here.
 */
#ifndef NOC_W26_PLUGINSACT_D4FBE5_H
#define NOC_W26_PLUGINSACT_D4FBE5_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* tools/mcp_tool_registration.py: _UTILITY_HANDLER_FACTORIES, in declaration order. */
#define W26_PLUGINSACT_D4FBE5_UTILITY_SUFFIXES 7

/* activation -> the chat note ("" when nothing of the plugin is live). */
json_t *w26_pluginsact_d4fbe5_live_notice(const json_t *activation);

/* req: {"server": <any>} -> "mcp__<sanitized>__". */
json_t *w26_pluginsact_d4fbe5_tool_prefix(const json_t *req);

/* The suffix/prefix literals, as an object. */
json_t *w26_pluginsact_d4fbe5_constants(void);

#ifdef __cplusplus
}
#endif
#endif
