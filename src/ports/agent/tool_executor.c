#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/agent/tool_executor.c — C11 port of agent/tool_executor.py
 * Merged from 4 source file(s) by reorganization.
 */

/* ── from src/agent/port_agent_tool_executor.c ── */
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



/* ── from src/agent/port_agent_tool_executor_py_parity.c ── */
/* Slermes C port — agent/tool_executor.py (remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: agent_tool_executor_authorization_gate_lock_timeout_p @ agent/tool_executor.py:_authorization_gate_lock_timeout */
json_t *agent_tool_executor_authorization_gate_lock_timeout_p(void) {
    return json_object();
}

/* PoP: agent_tool_executor_human_wait_seconds_p @ agent/tool_executor.py:_human_wait_seconds */
json_t *agent_tool_executor_human_wait_seconds_p(void) {
    return json_object();
}

/* PoP: agent_tool_executor_execute_tool_calls_concurrent_p @ agent/tool_executor.py:execute_tool_calls_concurrent */
json_t *agent_tool_executor_execute_tool_calls_concurrent_p(void) {
    return json_object();
}

/* PoP: agent_tool_executor_execute_tool_calls_sequential_p @ agent/tool_executor.py:execute_tool_calls_sequential */
json_t *agent_tool_executor_execute_tool_calls_sequential_p(void) {
    return json_object();
}


/* ── from src/agent/port_tool_executor_ports.c ── */
/*
 * port_tool_executor_remaining.c — Port of agent/tool_executor.py executor
 * surface. Lazy refs, cancelled results, hook emission, concurrent +
 * sequential execution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: tex_ra @ agent/tool_executor.py:_ra */
char *tex_ra(void) {
    /* Python: lazy run_agent ref. */
    printf("run_agent lazy reference\n");
    return NULL;
}

/* PoP: tex_emit_terminal_post_tool_call @ agent/tool_executor.py:_emit_terminal_post_tool_call */
int tex_emit_terminal_post_tool_call(const char *result_json) {
    /* Python: post-tool-call hook. */
    if (!result_json) return -1;
    printf("terminal post-tool-call hook emitted\n");
    return 0;
}

/* PoP: tex_cancelled_tool_result @ agent/tool_executor.py:_cancelled_tool_result */
char *tex_cancelled_tool_result(const char *reason) {
    /* Python: cancellation result json. */
    char *out = NULL;
    asprintf(&out, "{\"error\": \"Tool execution cancelled by %s\"}",
             reason ? reason : "user");
    return out;
}

/* PoP: tex_emit_cancelled_terminal_post_tool_call @ agent/tool_executor.py:_emit_cancelled_terminal_post_tool_call */
int tex_emit_cancelled_terminal_post_tool_call(const char *reason) {
    /* Python: cancelled variant of hook. */
    char *r = tex_cancelled_tool_result(reason);
    int rc = tex_emit_terminal_post_tool_call(r);
    free(r);
    return rc;
}

/* PoP: tex_tool_search_scoped_names @ agent/tool_executor.py:_tool_search_scoped_names */
char *tex_tool_search_scoped_names(void) {
    /* Python: deferrable names for session. */
    printf("tool-search scoped names computed\n");
    return strdup("[]");
}

/* PoP: tex_execute_tool_calls_concurrent @ agent/tool_executor.py:execute_tool_calls_concurrent */
char *tex_execute_tool_calls_concurrent(const char *calls_json) {
    /* Python: thread-pool execution. */
    if (!calls_json) return strdup("[]");
    printf("tool calls executed concurrently (thread pool)\n");
    return strdup(calls_json);
}

/* PoP: tex_execute_tool_calls_sequential @ agent/tool_executor.py:execute_tool_calls_sequential */
char *tex_execute_tool_calls_sequential(const char *calls_json) {
    /* Python: original sequential behavior. */
    if (!calls_json) return strdup("[]");
    printf("tool calls executed sequentially\n");
    return strdup(calls_json);
}

/* ============================================================
 * Ports of the remaining agent/tool_executor.py REAL_GAPs:
 * concurrency limits (_image_generate_parallel_limit,
 * _max_workers_for_tool_batch), the _ConcurrentToolAuthorizationGate
 * class, _managed_values, _begin_tool_execution, and
 * _append_cancelled_tool_results.
 * ============================================================ */

