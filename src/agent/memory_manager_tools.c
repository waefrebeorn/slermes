/*
 * memory_manager_tools.c — real C11 port of the pure tool-schema helpers of
 * upstream Hermes agent/memory_manager_tools.py (gate w13_memorymana_3efc6f).
 *
 * PoP: w13_memorymana_3efc6f_normalize_tool_schema @ agent/memory_manager_tools.py:normalize_tool_schema
 * PoP: w13_memorymana_3efc6f_memory_provider_tools_enabled @ agent/memory_manager_tools.py:memory_provider_tools_enabled
 * PoP: w13_memorymana_3efc6f_tool_name @ agent/memory_manager_tools.py:_tool_name
 * PoP: w13_memorymana_3efc6f_memory_provider_tools_exposed @ agent/memory_manager_tools.py:memory_provider_tools_exposed
 *
 * The `toolsets.resolve_toolset` dependency is injected as a JSON object so
 * the port stays pure; a missing name models the live resolve exception.
 */
#include "noc_w13_memorymana_3efc6f.h"

#include <stdlib.h>
#include <string.h>

static int mm_arr_has(const json_t *arr, const char *want) {
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        const json_t *it = json_get(arr, i);
        if (it && it->type == JSON_STRING && it->str_val && !strcmp(it->str_val, want)) return 1;
    }
    return 0;
}

json_t *w13_memorymana_3efc6f_normalize_tool_schema(const json_t *schema) {
    if (!schema || schema->type != JSON_OBJECT) return json_null();
    const json_t *src = schema;
    const json_t *ty = json_obj_get(schema, "type");
    const json_t *fn = json_obj_get(schema, "function");
    if (ty && ty->type == JSON_STRING && ty->str_val && !strcmp(ty->str_val, "function") &&
        fn && fn->type == JSON_OBJECT)
        src = fn;
    const json_t *nm = json_obj_get(src, "name");
    if (nm && nm->type == JSON_STRING && nm->str_val && nm->str_val[0])
        return json_copy(src);
    return json_null();
}

int w13_memorymana_3efc6f_memory_provider_tools_enabled(const json_t *enabled,
        const json_t *disabled, int present, const json_t *resolved) {
    /* JSON null is Python None here. */
    if (enabled && enabled->type == JSON_NULL) enabled = NULL;
    if (disabled && disabled->type == JSON_NULL) disabled = NULL;
    if (disabled && disabled->type == JSON_ARRAY && json_len(disabled) &&
        (mm_arr_has(disabled, "memory") || mm_arr_has(disabled, "all") || mm_arr_has(disabled, "*")))
        return 0;
    if (present || enabled == NULL) return 1;
    if (enabled->type != JSON_ARRAY || json_len(enabled) == 0) return 0;
    if (mm_arr_has(enabled, "memory")) return 1;
    size_t n = json_len(enabled);
    for (size_t i = 0; i < n; i++) {
        const json_t *nm = json_get(enabled, i);
        const char *name = (nm && nm->type == JSON_STRING) ? nm->str_val : NULL;
        /* resolve_toolset(name) -> resolved[name]; failure => False for whole call */
        if (!name || !resolved || resolved->type != JSON_OBJECT) return 0;
        const json_t *arr = json_obj_get(resolved, name);
        if (!arr || arr->type != JSON_ARRAY) return 0;
        if (mm_arr_has(arr, "memory")) return 1;   /* any() short-circuits */
    }
    return 0;
}

json_t *w13_memorymana_3efc6f_tool_name(const json_t *tool) {
    if (!tool || tool->type != JSON_OBJECT) return json_null();
    const json_t *fn = json_obj_get(tool, "function");
    if (!fn || fn->type != JSON_OBJECT) return json_null();
    const json_t *nm = json_obj_get(fn, "name");
    return nm ? json_copy(nm) : json_null();
}

int w13_memorymana_3efc6f_memory_provider_tools_exposed(const json_t *tools,
        const json_t *enabled, const json_t *disabled, const json_t *resolved) {
    int present = 0;
    if (tools && tools->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(tools); i++) {
            json_t *nm = w13_memorymana_3efc6f_tool_name(json_get(tools, i));
            if (json_is_string(nm) && nm->str_val && !strcmp(nm->str_val, "memory")) present = 1;
            json_free(nm);
            if (present) break;
        }
    }
    return w13_memorymana_3efc6f_memory_provider_tools_enabled(enabled, disabled, present, resolved);
}

