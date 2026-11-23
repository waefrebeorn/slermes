#ifndef NOC_W15_TOOLSCONFI_129C58_H
#define NOC_W15_TOOLSCONFI_129C58_H

#include "hermes_json.h"

/* PoP port of hermes_cli/tools_config_mcp.py (gate w15_toolsconfi_129c58). */

/* _mcp_preselected(tool_names, include_set|null, exclude_set|null) -> int[]. */
json_t *w15_toolsconfi_129c58_preselected(const json_t *tool_names,
                                         const json_t *include_set,
                                         const json_t *exclude_set);
/* _apply_mcp_change(config, targets, action) -> {failed:[...], config:{...}}. */
json_t *w15_toolsconfi_129c58_apply_mcp_change(const json_t *config,
                                              const json_t *targets,
                                              const char *action);

#endif /* NOC_W15_TOOLSCONFI_129C58_H */