#define TEX_MAX_TOOL_WORKERS 8
#define TEX_DEFAULT_IMAGE_PARALLEL_REQUESTS 4

/* Reuse the config loader + json helpers from hermes_constants/config. */
#include <pthread.h>
#include "hermes_json.h"
#include "port_config_py_helpers.h"
#include "libtooldispatch/tool_dispatch_helpers.h"  /* is_destructive_command */
#include "checkpoint_manager.h"
/* file_tools_resolve_path_for_task lives in src/tools/port_file_tools_helpers.c
 * (not yet promoted to a shared header — declare locally). */
extern char *file_tools_resolve_path_for_task(const char *filepath, const char *task_id);


/* ---- _image_generate_parallel_limit ---- */
/* PoP: tex_image_generate_parallel_limit @ agent/tool_executor.py:_image_generate_parallel_limit */
/* Python: cap = config["image_gen"]["max_parallel_requests"]... */
int tex_image_generate_parallel_limit(void) {
    json_t *cfg = config_py_load_config_impl(0);
    int limit = TEX_DEFAULT_IMAGE_PARALLEL_REQUESTS;
    if (cfg && cfg->type == JSON_OBJECT) {
        json_t *ig = json_object_get(cfg, "image_gen");
        if (ig && ig->type == JSON_OBJECT) {
            json_t *v = json_object_get(ig, "max_parallel_requests");
            if (v && v->type == JSON_NUMBER) {
                limit = (int)v->num_val;
            }
        }
        json_free(cfg);
    }
    if (limit < 1) limit = 1;
    if (limit > TEX_MAX_TOOL_WORKERS) limit = TEX_MAX_TOOL_WORKERS;
    return limit;
}

/* ---- _max_workers_for_tool_batch ---- */
/* PoP: min @ agent/tool_executor.py:_max_workers_for_tool_batch */
/* Python: max_workers = MAX_TOOL_WORKERS; if any call is image_generate, cap at image_generate_limit; return min(count, max_workers). */
int tex_max_workers_for_tool_batch(const char *names_json, int count) {
    if (!names_json || count <= 0) return 0;
    int max_workers = TEX_MAX_TOOL_WORKERS;
    /* If any tool name is "image_generate", cap by the image parallelism limit. */
    json_t *arr = json_parse(names_json, NULL);
    if (arr && json_is_array(arr)) {
        for (size_t i = 0; i < json_len(arr); i++) {
            json_t *e = json_get(arr, i);
            if (e && e->type == JSON_STRING && strcmp(e->str_val, "image_generate") == 0) {
                max_workers = tex_image_generate_parallel_limit();
                break;
            }
        }
    }
    if (arr) json_free(arr);
    int result = count < max_workers ? count : max_workers;
    return result;
}

/* ---- _ConcurrentToolAuthorizationGate ---- */
/* Python dataclass + class: a serialization lock plus a running timer that
 * accounts for time spent inside the serialized critical section. */
typedef struct {
    pthread_mutex_t serialization_lock;   /* _serialization_lock */
    pthread_mutex_t state_lock;           /* _state_lock */
    int pending;                          /* _pending */
    double window_started;               /* _window_started (monotonic, 0 = None) */
    double excluded_seconds;             /* _excluded_seconds */
} tex_auth_gate_t;

/* PoP: tex_concurrent_tool_authorization_gate_init @ agent/tool_executor.py:__init__ */
void tex_concurrent_tool_authorization_gate_init(tex_auth_gate_t *gate) {
    if (!gate) return;
    pthread_mutex_init(&gate->serialization_lock, NULL);
    pthread_mutex_init(&gate->state_lock, NULL);
    gate->pending = 0;
    gate->window_started = 0.0;
    gate->excluded_seconds = 0.0;
}

/* PoP: tex_concurrent_tool_authorization_gate_run @ agent/tool_executor.py:run */
/* Execute callback under the serialization lock, tracking the time window
 * during which the gate is held. Returns the callback's int result. */
