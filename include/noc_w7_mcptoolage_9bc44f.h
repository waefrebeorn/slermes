#ifndef NOC_W7_MCPTOOLAGE_9BC44F_H
#define NOC_W7_MCPTOOLAGE_9BC44F_H

#include "hermes_json.h"

/* Public C API for the pure tool-definition helpers of
 * tools/mcp_tool_agent.py (gate w7_mcptoolage_9bc44f). mcptagent_ prefix
 * avoids collisions with the other mcp_tool_* symbols in this repo. */
struct json_t *mcptagent_def_name(const struct json_t *tool_def);
struct json_t *mcptagent_agent_tool_names(const struct json_t *tools);
/* {merged: [...], names: sorted [...]} from _merge_preserving_prefix. */
struct json_t *mcptagent_merge_preserving_prefix(const struct json_t *current_defs,
                                                 const struct json_t *new_defs,
                                                 const struct json_t *registered_names,
                                                 const struct json_t *held);

#endif /* NOC_W7_MCPTOOLAGE_9BC44F_H */
