/* tools_config_toolsets.h — faithful C11 port of the toolset-enumeration and
 * toolset-state surface of hermes_cli/tools_config.py.
 *
 * Reuses platform_tools.h (_get_platform_tools / _save_platform_tools /
 * CONFIGURABLE_TOOLSETS) and tools_config_helpers.h
 * (_toolset_allowed_for_platform). Opaque, minimal includes, C11 only.
 */
#ifndef SLERMES_TOOLS_CONFIG_TOOLSETS_H
#define SLERMES_TOOLS_CONFIG_TOOLSETS_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

/* One CONFIGURABLE_TOOLSETS row: (key, label, description). */
typedef struct {
    const char *key;
    const char *label;
    const char *description;
} tools_config_toolset_row_t;

/* CONFIGURABLE_TOOLSETS as a NULL-key-terminated table (borrowed). */
const tools_config_toolset_row_t *tools_config_configurable_toolsets(void);

/* PoP: tools_config_get_effective_configurable_toolsets @ hermes_cli/tools_config.py:_get_effective_configurable_toolsets
 * CONFIGURABLE_TOOLSETS plus plugin toolsets appended, keys deduped with the
 * built-in label winning. Rows are malloc'd; free with
 * tools_config_free_toolset_rows(). */
tools_config_toolset_row_t *tools_config_get_effective_configurable_toolsets(
    size_t *out_n);
void tools_config_free_toolset_rows(tools_config_toolset_row_t *rows, size_t n);

/* PoP: tools_config_checklist_toolset_keys @ hermes_cli/tools_config.py:_checklist_toolset_keys
 * The keys the `hermes tools` checklist actually offers for `platform`:
 * effective configurable toolsets, platform-allowed, minus _CONFIG_ONLY_TOOLSETS
 * ({"stt"}). Sorted unique. Caller frees with tools_config_free_list(). */
char **tools_config_checklist_toolset_keys(const char *platform, size_t *out_n);

/* PoP: tools_config_apply_toolset_change @ hermes_cli/tools_config.py:_apply_toolset_change
 * action is "disable" (remove) or anything else (add). Mutates config in
 * place via platform_tools_save; does not persist the file. */
void tools_config_apply_toolset_change(json_t *config, const char *platform,
                                       const char *const *toolset_names,
                                       size_t n_toolset_names,
                                       const char *action);

/* PoP: tools_config_toolset_enabled_for_reconfigure @ hermes_cli/tools_config.py:_toolset_enabled_for_reconfigure
 * True when the toolset is enabled on any platform that allows it. */
bool tools_config_toolset_enabled_for_reconfigure(const char *ts_key,
                                                  const json_t *config);

/* PoP: tools_config_known_tool_platforms @ hermes_cli/tools_config.py:_known_tool_platforms
 * Built-in PLATFORMS plus registered plugin platform names. Sorted unique. */
char **tools_config_known_tool_platforms(size_t *out_n);

/* PoP: tools_config_web_provider_capabilities @ hermes_cli/tools_config.py:web_provider_capabilities
 * ["search"], ["extract"], or both. Unregistered backends fall back to both.
 * Sorted per the Python append order (search before extract). */
char **tools_config_web_provider_capabilities(const char *backend,
                                              size_t *out_n);

/* PoP: tools_config_detect_active_provider_index @ hermes_cli/tools_config.py:_detect_active_provider_index
 * Index of the first active provider, else the first whose env_vars are all
 * set, else 0. `providers` is the JSON provider-row array. */
size_t tools_config_detect_active_provider_index(const json_t *providers,
                                                 const json_t *config);

/* PoP: tools_config_estimate_tool_tokens @ hermes_cli/tools_config.py:_estimate_tool_tokens
 * {tool_name: token_count} over the registry's OpenAI-format schemas, using
 * the same {"type":"function","function":<schema>} envelope the API receives.
 * Empty object when the registry is unavailable. Caller json_free()s. */
json_t *tools_config_estimate_tool_tokens(void);

/* PoP: tools_config_module_installed @ hermes_cli/tools_config.py:_module_installed
 * Availability probe for an optional runtime dependency (the C analog of
 * importlib.util.find_spec: is the capability present on this system). */
bool tools_config_module_installed(const char *module_name);

/* PoP: tools_config_restorable_python_tool_dependency @ hermes_cli/tools_config.py:restorable_python_tool_dependency
 * Import probe + pip args for an allowlisted tool dependency, or false when
 * the name isn't allowlisted. Both out params are borrowed. */
bool tools_config_restorable_python_tool_dependency(
    const char *name, const char **out_module,
    const char *const **out_install_args);

/* PoP: tools_config_active_restorable_python_tool_dependencies @ hermes_cli/tools_config.py:active_restorable_python_tool_dependencies
 * Allowlisted dependency names present in this runtime, in declaration order. */
char **tools_config_active_restorable_python_tool_dependencies(size_t *out_n);

/* PoP: tools_config_homeassistant_credentials_present @ hermes_cli/tools_config.py:_homeassistant_credentials_present */
bool tools_config_homeassistant_credentials_present(void);

/* PoP: tools_config_hidden_nous_gateway_message @ hermes_cli/tools_config.py:_hidden_nous_gateway_message
 * Deprecated no-op: Nous Tool Gateway rows are never hidden. Always "". */
const char *tools_config_hidden_nous_gateway_message(void);

/* PoP: tools_config_toolset_has_keys @ hermes_cli/tools_config.py:_toolset_has_keys
 * True when the toolset's required API keys are configured (config NULL loads
 * from disk; force_fresh bypasses the portal cache). */
bool tools_config_toolset_has_keys(const char *ts_key, const json_t *config,
                                   bool force_fresh);

/* PoP: tools_config_enable_recently_shipped_toolsets @ hermes_cli/tools_config.py:_enable_recently_shipped_toolsets */
void tools_config_enable_recently_shipped_toolsets(json_t *enabled_toolsets,
                                                   const json_t *config,
                                                   const char *platform);

/* PoP: tools_config_prompt_toolset_checklist @ hermes_cli/tools_config.py:_prompt_toolset_checklist */
char **tools_config_prompt_toolset_checklist(const char *platform_label,
                                             const json_t *enabled,
                                             const char *platform,
                                             bool force_fresh,
                                             size_t *out_n);

/* PoP: tools_config_configure_toolset @ hermes_cli/tools_config.py:_configure_toolset */
void tools_config_configure_toolset(const char *ts_key, json_t *config,
                                    bool force_fresh);

/* PoP: tools_config_get_plugin_toolset_keys @ hermes_cli/tools_config.py:_get_plugin_toolset_keys
 * Plugin-provided toolset keys (non-blocking). Sorted unique. */
char **tools_config_get_plugin_toolset_keys(size_t *out_n);

/* PoP: tools_config_valid_post_setup_keys @ hermes_cli/tools_config.py:valid_post_setup_keys
 * The post_setup allowlist: every key declared by a visible provider row.
 * Sorted unique. */
char **tools_config_valid_post_setup_keys(size_t *out_n);

void tools_config_free_list(char **list, size_t n);

#endif /* SLERMES_TOOLS_CONFIG_TOOLSETS_H */
