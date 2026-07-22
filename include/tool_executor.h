/**
 * @file tool_executor.h
 * @brief Tool execution helpers — post-tool-call hooks, scoped names.
 *
 * Port of Python agent/tool_executor.py.
 *
 * MIT License — WuBu Slermes Project
 */
#ifndef TOOL_EXECUTOR_H
#define TOOL_EXECUTOR_H

#include "hermes_core_types.h"
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Emit a post-tool-call event to the hook system.
 * Port of Python: _emit_terminal_post_tool_call()
 */
void tool_executor_emit_post_tool_call(
    const char *function_name,
    const char *function_args_json,
    const char *result,
    const char *effective_task_id,
    const char *tool_call_id,
    long duration_ms,
    const char *status,
    const char *error_type,
    const char *error_message);

/**
 * Emit a cancelled post-tool-call event and return cancelled result JSON.
 * Port of Python: _emit_cancelled_terminal_post_tool_call()
 * Returns malloc'd string. Caller must free.
 */
char *tool_executor_emit_cancelled_post_tool_call(
    const char *function_name,
    const char *function_args_json,
    const char *effective_task_id,
    const char *tool_call_id,
    double start_time_sec,
    const char *reason,
    const char *error_type);

/**
 * Get scoped deferrable tool names for the session.
 * Port of Python: _tool_search_scoped_names()
 * Returns a JSON array of name strings. Caller must free with json_free().
 */
json_t *tool_executor_get_scoped_names(const agent_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* TOOL_EXECUTOR_H */
