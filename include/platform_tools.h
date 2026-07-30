/* platform_tools.h — faithful C11 port of the hermes_cli/tools_config.py
 * platform-toolset resolution surface (_get_platform_tools and friends).
 * Config is passed as a parsed json_t (config.yaml via json_parse_yaml).
 */
#ifndef SLERMES_PLATFORM_TOOLS_H
#define SLERMES_PLATFORM_TOOLS_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

/* PoP: platform_tools_enabled_mcp_server_names @ hermes_cli/tools_config.py:enabled_mcp_server_names
 * Names of MCP servers globally enabled in config. Sorted unique list. */
char **platform_tools_enabled_mcp_server_names(const json_t *config,
                                               size_t *out_n);

/* PoP: platform_tools_get_enabled_platforms @ hermes_cli/tools_config.py:_get_enabled_platforms
 * Platform keys that are configured (env-token driven; "cli" always). */
char **platform_tools_get_enabled_platforms(size_t *out_n);

/* PoP: platform_tools_get @ hermes_cli/tools_config.py:_get_platform_tools
 * Resolve enabled toolset names for a platform. Sorted unique list. */
char **platform_tools_get(const json_t *config, const char *platform,
                          bool include_default_mcp_servers, size_t *out_n);

/* PoP: platform_tools_summary @ hermes_cli/tools_config.py:_platform_toolset_summary
 * JSON object {platform: [toolsets...]} for the given platforms (or
 * auto-detected when platforms==NULL). Caller json_free()s. */
json_t *platform_tools_summary(const json_t *config,
                               const char *const *platforms, size_t n_platforms);

/* PoP: platform_tools_configuration_platform @ hermes_cli/tools_config.py:_toolset_configuration_platform
 * Returns malloc'd platform key a platform-less config UI should target. */
char *platform_tools_configuration_platform(const char *ts_key,
                                            const char *default_platform);

/* PoP: platform_tools_save @ hermes_cli/tools_config.py:_save_platform_tools
 * Mutates config: platform_toolsets.<platform> = sorted(enabled|preserved),
 * updates known_plugin_toolsets and reconciles agent.disabled_toolsets.
 * Does NOT write the file (caller persists). */
void platform_tools_save(json_t *config, const char *platform,
                         const char *const *enabled_keys, size_t n_enabled);

/* Configurable toolset keys (CONFIGURABLE_TOOLSETS). NULL-terminated. */
const char *const *platform_tools_configurable_keys(void);

/* Default-off toolsets (_DEFAULT_OFF_TOOLSETS). NULL-terminated. */
const char *const *platform_tools_default_off(void);

/* Platform key -> default toolset (PLATFORMS registry). NULL when unknown
 * (caller falls back to "hermes-<platform>"). */
const char *platform_tools_default_toolset(const char *platform);

void platform_tools_free_list(char **list, size_t n);

#endif /* SLERMES_PLATFORM_TOOLS_H */
