/* noc_w19_tool_6f4666.h - public C API for tools/connectors/tool.py. */
#ifndef NOC_W19_TOOL_6F4666_H
#define NOC_W19_TOOL_6F4666_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_tool_6f4666_normalize_targets @ tools/connectors/targets.py:normalize_targets
 * -> {"managed":[{"name","alias","to"}], "mcp":[...], "error": str|null} */
json_t *w19_tool_6f4666_normalize_targets(json_t *raw);

/* PoP: w19_tool_6f4666_validate_action @ tools/connectors/targets.py:validate_action
 * (`managed` is the normalized [{"name","alias","to"}] list) -> str|null */
json_t *w19_tool_6f4666_validate_action(const char *action, json_t *managed, json_t *mcp);

/* PoP: w19_tool_6f4666_manage_connections @ tools/connectors/tool.py:manage_connections
 * -> {"error": msg} | {"kind":"managed"|"mcp","action":a,"targets":[...]} */
json_t *w19_tool_6f4666_manage_connections(json_t *args);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_TOOL_6F4666_H */