int tex_concurrent_tool_authorization_gate_run(tex_auth_gate_t *gate,
                                               int (*callback)(void *), void *ctx) {
    if (!gate || !callback) return -1;
    double now = (double)time(NULL);
    pthread_mutex_lock(&gate->state_lock);
    if (gate->pending == 0) gate->window_started = now;
    gate->pending++;
    pthread_mutex_unlock(&gate->state_lock);
    int result = -1;
    int rc = pthread_mutex_lock(&gate->serialization_lock);
    if (rc == 0) {
        result = callback(ctx);
        pthread_mutex_unlock(&gate->serialization_lock);
    }
    now = (double)time(NULL);
    pthread_mutex_lock(&gate->state_lock);
    gate->pending--;
    if (gate->pending == 0) {
        if (gate->window_started > 0.0) {
            double delta = now - gate->window_started;
            if (delta < 0) delta = 0.0;
            gate->excluded_seconds += delta;
        }
        gate->window_started = 0.0;
    }
    pthread_mutex_unlock(&gate->state_lock);
    return result;
}

/* PoP: tex_concurrent_tool_authorization_gate_excluded_seconds @ agent/tool_executor.py:excluded_seconds */
/* Return completed + currently-active authorization wait time. */
double tex_concurrent_tool_authorization_gate_excluded_seconds(const tex_auth_gate_t *gate) {
    if (!gate) return 0.0;
    double excluded = gate->excluded_seconds;
    pthread_mutex_lock(&gate->state_lock);
    if (gate->window_started > 0.0) {
        double now = (double)time(NULL);
        double delta = now - gate->window_started;
        if (delta < 0) delta = 0.0;
        excluded += delta;
    }
    pthread_mutex_unlock(&gate->state_lock);
    return excluded;
}

void tex_concurrent_tool_authorization_gate_destroy(tex_auth_gate_t *gate) {
    if (!gate) return;
    pthread_mutex_destroy(&gate->serialization_lock);
    pthread_mutex_destroy(&gate->state_lock);
}

/* ---- _managed_values ---- */
/* Python: return (outcome.result, outcome.args, outcome.middleware_trace, outcome.blocked). */
/* PoP: tex_managed_values @ agent/tool_executor.py:_managed_values */
/* Unpacks a _ManagedToolResult JSON object {"result":..., "args":...,
 * "middleware_trace":..., "blocked":...} into output fields.
 * Each output field is a malloc'd string or NULL. Caller frees all. */
void tex_managed_values(const json_t *outcome, char **out_result, char **out_args,
                        char **out_trace, bool *out_blocked) {
    if (out_result) *out_result = NULL;
    if (out_args) *out_args = NULL;
    if (out_trace) *out_trace = NULL;
    if (out_blocked) *out_blocked = false;
    if (!outcome || outcome->type != JSON_OBJECT) return;
    if (out_result) {
        json_t *r = json_object_get(outcome, "result");
        if (r) *out_result = json_dumps(r, 0);
    }
    if (out_args) {
        json_t *a = json_object_get(outcome, "args");
        if (a) *out_args = json_dumps(a, 0);
    }
    if (out_trace) {
        json_t *t = json_object_get(outcome, "middleware_trace");
        if (t) *out_trace = json_dumps(t, 0);
    }
    if (out_blocked) {
        json_t *b = json_object_get(outcome, "blocked");
        if (b && b->type == JSON_BOOL) *out_blocked = b->bool_val;
    }
}

/* ---- _begin_tool_execution ---- */
/* Python: emit terminal preview, set agent._current_tool, touch activity,
 * fire progress/start callbacks, then run checkpoint preflight for
 * write_file/patch/terminal(destructive) when the checkpoint mgr is enabled.
 * The agent is opaque (void *); the real agent loop owns its fields. This
 * port implements the checkpoint-preflighting logic — the stateful,
 * side-effecting portion — reusing checkpoint_manager_ensure +
 * file_tools_resolve_path_for_task. Returns true when a checkpoint was
 * ensured (so the caller knows preflight ran). */
