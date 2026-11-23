#ifndef NOC_W14_INLINETOOL_874779_H
#define NOC_W14_INLINETOOL_874779_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_inlinetool_874779_tool_hook_ids @ agent/inline_tool_executors.py:tool_hook_ids
 * Identity kwargs dict. The five identity arguments mirror the agent attributes and
 * receive Python `x or ""`; conversation_id is injected as-is (may be JSON null). */
struct json_t *w14_inlinetool_874779_tool_hook_ids(const char *effective_task_id,
        const char *session_id, const char *tool_call_id, const char *turn_id,
        const char *api_request_id, const struct json_t *conversation_id);

/* PoP: w14_inlinetool_874779_is_context_engine_tool @ agent/inline_tool_executors.py:is_context_engine_tool
 * names = agent._context_engine_tool_names (JSON array or null). */
int w14_inlinetool_874779_is_context_engine_tool(const struct json_t *names,
        const char *function_name);

/* PoP: w14_inlinetool_874779_is_memory_provider_tool @ agent/inline_tool_executors.py:is_memory_provider_tool
 * has_manager is bool(agent._memory_manager); tool_names is manager's tool set. */
int w14_inlinetool_874779_is_memory_provider_tool(int has_manager,
        const struct json_t *tool_names, const char *function_name);

/* PoP: w14_inlinetool_874779_pre_registry_route @ agent/inline_tool_executors.py:pre_registry_route
 * Returns a static string: "alias"/"bridge"/"inline"/"connector"/"context_engine"/
 * "memory_provider", or NULL (Python None). Injected sets stand in for the four
 * lazily-imported predicates; the INLINE_TOOL_EXECUTORS key set is mirrored here. */
const char *w14_inlinetool_874779_pre_registry_route(const char *function_name,
        const struct json_t *legacy_aliases, const struct json_t *bridge_names,
        const struct json_t *agent_loop_tools, const struct json_t *connector_names,
        const struct json_t *context_engine_names, const struct json_t *memory_provider_names,
        int has_agent);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_INLINETOOL_874779_H */
