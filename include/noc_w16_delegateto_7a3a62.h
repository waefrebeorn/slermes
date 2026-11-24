#ifndef NOC_W16_DELEGATETO_7A3A62_H
#define NOC_W16_DELEGATETO_7A3A62_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of tools/delegate_tool_toolsets.py (gate w16_delegateto_7a3a62).
 * Static dependencies (TOOLSETS / resolve_toolset / alias targets) are supplied
 * by the fixture so the pure decision logic can be exercised faithfully. */

/* _is_mcp_toolset_name(name): alias_target is the registry alias target or NULL. */
json_t *w16_delegateto_7a3a62_is_mcp_toolset_name(const char *name, const char *alias_target);

/* _blocked_toolsets_for_role(role): toolsets maps name -> {"tools":[...]}. */
json_t *w16_delegateto_7a3a62_blocked_toolsets_for_role(const char *role, const json_t *toolsets);

/* _strip_blocked_tools(toolsets): resolved maps name -> [tool,...]. */
json_t *w16_delegateto_7a3a62_strip_blocked_tools(const json_t *toolsets, const json_t *resolved);

/* _expand_parent_toolsets(parent): resolved maps name -> [tool,...]. Returns sorted names. */
json_t *w16_delegateto_7a3a62_expand_parent_toolsets(const json_t *parent, const json_t *resolved);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_DELEGATETO_7A3A62_H */
