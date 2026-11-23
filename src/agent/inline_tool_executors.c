/*
 * inline_tool_executors.c — faithful C11 port of agent/inline_tool_executors.py.
 * Gate w14_inlinetool_874779.
 *
 * PoP: w14_inlinetool_874779_tool_hook_ids @ agent/inline_tool_executors.py:tool_hook_ids
 * PoP: w14_inlinetool_874779_is_context_engine_tool @ agent/inline_tool_executors.py:is_context_engine_tool
 * PoP: w14_inlinetool_874779_is_memory_provider_tool @ agent/inline_tool_executors.py:is_memory_provider_tool
 * PoP: w14_inlinetool_874779_pre_registry_route @ agent/inline_tool_executors.py:pre_registry_route
 *
 * All agent/lazy-import state is an explicit injected seam (see header): no process
 * environment is mutated and no global mutable state is used.
 */
#include "noc_w14_inlinetool_874779.h"
#include "hermes_json.h"
#include <string.h>

/* INLINE_TOOL_EXECUTORS keys — historical sequential-path order from the source. */
static const char *const INLINE_NAMES[] = {
    "todo_list", "message_agent", "session_search", "memory", "clarify",
    "read_terminal", "desktop_preview", "browser_vault", "drive_preview",
    "annotate_preview", "read_window_below", "gui_tour", "set_theme",
    "setup_beat", "manage_connections", "manage_catalog", "setup_mcp",
    "delegate_task", NULL
};

static int str_in(const char *s, const char *const *set) {
    if (!s || !set) return 0;
    for (int i = 0; set[i]; i++) if (!strcmp(s, set[i])) return 1;
    return 0;
}

static int json_set_has_str(const struct json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *e = json_get(arr, i);
        if (e && e->type == JSON_STRING && e->str_val && !strcmp(e->str_val, s)) return 1;
    }
    return 0;
}

static const char *or_empty(const char *s) { return s ? s : ""; }

struct json_t *w14_inlinetool_874779_tool_hook_ids(const char *effective_task_id,
        const char *session_id, const char *tool_call_id, const char *turn_id,
        const char *api_request_id, const struct json_t *conversation_id) {
    json_t *out = json_object();
    json_set(out, "task_id", json_string(or_empty(effective_task_id)));
    json_set(out, "session_id", json_string(or_empty(session_id)));
    json_set(out, "tool_call_id", json_string(or_empty(tool_call_id)));
    json_set(out, "turn_id", json_string(or_empty(turn_id)));
    json_set(out, "api_request_id", json_string(or_empty(api_request_id)));
    if (conversation_id && conversation_id->type == JSON_STRING)
        json_set(out, "conversation_id", json_copy(conversation_id));
    else
        json_set(out, "conversation_id", json_null());
    return out;
}

int w14_inlinetool_874779_is_context_engine_tool(const struct json_t *names,
        const char *function_name) {
    if (!names || names->type != JSON_ARRAY || names->c.count == 0) return 0;
    return json_set_has_str(names, function_name);
}

int w14_inlinetool_874779_is_memory_provider_tool(int has_manager,
        const struct json_t *tool_names, const char *function_name) {
    if (!has_manager) return 0;
    return json_set_has_str(tool_names, function_name);
}

const char *w14_inlinetool_874779_pre_registry_route(const char *function_name,
        const struct json_t *legacy_aliases, const struct json_t *bridge_names,
        const struct json_t *agent_loop_tools, const struct json_t *connector_names,
        const struct json_t *context_engine_names, const struct json_t *memory_provider_names,
        int has_agent) {
    if (!function_name) return NULL;
    if (json_set_has_str(legacy_aliases, function_name)) return "alias";
    if (json_set_has_str(bridge_names, function_name)) return "bridge";
    if (str_in(function_name, INLINE_NAMES) || json_set_has_str(agent_loop_tools, function_name))
        return "inline";
    if (json_set_has_str(connector_names, function_name)) return "connector";
    if (has_agent && json_set_has_str(context_engine_names, function_name)) return "context_engine";
    if (has_agent && json_set_has_str(memory_provider_names, function_name)) return "memory_provider";
    return NULL;
}
