/* tools_config_reconfig.h — faithful C11 port of the reconfigure / MCP /
 * listing surface of hermes_cli/tools_config.py:
 * _reconfigure_tool, _configure_tool_category_for_reconfig,
 * _reconfigure_provider, _reconfigure_simple_requirements,
 * _configure_mcp_tools_interactive, _apply_mcp_change, _print_tools_list,
 * tools_disable_enable_command.
 */
#ifndef SLERMES_TOOLS_CONFIG_RECONFIG_H
#define SLERMES_TOOLS_CONFIG_RECONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

void tools_config_reconfigure_tool(json_t *config, bool force_fresh);
void tools_config_configure_tool_category_for_reconfig(const char *ts_key,
                                                       const json_t *cat,
                                                       json_t *config,
                                                       bool force_fresh);
void tools_config_reconfigure_provider(const json_t *provider, json_t *config,
                                       bool force_fresh);
void tools_config_reconfigure_simple_requirements(const char *ts_key);
void tools_config_configure_mcp_tools_interactive(json_t *config);
/* Returns the set of server names not found in config (fresh json array). */
json_t *tools_config_apply_mcp_change(json_t *config, const char *const *targets,
                                      size_t n_targets, const char *action);
void tools_config_print_tools_list(const json_t *enabled_toolsets,
                                   const json_t *mcp_servers,
                                   const char *platform);
/* action: "enable" | "disable" | "list". args carried as plain values:
 * platform + names array. */
void tools_config_disable_enable_command(const char *action,
                                         const char *platform,
                                         const char *const *names,
                                         size_t n_names);

#endif /* SLERMES_TOOLS_CONFIG_RECONFIG_H */
