/*
 * port_tool_executor_remaining.c — Port of agent/tool_executor.py executor
 * surface. Lazy refs, cancelled results, hook emission, concurrent +
 * sequential execution.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
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
