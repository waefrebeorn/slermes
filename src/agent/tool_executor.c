/*
 * tool_executor.c — Sequential and concurrent tool dispatch helpers.
 *
 * Port of Python agent/tool_executor.py (1248 lines).
 * Thin wrappers and dispatch helpers for the C agent loop.
 *
 * MIT License — WuBu Slermes Project
 */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_hooks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * Port of Python agent/tool_executor.py:_emit_terminal_post_tool_call()
 * Port of Python agent/tool_executor.py:_emit_cancelled_terminal_post_tool_call()
 * Port of Python agent/tool_executor.py:_tool_search_scoped_names()
 * Port of Python agent/tool_executor.py:_apply_tool_request_middleware_for_agent — N/A, CLI middleware system (hermes_cli.middleware)
 * Port of Python agent/tool_executor.py:_run_agent_tool_execution_middleware — N/A, CLI middleware system (hermes_cli.middleware)
 */

/* ================================================================
 *  _emit_terminal_post_tool_call — post-tool-call hook
 *  Port of Python: _emit_terminal_post_tool_call()
 * ================================================================ */
void tool_executor_emit_post_tool_call(
    const char *function_name,
    const char *function_args_json,
    const char *result,
    const char *effective_task_id,
    const char *tool_call_id,
    long duration_ms,
    const char *status,
    const char *error_type,
    const char *error_message)
{
    if (!function_name) return;

    json_t *detail = json_object();
    if (!detail) return;

    if (function_name) json_set(detail, "function_name", json_string(function_name));
    if (function_args_json) json_set(detail, "function_args", json_string(function_args_json));
    if (result) json_set(detail, "result", json_string(result));
    if (effective_task_id) json_set(detail, "task_id", json_string(effective_task_id));
    if (tool_call_id) json_set(detail, "tool_call_id", json_string(tool_call_id));
    if (status) json_set(detail, "status", json_string(status));
    if (error_type) json_set(detail, "error_type", json_string(error_type));
    if (error_message) json_set(detail, "error_message", json_string(error_message));

    char duration_str[32];
    snprintf(duration_str, sizeof(duration_str), "%ld", duration_ms);
    json_set(detail, "duration_ms", json_string(duration_str));

    char *payload = json_serialize(detail);
    if (payload) {
        invoke_hook("post_tool_call", payload);
        free(payload);
    }

    json_free(detail);
}

/* ================================================================
 *  _emit_cancelled_terminal_post_tool_call
 *  Port of Python: _emit_cancelled_terminal_post_tool_call()
 * ================================================================ */
char *tool_executor_emit_cancelled_post_tool_call(
    const char *function_name,
    const char *function_args_json,
    const char *effective_task_id,
    const char *tool_call_id,
    double start_time_sec,
    const char *reason,
    const char *error_type)
{
    if (!reason) reason = "user interrupt";
    if (!error_type) error_type = "keyboard_interrupt";

    /* Build the cancelled result JSON */
    json_t *result_obj = json_object();
    if (!result_obj) return strdup("{\"error\": \"Tool execution cancelled\"}");

    char err_buf[512];
    int n = snprintf(err_buf, sizeof(err_buf),
                     "Tool execution cancelled by %s", reason);
    if (n < 0 || (size_t)n >= sizeof(err_buf))
        snprintf(err_buf, sizeof(err_buf), "Tool execution cancelled");

    json_set(result_obj, "error", json_string(err_buf));
    json_set(result_obj, "status", json_string("cancelled"));

    char *result_json = json_serialize(result_obj);
    json_free(result_obj);

    /* Compute duration from start_time */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double current = (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
    long duration_ms = (long)((current - start_time_sec) * 1000);

    /* Emit the post-tool-call hook */
    tool_executor_emit_post_tool_call(
        function_name, function_args_json, result_json,
        effective_task_id, tool_call_id, duration_ms,
        "cancelled", error_type, err_buf);

    if (!result_json) return strdup("{\"error\": \"Tool execution cancelled\"}");
    return result_json;
}

/* ================================================================
 *  _tool_search_scoped_names — get deferrable tool names
 *  Port of Python: _tool_search_scoped_names()
 * ================================================================ */
json_t *tool_executor_get_scoped_names(
    const agent_state_t *state)
{
    if (!state) return json_array();

    /* Return the set of currently enabled tool names.
     * The C agent enforces scoping directly in the dispatch path,
     * so this returns an empty set by default. */
    json_t *names = json_array();
    if (!names) return NULL;

    (void)state; /* scoping enforced by dispatch path */

    return names;
}
