/* toolsets.h — faithful C11 port of toolsets.py (toolset alias/composition
 * system). Static TOOLSETS definitions + recursive resolution with cycle
 * detection, registry merge, custom toolsets. All returned arrays are
 * malloc'd arrays of malloc'd strings; free with toolsets_free_list().
 */
#ifndef SLERMES_TOOLSETS_H
#define SLERMES_TOOLSETS_H

#include <stdbool.h>
#include <stddef.h>

/* Static definition view of one toolset. */
typedef struct {
    const char *name;
    const char *description;
    const char *const *tools;    /* NULL-terminated */
    const char *const *includes; /* NULL-terminated */
    bool posture;                /* posture toolsets (e.g. "coding") */
} toolset_def_t;

/* PoP: toolsets_get_static @ toolsets.py:get_toolset
 * Static (include_registry=False) lookup. Returns NULL when unknown.
 * Custom toolsets (toolsets_create_custom) are also visible here. */
const toolset_def_t *toolsets_get_static(const char *name);

/* PoP: toolsets_resolve @ toolsets.py:resolve_toolset
 * Recursive resolution with cycle detection; "all"/"*" resolve every
 * toolset. include_registry merges registry-registered tools (via
 * registry_get_tool_names_for_toolset). Sorted unique list. */
char **toolsets_resolve(const char *name, bool include_registry,
                        size_t *out_n);

/* PoP: toolsets_resolve_multiple @ toolsets.py:resolve_multiple_toolsets */
char **toolsets_resolve_multiple(const char *const *names, size_t n_names,
                                 size_t *out_n);

/* PoP: toolsets_validate @ toolsets.py:validate_toolset */
bool toolsets_validate(const char *name);

/* PoP: toolsets_get_names @ toolsets.py:get_toolset_names
 * Sorted names of all static + custom toolsets (registry plugin toolsets
 * included when the registry is loaded). */
char **toolsets_get_names(size_t *out_n);

/* PoP: toolsets_bundle_non_core_tools @ toolsets.py:bundle_non_core_tools
 * A hermes-* bundle's platform-specific tools excluding core. */
char **toolsets_bundle_non_core_tools(const char *toolset_name, size_t *out_n);

/* PoP: toolsets_create_custom @ toolsets.py:create_custom_toolset
 * Register a runtime custom toolset (copied). */
void toolsets_create_custom(const char *name, const char *description,
                            const char *const *tools, size_t n_tools,
                            const char *const *includes, size_t n_includes);

/* Core tool list (mirrors _HERMES_CORE_TOOLS). NULL-terminated. */
const char *const *toolsets_core_tools(void);

void toolsets_free_list(char **list, size_t n);

#endif /* SLERMES_TOOLSETS_H */