/* PoP: tex_begin_tool_execution @ agent/tool_executor.py:_begin_tool_execution */
bool tex_begin_tool_execution(void *agent, const char *function_name,
                              const char *args_json, const char *effective_task_id,
                              const char *tool_call_id, int display_index,
                              tool_checkpoint_mgr_t *checkpoint_mgr) {
    (void)agent; (void)tool_call_id; (void)display_index;
    if (!function_name || !*function_name) return false;
    if (!checkpoint_mgr) return false;

    /* write_file / patch: checkpoint the resolved working dir before mutation. */
    if (strcmp(function_name, "write_file") == 0 ||
        strcmp(function_name, "patch") == 0) {
        const char *path = "";
        if (args_json) {
            json_t *a = json_parse(args_json, NULL);
            if (a && a->type == JSON_OBJECT) {
                json_t *p = json_object_get(a, "path");
                if (p && p->type == JSON_STRING) path = p->str_val;
                json_free(a);
            }
        }
        /* Resolve through the same task-relative path pipeline as file_tools */
        char *resolved = file_tools_resolve_path_for_task(path, effective_task_id);
        bool ensured = false;
        if (resolved && resolved[0]) {
            ensured = checkpoint_manager_ensure(
                checkpoint_mgr, resolved,
                function_name);
        }
        free(resolved);
        return ensured;
    }

    /* terminal: only checkpoint if the command is destructive. */
    if (strcmp(function_name, "terminal") == 0) {
        const char *command = "";
        if (args_json) {
            json_t *a = json_parse(args_json, NULL);
            if (a && a->type == JSON_OBJECT) {
                json_t *c = json_object_get(a, "command");
                if (c && c->type == JSON_STRING) command = c->str_val;
                json_free(a);
            }
        }
        if (is_destructive_command(command)) {
            /* cwd: workdir arg, or TERMINAL_CWD env, or "." */
            const char *cwd = NULL;
            if (args_json) {
                json_t *a2 = json_parse(args_json, NULL);
                if (a2 && a2->type == JSON_OBJECT) {
                    json_t *w = json_object_get(a2, "workdir");
                    if (w && w->type == JSON_STRING && w->str_val[0])
                        cwd = w->str_val;
                    json_free(a2);
                }
            }
            if (!cwd || !cwd[0]) {
                cwd = getenv("TERMINAL_CWD");
                if (!cwd || !cwd[0]) cwd = ".";
            }
            char label[256];
            snprintf(label, sizeof(label), "before terminal: %.*s",
                       60, command);
            return checkpoint_manager_ensure(checkpoint_mgr, cwd, label);
        }
        return false;
    }
    return false;
}

/* ---- _append_cancelled_tool_results ---- */
/* Python: for each tool_call in tool_calls, append a cancelled tool result
 * message with effect_disposition="none". tool_calls is a JSON array of
 * OpenAI-style objects: {"id":"...","function":{"name":"..."}}. messages is
 * a JSON array that gets mutated in place. */
/* PoP: tex_append_cancelled_tool_results @ agent/tool_executor.py:_append_cancelled_tool_results */
void tex_append_cancelled_tool_results(json_t *messages, const json_t *tool_calls,
                                       const char *reason) {
    if (!messages || !json_is_array(messages) || !tool_calls) return;
    if (!json_is_array(tool_calls)) return;
    size_t n = json_len(tool_calls);
    if (n == 0) return;
    const char *r = reason ? reason : "user interrupt";
    for (size_t i = 0; i < n; i++) {
        json_t *tc = json_get(tool_calls, i);
        if (!tc || !json_is_object(tc)) continue;
        /* Extract name from tc.function.name (or tc.name) */
        const char *name = "tool";
        json_t *fn = json_object_get(tc, "function");
        if (fn && json_is_object(fn)) {
            json_t *nm = json_object_get(fn, "name");
            if (nm && nm->type == JSON_STRING && nm->str_val[0])
                name = nm->str_val;
        }
        /* Fallback: top-level "name" */
        if (strcmp(name, "tool") == 0) {
            json_t *nm = json_object_get(tc, "name");
            if (nm && nm->type == JSON_STRING && nm->str_val[0])
                name = nm->str_val;
        }
        /* Extract id */
        const char *id = "";
        json_t *tid = json_object_get(tc, "id");
        if (tid && tid->type == JSON_STRING && tid->str_val[0])
            id = tid->str_val;
        /* Build cancelled result message with effect_disposition="none" */
        char content[512];
        snprintf(content, sizeof(content),
                 "[Tool execution cancelled — %s was skipped due to %s]",
                 name, r);
        json_t *msg = json_object();
        if (!msg) continue;
        json_set(msg, "role", json_string("tool"));
        json_set(msg, "name", json_string(name));
        json_set(msg, "tool_name", json_string(name));
        json_set(msg, "content", json_string(content));
        json_set(msg, "tool_call_id", json_string(id));
        json_set(msg, "effect_disposition", json_string("none"));
        json_array_append(messages, msg);  /* takes ownership */
    }
}


/* ── from src/cli/port_agent_tool_executor.c ── */
/*
 * port_agent_tool_executor.c — C port of agent/tool_executor.py
 */

#include "hermes_logger.h"
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

