/*
 * registry.h — Public API for the tool registry.
 *
 * Faithful extraction from the monolithic hermes.h god header (the
 * god-header-elimination pass). The registry owns tool registration,
 * discovery, toolset availability, per-tool timeouts, and wildcard
 * matching. This header is the module's single source of truth so that
 * translation units can include it directly instead of dragging in the
 * entire master header.
 *
 * Self-contained: includes only the core type header (for tool_t /
 * tool_registry_t) and the C standard headers it actually needs.
 */

#ifndef REGISTRY_H
#define REGISTRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hermes_core_types.h"   /* tool_t, tool_registry_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Registry accessors */
size_t registry_get_count(void);
const char *registry_get_name(size_t i);

/* Generation counter — bumped on every mutation.
 * Callers can cache tool metadata and check generation for staleness. */
uint64_t registry_generation(void);

/* Find tool by name in registry. Returns NULL if not found. */
tool_t *registry_find(const char *name);

/* P52: Per-tool timeout. Set seconds, 0 = default, -1 = no timeout. */
void registry_set_timeout(const char *name, int seconds);
/* P150: Filter tool registry by enabled/disabled toolsets. Marks matching tools unavailable. */
void registry_filter_by_toolset(tool_registry_t *reg, const char *enabled_csv, const char *disabled_csv);
/* P150: Get toolset name for a registered tool. Returns "" if not set. */
const char *registry_get_toolset(const char *name);
/* P150: Set toolset name for a registered tool (after registration). */
void registry_set_toolset(const char *name, const char *toolset);
int  registry_get_timeout(const char *name);

/* P55: Tool wildcard matching — enable/disable all tools matching a pattern */
/* Pattern supports '*' wildcard: "discord:*", "browser_*", "*_search" */
void registry_set_available_pattern(const char *pattern, bool available);

/* S14 gap #9: Toolset availability check — register check function for toolset.
 * Once set, registry_refresh_availability() calls check_fn (with 30s cache) to
 * mark tools in this toolset as available/unavailable. */
void registry_set_toolset_check_fn(const char *toolset, bool (*fn)(void));

/* Per-tool availability check — register check_fn for a single tool (mirrors
 * Python's per-tool check_fn on registry.register). Used by desktop-only tools
 * like close_terminal that must not appear outside the GUI. */
void registry_set_check_fn(const char *name, bool (*fn)(void));

/* Refresh availability of all tools that have check_fn registered.
 * Caches results for 30 seconds (check_fn_last). */
void registry_refresh_availability(void);

/* Per-tool availability getter (mirrors Python registry.is_available). */
bool registry_is_available(const char *name);

/* Wildcard name matcher (e.g. "browser*" matches "browser_tool_eval").
 * Exact match when pattern has no '*'; prefix match for "prefix*". */
bool registry_name_matches(const char *name, const char *pattern);

/* S14 gap #11: Deregister a tool by name. Used for MCP dynamic tool removal.
 * Returns true if tool was found and removed, false if not found. */
bool registry_deregister(const char *name);

/* S14 gap #16: Rich query API — get schema JSON for a tool (returns "" if not found) */
const char *registry_get_schema(const char *name);
/* Rich query: return display emoji for a tool, or default (⚡) if unset */
const char *registry_get_emoji(const char *name, const char *default_emoji);

/* S14 gap #16: Rich query API — check if any tool in a toolset is available */
bool registry_is_toolset_available(const char *toolset);

/* Full registration (port of Python ToolRegistry.register): accepts the
 * requires_env list + per-tool max result size. requires_env_n is the number
 * of entries in the requires_env array (may be 0/NULL). */
bool registry_register_ex_full(const char *name, const char *description,
                               const char *schema_json, const char *toolset,
                               char *(*handler)(const char *args_json, const char *task_id),
                               const char *const *requires_env, size_t requires_env_n,
                               int max_result_size_chars);

/* --- Toolset enumeration + alias API (port of Python ToolRegistry) --------- */

/* Sorted unique toolset names present in the registry.
 * Returns a NULL-terminated char* array; caller frees each + the array. */
char **registry_get_registered_toolset_names(size_t *out_n);
/* Sorted tool names registered under a given toolset. Caller frees. */
char **registry_get_tool_names_for_toolset(const char *toolset, size_t *out_n);
/* Sorted names of every registered tool. Caller frees. */
char **registry_get_all_tool_names(size_t *out_n);
/* {tool_name: toolset_name} JSON object for every registered tool. Caller frees. */
char *registry_get_tool_to_toolset_map(void);
/* Toolset a tool belongs to, or NULL (borrowed, do not free). */
const char *registry_get_toolset_for_tool(const char *name);

/* Register an explicit alias -> canonical toolset mapping (overwrites prior). */
void registry_register_toolset_alias(const char *alias, const char *toolset);
/* Canonical toolset name for an alias, or NULL (borrowed). */
const char *registry_get_toolset_alias_target(const char *alias);
/* JSON {"alias": "toolset"} snapshot of all alias mappings. Caller frees. */
char *registry_get_registered_toolset_aliases(void);

/* Default max result size (chars) when a tool doesn't set its own.
 * Mirrors tools/budget_config.DEFAULT_RESULT_SIZE_CHARS. */
#define REGISTRY_DEFAULT_RESULT_SIZE_CHARS 100000

/* Return per-tool max result size, or *default*, or the global default. */
int registry_get_max_result_size(const char *name, int default_size);

/* {toolset: available_bool} for every registered toolset (JSON object). */
char *registry_check_toolset_requirements(void);
/* {toolset: {available, tools[]}} metadata for UI display (JSON object). */
char *registry_get_available_toolsets(void);
/* {toolset: {name, env_vars[], check_fn, setup_url, tools[]}} (JSON object). */
char *registry_get_toolset_requirements(void);

/* S14 gap #2: Tool Search bridge — search tools by keyword (name/description).
 * Returns JSON array of matching tool names, or ["error":"..."] on failure.
 * Caller must free the returned string. */
char *registry_search(const char *keyword);

/* S14 gap #2: Tool Search bridge — describe a tool.
 * Returns JSON object with name, description, schema, toolset, or {"error":"..."}.
 * Caller must free the returned string. */
char *registry_describe(const char *name);

/* Register a tool with the given name/description/schema and handler.
 * Registers into the "" (default) toolset. */
bool registry_register(const char *name, const char *description,
                        const char *schema_json,
                        char *(*handler)(const char *args_json, const char *task_id));

/* Register a tool with an explicit toolset. */
bool registry_register_ex(const char *name, const char *description,
                           const char *schema_json, const char *toolset,
                           char *(*handler)(const char *args_json, const char *task_id));

/* ── Discovery cache + check_fn verdict cache (port of registry.py module fns) */
char *discovery_cache_path(void);
json_t *load_discovery_cache(void);
void save_discovery_cache(const json_t *cache);
char *check_fn_cache_scope(void);
bool get_cached_check_fn_result(void *fn, bool *hit, bool default_value);
bool check_fn_cached(bool (*fn)(void));
void invalidate_check_fn_cache(void);

#ifdef __cplusplus
}
#endif

#endif /* REGISTRY_H */
