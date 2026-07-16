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

/* Refresh availability of all tools that have check_fn registered.
 * Caches results for 30 seconds (check_fn_last). */
void registry_refresh_availability(void);

/* S14 gap #11: Deregister a tool by name. Used for MCP dynamic tool removal.
 * Returns true if tool was found and removed, false if not found. */
bool registry_deregister(const char *name);

/* S14 gap #16: Rich query API — get schema JSON for a tool (returns "" if not found) */
const char *registry_get_schema(const char *name);
/* Rich query: return display emoji for a tool, or default (⚡) if unset */
const char *registry_get_emoji(const char *name, const char *default_emoji);

/* S14 gap #16: Rich query API — check if any tool in a toolset is available */
bool registry_is_toolset_available(const char *toolset);

/* S14 gap #2: Tool Search bridge — search tools by keyword (name/description).
 * Returns JSON array of matching tool names, or ["error":"..."] on failure.
 * Caller must free the returned string. */
char *registry_search(const char *keyword);

/* S14 gap #2: Tool Search bridge — describe a tool.
 * Returns JSON object with name, description, schema, toolset, or {"error":"..."}.
 * Caller must free the returned string. */
char *registry_describe(const char *name);

/* S14 gap #8: Mark a tool as async (handlers that should run in detached thread) */
void registry_set_async(const char *name, bool async);

/* Check if tool name matches a wildcard pattern. Returns true on match. */
bool registry_name_matches(const char *name, const char *pattern);

#ifdef __cplusplus
}
#endif

#endif /* REGISTRY_H */
