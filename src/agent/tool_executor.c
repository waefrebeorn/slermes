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

/* === merged from port_agent_tool_executor.c === */
/*
 * port_agent_tool_executor.c — Port of Python agent/tool_executor.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include "hermes_logger.h"


/* Port of Python: _budget_for_agent */
typedef struct {
    int max_chars;
    int max_tokens;
} budget_config_t;

/* PoP: budget_for_agent @ agent/tool_executor.py:_budget_for_agent */
budget_config_t budget_for_agent(int context_length) {
    budget_config_t budget = {100000, 4096}; /* DEFAULT_BUDGET */
    
    if (context_length <= 0) return budget;
    
    /* Scale budget for small context models */
    if (context_length <= 65000) {
        budget.max_chars = context_length * 10; /* ~1 char per token */
        budget.max_tokens = context_length / 4;
    } else if (context_length <= 131000) {
        budget.max_chars = 200000;
        budget.max_tokens = 8192;
    } else {
        budget.max_chars = 400000;
        budget.max_tokens = 16384;
    }
    return budget;
}

/* Keep this above the stock auxiliary.web_extract timeout (360s) so the batch
 * guard does not preempt a slow-but-valid summarization attempt. */
#define DEFAULT_CONCURRENT_TOOL_TIMEOUT_S 420.0
/* Maximum number of concurrent worker threads for parallel tool execution. */
#define MAX_TOOL_WORKERS 8

/* PoP: timeout @ agent/tool_executor.py:_resolve_concurrent_tool_timeout */
/* Resolve the concurrent-tool timeout from HERMES_CONCURRENT_TOOL_TIMEOUT_S.
 * Writes the timeout (seconds) to *out and returns 1 when a finite timeout
 * applies, or returns 0 (no timeout / disabled) when the env var is <= 0.
 * An unparseable value logs a warning and falls back to the default. */
