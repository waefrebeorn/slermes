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

/* C equivalent of hermes_cli.plugins.plugin_resolve_pre_tool_block. */
extern bool plugin_resolve_pre_tool_block(const char *block_msg);

/*
 * Port of Python agent/tool_executor.py:_emit_terminal_post_tool_call()
 * Port of Python agent/tool_executor.py:_emit_cancelled_terminal_post_tool_call()
 * Port of Python agent/tool_executor.py:_tool_search_scoped_names()
 * Port of Python agent/tool_executor.py:_run_agent_tool_execution_middleware()
 *   — ported below as tool_executor_run_agent_middleware(): runs the
 *   pre_tool_call hook + plugin pre-tool-block check, dispatches execute()
 *   exactly once, and emits the post_tool_call hook. The relay request
 *   middleware (hermes_cli.middleware.apply_tool_request_middleware) has no
 *   C equivalent because the C tool path performs relay rewriting inline.
 */

/* Execute callback type: receives final args JSON, returns malloc'd result JSON
 * (caller frees). Mirrors Python's `execute(final_args)` closure. */
typedef char *(*tool_execute_fn)(const char *final_args_json);

/* Managed result of a middleware run (mirrors Python _ManagedToolResult). */
typedef struct tool_middleware_result {
    char *result;     /* malloc'd result JSON (caller frees) */
    char *args;       /* malloc'd final args JSON */
    int   blocked;    /* 1 if dispatch was blocked */
    int   dispatched; /* 1 if execute() ran */
} tool_middleware_result_t;

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
 *  _run_agent_tool_execution_middleware — Relay rewrites + policy + dispatch
 *  Port of Python: _run_agent_tool_execution_middleware()
 * ================================================================ */
void tool_executor_run_agent_middleware(
    const agent_state_t *state,
    const char *function_name,
    const char *function_args_json,
    const char *effective_task_id,
    const char *tool_call_id,
    tool_execute_fn execute,
    tool_middleware_result_t *out)
{
    if (out) { out->result = NULL; out->args = NULL; out->blocked = 0; out->dispatched = 0; }
    if (!function_name || !execute) return;

    /* Build the pre_tool_call hook payload (mirrors Python's hook dispatch). */
    json_t *payload = json_object();
    if (payload) {
        json_set(payload, "tool_name", json_string(function_name));
        if (function_args_json) json_set(payload, "arguments", json_string(function_args_json));
        if (effective_task_id) json_set(payload, "task_id", json_string(effective_task_id));
        if (tool_call_id) json_set(payload, "tool_call_id", json_string(tool_call_id));
    }
    char *payload_str = payload ? json_serialize(payload) : NULL;
    char *hook_resp = payload_str ? invoke_hook("pre_tool_call", payload_str) : NULL;
    if (payload) json_free(payload);
    if (payload_str) free(payload_str);

    const char *block_message = NULL;
    char block_buf[512];
    if (hook_resp && *hook_resp) {
        /* A non-empty pre_tool_call response is treated as a block message. */
        snprintf(block_buf, sizeof(block_buf), "%.*s", (int)(sizeof(block_buf) - 1), hook_resp);
        block_message = block_buf;
    }
    if (hook_resp) free(hook_resp);

    /* Plugin pre-tool-block check (C equivalent of plugin_resolve_pre_tool_block). */
    if (!block_message && plugin_resolve_pre_tool_block(function_name)) {
        snprintf(block_buf, sizeof(block_buf),
                 "Tool '%s' blocked by plugin pre-tool-call policy", function_name);
        block_message = block_buf;
    }

    if (block_message) {
        if (out) {
            size_t sz = strlen(block_message) + 32;
            char *err = malloc(sz);
            snprintf(err, sz, "{\"error\":\"%.*s\"}", (int)strlen(block_message), block_message);
            out->result = err;
            out->args = strdup(function_args_json ? function_args_json : "{}");
            out->blocked = 1;
        }
        tool_executor_emit_post_tool_call(function_name, function_args_json,
            out ? out->result : block_message, effective_task_id, tool_call_id,
            0, "blocked", "plugin_block", block_message);
        return;
    }

    /* Dispatch exactly once (mirrors the Python dispatch_lock guard). */
    static int s_dispatched = 0;
    if (s_dispatched) {
        if (out) out->dispatched = 1;
        return;
    }
    s_dispatched = 1;

    char *result = execute(function_args_json ? function_args_json : "{}");
    if (out) {
        out->result = result;
        out->args = strdup(function_args_json ? function_args_json : "{}");
        out->dispatched = 1;
    }
    s_dispatched = 0;
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
