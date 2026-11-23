#ifndef NOC_W13_MEMORYMANA_3EFC6F_H
#define NOC_W13_MEMORYMANA_3EFC6F_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* C11 port of the pure tool-schema helpers of agent/memory_manager_tools.py
 * (gate w13_memorymana_3efc6f). The live code's resolve_toolset() dependency is
 * injected as `resolved` (JSON object: toolset name -> array of tool names);
 * a missing entry models a resolve failure. Returned json_t trees are owned
 * by the caller (json_free). Ints are 1/0 booleans. */

struct json_t *w13_memorymana_3efc6f_normalize_tool_schema(const struct json_t *schema);
int w13_memorymana_3efc6f_memory_provider_tools_enabled(const struct json_t *enabled,
        const struct json_t *disabled, int memory_tool_present, const struct json_t *resolved);
struct json_t *w13_memorymana_3efc6f_tool_name(const struct json_t *tool);
int w13_memorymana_3efc6f_memory_provider_tools_exposed(const struct json_t *tools,
        const struct json_t *enabled, const struct json_t *disabled,
        const struct json_t *resolved);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W13_MEMORYMANA_3EFC6F_H */