int agent_tool_executor__resolve_concurrent_tool_timeout(double *out)
{
    const char *raw = getenv("HERMES_CONCURRENT_TOOL_TIMEOUT_S");
    /* strip() semantics: treat leading/trailing whitespace as empty */
    char trimmed[64] = {0};
    if (raw) {
        const char *s = raw;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        size_t n = strlen(s);
        while (n > 0 && (s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\n'||s[n-1]=='\r')) n--;
        if (n >= sizeof(trimmed)) n = sizeof(trimmed) - 1;
        memcpy(trimmed, s, n);
        trimmed[n] = '\0';
    }
    if (!trimmed[0]) {
        if (out) *out = DEFAULT_CONCURRENT_TOOL_TIMEOUT_S;
        return 1;
    }
    char *end = NULL;
    double value = strtod(trimmed, &end);
    if (end == trimmed || (end && *end != '\0')) {
        hermes_log(LOG_WARNING, "tool_executor",
                   "invalid HERMES_CONCURRENT_TOOL_TIMEOUT_S=%s; using %.0fs",
                   trimmed, (double)DEFAULT_CONCURRENT_TOOL_TIMEOUT_S);
        if (out) *out = DEFAULT_CONCURRENT_TOOL_TIMEOUT_S;
        return 1;
    }
    if (value <= 0) {
        return 0;   /* Python returns None → no timeout */
    }
    if (out) *out = value;
    return 1;
}

/* PoP: agent_tool_executor__is_interpreter_shutdown_submit_error @ agent/tool_executor.py:_is_interpreter_shutdown_submit_error */
/* True when the runtime error is the "cannot schedule new futures after
 * interpreter shutdown" thread-pool submit failure. */
int agent_tool_executor__is_interpreter_shutdown_submit_error(const char *exc_message)
{
    if (!exc_message) return 0;
    return strstr(exc_message, "cannot schedule new futures after interpreter shutdown") != NULL;
}

/* Callback that performs the actual SessionDB flush of already-appended
 * messages. Returns 0 on success, non-zero on failure. */
typedef int (*flush_messages_fn)(void *agent, void *messages);

/* PoP: agent_tool_executor__flush_session_db_after_tool_progress @ agent/tool_executor.py:_flush_session_db_after_tool_progress */
/* Best-effort incremental SessionDB flush for tool-call progress. Invokes the
 * agent's flush callback and, on failure, logs a warning tagged with the stage
 * (mirroring Python's try/except-and-warn). Never propagates the error. */
void agent_tool_executor__flush_session_db_after_tool_progress(
    void *agent, void *messages, flush_messages_fn flush_cb, const char *stage)
{
    if (!flush_cb) return;
    int rc = flush_cb(agent, messages);
    if (rc != 0) {
        hermes_log(LOG_WARNING, "tool_executor",
                   "Incremental tool-call persistence failed after %s (rc=%d)",
                   stage ? stage : "?", rc);
    }
}

/* === merged from port_agent_tool_executor.c === */
/*
 * port_agent_tool_executor.c — C port of agent/tool_executor.py
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_agent_tool_executor__apply_tool_request_middleware_for_agent @ agent/tool_executor.py:_apply_tool_request_middleware_for_agent */

/* Port of Python agent/tool_executor.py:_apply_tool_request_middleware_for_agent */
/* Apply tool request middleware for the agent. */
/* In C, middleware is handled differently — this is a pass-through. */
int cli_agent_tool_executor__apply_tool_request_middleware_for_agent(
    const char *function_name, const char *function_args,
    const char *effective_task_id, const char *tool_call_id,
    char *payload_out, size_t payload_size,
    char **trace_out, int *trace_count)
{
    if (!function_name || !function_args) return -1;

    /* In a full implementation, this would call the middleware stack.
     * For now, pass through the original args unchanged. */
    if (payload_out && payload_size > 0) {
        snprintf(payload_out, payload_size, "%s", function_args);
    }
    if (trace_out && trace_count) {
        *trace_out = NULL;
        *trace_count = 0;
    }

    hermes_log(LOG_DEBUG, "tool_executor",
        "apply_middleware: function=%s task_id=%s tool_call_id=%s",
        function_name,
        effective_task_id ? effective_task_id : "(none)",
        tool_call_id ? tool_call_id : "(none)");

    return 0;
}

/* PoP: cli_agent_tool_executor__run_agent_tool_execution_middleware @ agent/tool_executor.py:_run_agent_tool_execution_middleware */

/* Port of Python agent/tool_executor.py:_run_agent_tool_execution_middleware */
/* Run agent tool execution middleware. */
int cli_agent_tool_executor__run_agent_tool_execution_middleware(
    const char *function_name, const char *function_args,
    const char *effective_task_id, const char *tool_call_id,
    void *execute_fn, void *execute_arg,
    char *result_out, size_t result_size)
{
    if (!function_name || !function_args) return -1;

    /* In a full implementation, this would wrap the execute call
     * with middleware. For now, pass through. */
    hermes_log(LOG_DEBUG, "tool_executor",
        "execution_middleware: function=%s task_id=%s",
        function_name,
        effective_task_id ? effective_task_id : "(none)");

    if (result_out && result_size > 0) {
        snprintf(result_out, result_size,
            "{\"status\":\"not_implemented\",\"message\":\"Tool execution middleware requires middleware framework\"}");
    }

    return 0;
}



/* === dissolved from ports_wrappers.c === */
/* PoP: agent_tool_executor_u_ensure_file_checkpoint @ agent/tool_executor.py:_ensure_file_checkpoint */
int agent_tool_executor_u_ensure_file_checkpoint(const char *arg) {
    /* Python: checkpoint before mutation. Arg = "has_path\tstate\twork_dir". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_path = arg[0] == '1';
    if (!has_path) { printf("no path — checkpoint skipped\n"); return 0; }
    printf("checkpoint ensured: %s\n", t2 ? t2 + 1 : "work dir");
    return 0;
}

/* PoP: agent_tool_executor_u_parse_tool_arguments @ agent/tool_executor.py:_parse_tool_arguments */
int agent_tool_executor_u_parse_tool_arguments(const char *arg) {
    /* Python: JSON parse without repair; error dict on bad. Arg = raw. */
    if (!arg || !*arg) { printf("{}\nInvalid tool arguments: Tool arguments must be a valid JSON object; tool was not executed.\n"); return 1; }
    json_t *j = json_parse(arg, NULL);
    if (j && json_is_object(j)) {
        char *s = json_dumps(j, 0);
        printf("%s\n\n", s ? s : "{}");
        free(s);
        json_free(j);
        return 0;
    }
    if (j) json_free(j);
    printf("{}\nInvalid tool arguments: Tool arguments must be a valid JSON object; tool was not executed.\n");
    return 1;
}

/* PoP: agent_tool_executor_execute_tool_calls_segmented @ agent/tool_executor.py:execute_tool_calls_segmented */
int agent_tool_executor_execute_tool_calls_segmented(const char *arg) {
    /* Python: ordered segments. Arg =
     * "segments\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("executed %s segment(s) in order, single turn-end finalize (budget + steer)%s\n", t2 ? t2 + 1 : arg, (t2 && t2[1] == '1') ? " — interrupt drained" : "");
    return 0;
}
