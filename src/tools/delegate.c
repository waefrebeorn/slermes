/*
 * delegate.c — Delegate task tool for Hermes C.
 * Port of Python tools/delegate_tool.py.
 * Spawns child agents with isolated context and restricted toolsets.
 * JSON interface: { "action": "start", "goal": "...", "context": "...", "subtasks": [...], "max_concurrent_children": N, "child_max_turns": M, "orchestrator": true }
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include "hermes_tool_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <stdbool.h>

/* Registry iteration primitives (defined in src/tools/registry.c) */
extern size_t registry_count(void);
extern const char *registry_get_name(size_t i);
extern const char *registry_get_toolset(const char *name);

/* Delegation config defaults (mirror Python module constants) */
#define DEL_DEFAULT_MAX_CONCURRENT_CHILDREN 3
#define DEL_DEFAULT_MAX_ASYNC_CHILDREN      3
#define DEL_DEFAULT_MAX_SUMMARY_CHARS       24000
#define DEL_MIN_SPAWN_DEPTH                 1
#define DEL_DEFAULT_MAX_SPAWN_DEPTH         1
#define DEL_MIN_SUMMARY_CHARS               2000
#define DEL_SUMMARY_HEADROOM_FRACTION       0.5   /* 50% of parent headroom */

/* Tools children must never have access to. */
static const char *g_delegate_blocked_tool_names[] = {
    "delegate_task",   /* no recursive delegation */
    "clarify",         /* no user interaction */
    "memory",          /* no writes to shared MEMORY.md */
    "send_message",    /* no cross-platform side effects */
    "execute_code",    /* children reason step-by-step, not script */
    "cronjob",         /* no scheduling in the parent's name */
    NULL
};

/* Composite toolsets that must never pass through to children. */
static const char *g_delegate_composite_blocked_toolsets[] = {
    "delegation",
    "code_execution",
    NULL
};

#define MAX_CHILDREN 64
#define MAX_GOAL_LEN 8192
#define MAX_CONTEXT_LEN 8192

typedef struct {
    int     session_id;
    char    goal[MAX_GOAL_LEN];
    char    context[MAX_CONTEXT_LEN];
    bool    running;
    time_t  start_time;
    int     child_max_turns;
    int     max_concurrent;
    bool    orchestrator;
} delegate_child_t;

static delegate_child_t g_children[MAX_CHILDREN];
static int g_next_delegate_id = 1;
static int g_max_concurrent_default = 3;
static int g_max_spawn_depth_default = 1;
static int g_child_max_turns_default = 30;

/* Spawn-pause gate (mirrors Python tools/delegate_tool.py:
 *   _spawn_paused + _spawn_pause_lock).
 * Active children keep running; only NEW delegate_task spawns fail fast until
 * unblocked. Read/written under g_spawn_pause_lock. */
static bool g_spawn_paused = false;
static pthread_mutex_t g_spawn_pause_lock = PTHREAD_MUTEX_INITIALIZER;

/* PoP: set_spawn_paused @ tools/delegate_tool.py:set_spawn_paused */
/* Port of Python tools/delegate_tool.py:set_spawn_paused().
 * Globally block/unblock new delegate_task spawns. Returns the NEW state. */
bool set_spawn_paused(bool paused) {
    pthread_mutex_lock(&g_spawn_pause_lock);
    g_spawn_paused = paused;
    bool new_state = g_spawn_paused;
    pthread_mutex_unlock(&g_spawn_pause_lock);
    return new_state;
}

/* PoP: is_spawn_paused @ tools/delegate_tool.py:is_spawn_paused */
/* Port of Python tools/delegate_tool.py:is_spawn_paused().
 * Returns true when new delegate spawns are currently blocked. */
bool is_spawn_paused(void) {
    pthread_mutex_lock(&g_spawn_pause_lock);
    bool state = g_spawn_paused;
    pthread_mutex_unlock(&g_spawn_pause_lock);
    return state;
}

static const char *g_delegation_blocked_tools[] = {
    "delegate_task",
    "clarify",
    "memory",
    "send_message",
    "execute_code",
    NULL
};

static int find_free_child_slot(void) {
    for (int i = 0; i < MAX_CHILDREN; i++)
        if (g_children[i].session_id == 0) return i;
    return -1;
}

static int find_child_by_session(int session_id) {
    for (int i = 0; i < MAX_CHILDREN; i++)
        if (g_children[i].session_id == session_id) return i;
    return -1;
}

static void reap_children(void) {
    for (int i = 0; i < MAX_CHILDREN; i++) {
        if (g_children[i].session_id > 0 && g_children[i].running) {
            /* Check if child process would have completed (timeout) */
            if (time(NULL) - g_children[i].start_time > g_children[i].child_max_turns * 2) {
                g_children[i].running = false;
            }
        }
    }
}

static bool is_tool_blocked(const char *tool_name) {
    if (!tool_name) return false;
    for (int i = 0; g_delegation_blocked_tools[i]; i++) {
        if (strcmp(tool_name, g_delegation_blocked_tools[i]) == 0)
            return true;
    }
    return false;
}

/* PoP: _build_child_agent @ src/tools/delegate.c:delegate_start */
/* Port of Python tools/delegate_tool.py:_build_child_agent(). */
static void delegate_start(const char *goal, const char *context, json_node_t *subtasks,
                           int max_concurrent, int child_max_turns, bool orchestrator,
                           json_node_t *result) {
    reap_children();

    int slot = find_free_child_slot();
    if (slot < 0) {
        json_object_set(result, "error", json_new_string("Max concurrent children reached"));
        return;
    }

    if (!goal || !goal[0]) {
        json_object_set(result, "error", json_new_string("Missing goal"));
        return;
    }

    delegate_child_t *child = &g_children[slot];
    child->session_id = g_next_delegate_id++;
    snprintf(child->goal, sizeof(child->goal), "%s", goal);
    snprintf(child->context, sizeof(child->context), "%s", context ? context : "");
    child->running = true;
    child->start_time = time(NULL);
    child->child_max_turns = child_max_turns > 0 ? child_max_turns : g_child_max_turns_default;
    child->max_concurrent = max_concurrent > 0 ? max_concurrent : g_max_concurrent_default;
    child->orchestrator = orchestrator;

    /* Validate subtasks if provided */
    int subtask_count = 0;
    if (subtasks && subtasks->type == JSON_ARRAY) {
        subtask_count = (int)subtasks->c.count;
        for (int j = 0; j < subtask_count; j++) {
            json_node_t *st = json_get(subtasks, j);
            if (st && st->type == JSON_OBJECT) {
                const char *desc = json_get_str(st, "description", "");
                if (!desc || !desc[0]) {
                    json_object_set(result, "error", json_new_string("Subtask missing description"));
                    child->session_id = 0;
                    return;
                }
            }
        }
    }

    /* Count running children to enforce max_concurrent */
    int running_count = 0;
    for (int i = 0; i < MAX_CHILDREN; i++) {
        if (g_children[i].session_id > 0 && g_children[i].running)
            running_count++;
    }
    if (running_count >= child->max_concurrent) {
        json_object_set(result, "error", json_new_string("Max concurrent children reached"));
        child->session_id = 0;
        return;
    }

    json_object_set(result, "session_id", json_new_number((double)child->session_id));
    json_object_set(result, "status", json_new_string("started"));
    json_object_set(result, "goal", json_new_string(child->goal));
    if (context && context[0])
        json_object_set(result, "context", json_new_string(context));
    if (subtask_count > 0)
        json_object_set(result, "subtasks", json_new_number((double)subtask_count));
    json_object_set(result, "orchestrator", json_new_bool(orchestrator));
    json_object_set(result, "max_concurrent_children", json_new_number((double)child->max_concurrent));
    json_object_set(result, "child_max_turns", json_new_number((double)child->child_max_turns));
}

/* PoP: list_active_subagents @ src/tools/delegate.c:delegate_status */
/* Port of Python tools/delegate_tool.py:list_active_subagents(). */
static void delegate_status(int session_id, json_node_t *result) {
    int slot = find_child_by_session(session_id);
    if (slot < 0) {
        json_object_set(result, "error", json_new_string("Delegation not found"));
        return;
    }

    delegate_child_t *child = &g_children[slot];
    json_object_set(result, "session_id", json_new_number((double)child->session_id));
    json_object_set(result, "goal", json_new_string(child->goal));
    json_object_set(result, "status", json_new_string(child->running ? "running" : "completed"));
    json_object_set(result, "orchestrator", json_new_bool(child->orchestrator));
    json_object_set(result, "elapsed_seconds", json_new_number((double)(time(NULL) - child->start_time)));
}

/* PoP: interrupt_subagent @ src/tools/delegate.c:delegate_kill */
/* Port of Python tools/delegate_tool.py:interrupt_subagent(). */
/* PoP: delegate_kill @ tools/delegate_tool.py:kill */
/* PoP: delegate_kill @ environments/base:kill */
static void delegate_kill(int session_id, json_node_t *result) {
    int slot = find_child_by_session(session_id);
    if (slot < 0) {
        json_object_set(result, "error", json_new_string("Delegation not found"));
        return;
    }

    delegate_child_t *child = &g_children[slot];
    if (child->running) {
        child->running = false;
        json_object_set(result, "status", json_new_string("killed"));
    } else {
        json_object_set(result, "status", json_new_string("already_completed"));
    }
    json_object_set(result, "session_id", json_new_number((double)session_id));
}

/* PoP: list_active_subagents @ src/tools/delegate.c:delegate_list */
/* Port of Python tools/delegate_tool.py:list_active_subagents(). */
void delegate_list(json_node_t *result) {
    json_node_t *children = json_new_array();
    int count = 0;
    for (int i = 0; i < MAX_CHILDREN; i++) {
        if (g_children[i].session_id > 0) {
            json_node_t *c = json_new_object();
            json_object_set(c, "session_id", json_new_number((double)g_children[i].session_id));
            json_object_set(c, "goal", json_new_string(g_children[i].goal));
            json_object_set(c, "status", json_new_string(g_children[i].running ? "running" : "completed"));
            json_object_set(c, "orchestrator", json_new_bool(g_children[i].orchestrator));
            json_object_set(c, "elapsed_seconds", json_new_number((double)(time(NULL) - g_children[i].start_time)));
            json_array_append(children, c);
            count++;
        }
    }
    json_object_set(result, "children", children);
    json_object_set(result, "count", json_new_number((double)count));
}

/* PoP: set_spawn_paused @ src/tools/delegate.c:delegate_pause */
/* Port of Python tools/delegate_tool.py:set_spawn_paused(). */
/* PoP: delegate_pause @ tools/delegate_tool.py:pause */
/* PoP: delegate_pause @ hermes_cli/goals.py:pause */
static void delegate_pause(bool paused, json_node_t *result) {
    bool new_state = set_spawn_paused(paused);
    json_object_set(result, "paused", json_new_bool(new_state));
    json_object_set(result, "message", json_new_string(new_state ? "Delegation spawning paused" : "Delegation spawning resumed"));
}

/* PoP: list_active_subagents @ src/tools/delegate.c:delegate_health */
/* Port of Python tools/delegate_tool.py:list_active_subagents(). */
static void delegate_health(json_node_t *result) {
    int running = 0, completed = 0;
    for (int i = 0; i < MAX_CHILDREN; i++) {
        if (g_children[i].session_id > 0) {
            if (g_children[i].running) running++;
            else completed++;
        }
    }
    json_object_set(result, "status", json_new_string("healthy"));
    json_object_set(result, "running_children", json_new_number((double)running));
    json_object_set(result, "completed_children", json_new_number((double)completed));
    json_object_set(result, "max_concurrent_children", json_new_number((double)g_max_concurrent_default));
    json_object_set(result, "max_spawn_depth", json_new_number((double)g_max_spawn_depth_default));
}

/* PoP: delegate_task @ src/tools/delegate.c:delegate_handler */
/* Port of Python tools/delegate_tool.py:delegate_task(). */

/* ---------------------------------------------------------------------------
 * Subagent approval callbacks (Python: _subagent_auto_deny / _subagent_auto_approve)
 * --------------------------------------------------------------------------- */
/* PoP: _subagent_auto_deny @ tools/delegate_tool.py:_subagent_auto_deny */
/* Auto-deny dangerous commands in subagent threads (safe default). */
static const char *delegate_subagent_auto_deny(const char *command, const char *description)
{
    (void)command; (void)description;
    hermes_log(LOG_WARNING, "delegate",
        "Subagent auto-denied dangerous command: %s (%s). "
        "Set delegation.subagent_auto_approve: true to allow.",
        command ? command : "", description ? description : "");
    return "deny";
}

/* PoP: _subagent_auto_approve @ tools/delegate_tool.py:_subagent_auto_approve */
/* Auto-approve dangerous commands in subagent threads (opt-in YOLO). */
static const char *delegate_subagent_auto_approve(const char *command, const char *description)
{
    (void)command; (void)description;
    hermes_log(LOG_WARNING, "delegate",
        "Subagent auto-approved dangerous command: %s (%s)",
        command ? command : "", description ? description : "");
    return "once";
}

/* PoP: _get_subagent_approval_callback @ tools/delegate_tool.py:_get_subagent_approval_callback */
/* Return the callback to install into subagent worker threads. */
static const char *(*delegate_get_subagent_approval_callback(void))(const char *, const char *)
{
    const char *val = tool_config_get("delegation", "subagent_auto_approve");
    if (val && (strcmp(val, "true") == 0 || strcmp(val, "1") == 0 ||
                strcmp(val, "yes") == 0 || strcmp(val, "on") == 0)) {
        return delegate_subagent_auto_approve;
    }
    return delegate_subagent_auto_deny;
}

/* ---------------------------------------------------------------------------
 * Subagent registry (Python: _register_subagent / _unregister_subagent)
 * --------------------------------------------------------------------------- */
typedef struct {
    char   subagent_id[64];
    char   parent_id[64];
    int    depth;
    char   goal[1024];
    char   model[128];
    double started_at;
    int    tool_count;
    char   status[32];
    bool   active;
} delegate_subagent_record_t;

static delegate_subagent_record_t g_subagents[MAX_CHILDREN];
static pthread_mutex_t g_subagents_lock = PTHREAD_MUTEX_INITIALIZER;

/* PoP: _register_subagent @ tools/delegate_tool.py:_register_subagent */
static void delegate_register_subagent(const char *subagent_id, const char *parent_id,
                                       int depth, const char *goal, const char *model,
                                       double started_at)
{
    if (!subagent_id || !subagent_id[0]) return;
    pthread_mutex_lock(&g_subagents_lock);
    for (int i = 0; i < MAX_CHILDREN; i++) {
        if (!g_subagents[i].active) {
            snprintf(g_subagents[i].subagent_id, sizeof(g_subagents[i].subagent_id), "%s", subagent_id);
            snprintf(g_subagents[i].parent_id, sizeof(g_subagents[i].parent_id), "%s", parent_id ? parent_id : "");
            g_subagents[i].depth = depth;
            snprintf(g_subagents[i].goal, sizeof(g_subagents[i].goal), "%s", goal ? goal : "");
            snprintf(g_subagents[i].model, sizeof(g_subagents[i].model), "%s", model ? model : "");
            g_subagents[i].started_at = started_at;
            g_subagents[i].tool_count = 0;
            snprintf(g_subagents[i].status, sizeof(g_subagents[i].status), "running");
            g_subagents[i].active = true;
            break;
        }
    }
    pthread_mutex_unlock(&g_subagents_lock);
}

/* PoP: _unregister_subagent @ tools/delegate_tool.py:_unregister_subagent */
static void delegate_unregister_subagent(const char *subagent_id)
{
    if (!subagent_id) return;
    pthread_mutex_lock(&g_subagents_lock);
    for (int i = 0; i < MAX_CHILDREN; i++) {
        if (g_subagents[i].active && strcmp(g_subagents[i].subagent_id, subagent_id) == 0) {
            g_subagents[i].active = false;
            break;
        }
    }
    pthread_mutex_unlock(&g_subagents_lock);
}

/* ---------------------------------------------------------------------------
 * Output tail helpers (Python: _stringify_tool_content / _looks_like_error_output /
 * _extract_output_tail)
 * --------------------------------------------------------------------------- */
/* PoP: _stringify_tool_content @ tools/delegate_tool.py:_stringify_tool_content */
/* Stable text representation for tool-result content (string / list / dict). */
char *delegate_stringify_tool_content(const json_node_t *content, char *out, size_t out_sz)
{
    if (out && out_sz) out[0] = '\0';
    if (!content) return out;
    if (content->type == JSON_STRING) {
        snprintf(out, out_sz, "%s", content->str_val ? content->str_val : "");
        return out;
    }
    if (content->type == JSON_ARRAY) {
        size_t pos = 0;
        for (size_t i = 0; i < content->c.count; i++) {
            json_node_t *item = json_get(content, i);
            if (!item) continue;
            char buf[4096];
            if (item->type == JSON_OBJECT) {
                json_node_t *txt = json_object_get(item, "text");
                if (txt && txt->type == JSON_STRING) {
                    snprintf(buf, sizeof(buf), "%s", txt->str_val ? txt->str_val : "");
                } else {
                    char *js = json_serialize(item);
                    snprintf(buf, sizeof(buf), "%s", js ? js : "");
                    if (js) free(js);
                }
            } else {
                char *js = json_serialize(item);
                snprintf(buf, sizeof(buf), "%s", js ? js : "");
                if (js) free(js);
            }
            size_t blen = strlen(buf);
            if (pos + blen + 1 >= out_sz) { out[pos] = '\0'; break; }
            memcpy(out + pos, buf, blen);
            pos += blen;
            if (i + 1 < content->c.count && pos + 1 < out_sz) out[pos++] = '\n';
            out[pos] = '\0';
        }
        return out;
    }
    if (content->type == JSON_OBJECT) {
        char *js = json_serialize(content);
        snprintf(out, out_sz, "%s", js ? js : "");
        if (js) free(js);
        return out;
    }
    /* primitives */
    char buf[256];
    if (content->type == JSON_NUMBER) snprintf(buf, sizeof(buf), "%g", content->num_val);
    else if (content->type == JSON_BOOL) snprintf(buf, sizeof(buf), "%s", content->bool_val ? "true" : "false");
    else snprintf(buf, sizeof(buf), "%s", "unknown");
    snprintf(out, out_sz, "%s", buf);
    return out;
}

/* PoP: _looks_like_error_output @ tools/delegate_tool.py:_looks_like_error_output */
/* Conservative stderr/error detector for tool-result previews. */
bool delegate_looks_like_error_output(const json_node_t *content)
{
    char buf[16384];
    delegate_stringify_tool_content(content, buf, sizeof(buf));
    if (buf[0] == '\0') return false;
    /* skip leading whitespace */
    char *head = buf;
    while (*head == ' ' || *head == '\t' || *head == '\n' || *head == '\r') head++;
    if (head[0] == '{' || head[0] == '[') {
        char *js = strdup(buf);
        if (js) {
            json_node_t *parsed = json_parse(js, NULL);
            free(js);
            if (parsed && parsed->type == JSON_OBJECT) {
                json_node_t *err = json_object_get(parsed, "error");
                bool is_err = (err && !(err->type == JSON_NULL));
                if (!is_err) {
                    json_node_t *st = json_object_get(parsed, "status");
                    if (st && st->type == JSON_STRING) {
                        const char *sv = st->str_val ? st->str_val : "";
                        if (strcmp(sv, "error") == 0 || strcmp(sv, "failed") == 0 ||
                            strcmp(sv, "failure") == 0 || strcmp(sv, "timeout") == 0) {
                            is_err = true;
                        }
                    }
                }
                json_free(parsed);
                if (is_err) return true;
            }
        }
    }
    /* first non-empty line starts with classic error marker */
    char *nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    char *line = buf;
    while (*line == ' ' || *line == '\t') line++;
    bool res = (strncmp(line, "error:", 6) == 0 ||
                strncmp(line, "failed:", 7) == 0 ||
                strncmp(line, "traceback", 8) == 0 ||
                strncmp(line, "exception:", 10) == 0);
    if (nl) *nl = '\n';
    return res;
}

/* PoP: _extract_output_tail @ tools/delegate_tool.py:_extract_output_tail */
/* Pull the last N tool-call results from a child's conversation. */
static json_node_t *delegate_extract_output_tail(const json_node_t *result,
                                                 int max_entries, int max_chars)
{
    json_node_t *tail = json_new_array();
    if (max_entries <= 0) max_entries = 12;
    if (max_chars <= 0) max_chars = 8000;
    if (!result || result->type != JSON_OBJECT) return tail;
    json_node_t *messages = json_object_get(result, "messages");
    if (!messages || messages->type != JSON_ARRAY) return tail;

    /* Forward pass: tool_call_id -> tool name */
    char tool_of_call[256][64];
    size_t call_n = 0;
    for (size_t i = 0; i < messages->c.count && call_n < 256; i++) {
        json_node_t *msg = json_get(messages, i);
        if (!msg || msg->type != JSON_OBJECT) continue;
        json_node_t *role = json_object_get(msg, "role");
        if (!role || role->type != JSON_STRING || strcmp(role->str_val, "assistant") != 0) continue;
        json_node_t *tcs = json_object_get(msg, "tool_calls");
        if (!tcs || tcs->type != JSON_ARRAY) continue;
        for (size_t j = 0; j < tcs->c.count; j++) {
            json_node_t *tc = json_get(tcs, j);
            if (!tc || tc->type != JSON_OBJECT) continue;
            json_node_t *id = json_object_get(tc, "id");
            if (!id || id->type != JSON_STRING) continue;
            if (call_n < 256) {
                snprintf(tool_of_call[call_n], sizeof(tool_of_call[0]), "%s", id->str_val);
                /* store name right after id using a parallel array */
                call_n++;
            }
        }
    }
    /* Map id -> name */
    char name_of_call[256][64];
    call_n = 0;
    for (size_t i = 0; i < messages->c.count && call_n < 256; i++) {
        json_node_t *msg = json_get(messages, i);
        if (!msg || msg->type != JSON_OBJECT) continue;
        json_node_t *role = json_object_get(msg, "role");
        if (!role || role->type != JSON_STRING || strcmp(role->str_val, "assistant") != 0) continue;
        json_node_t *tcs = json_object_get(msg, "tool_calls");
        if (!tcs || tcs->type != JSON_ARRAY) continue;
        for (size_t j = 0; j < tcs->c.count; j++) {
            json_node_t *tc = json_get(tcs, j);
            if (!tc || tc->type != JSON_OBJECT) continue;
            json_node_t *id = json_object_get(tc, "id");
            json_node_t *fn = json_object_get(tc, "function");
            if (!id || id->type != JSON_STRING) continue;
            const char *fname = "tool";
            if (fn && fn->type == JSON_OBJECT) {
                json_node_t *fname_n = json_object_get(fn, "name");
                if (fname_n && fname_n->type == JSON_STRING && fname_n->str_val) fname = fname_n->str_val;
            }
            if (call_n < 256) {
                snprintf(name_of_call[call_n], sizeof(name_of_call[0]), "%s", fname);
                call_n++;
            }
        }
    }

    /* Reverse pass: pick tool results, newest first */
    int collected = 0;
    for (size_t ri = 0; ri < messages->c.count; ri++) {
        size_t idx = messages->c.count - 1 - ri;
        if (collected >= max_entries) break;
        json_node_t *msg = json_get(messages, idx);
        if (!msg || msg->type != JSON_OBJECT) continue;
        json_node_t *role = json_object_get(msg, "role");
        if (!role || role->type != JSON_STRING || strcmp(role->str_val, "tool") != 0) continue;
        json_node_t *content = json_object_get(msg, "content");
        char preview[16384];
        delegate_stringify_tool_content(content, preview, sizeof(preview));
        bool is_error = delegate_looks_like_error_output(content);
        const char *tool_name = "tool";
        json_node_t *tcid = json_object_get(msg, "tool_call_id");
        if (tcid && tcid->type == JSON_STRING) {
            for (size_t k = 0; k < call_n; k++) {
                if (strcmp(tool_of_call[k], tcid->str_val) == 0) { tool_name = name_of_call[k]; break; }
            }
        }
        json_node_t *entry = json_new_object();
        json_object_set(entry, "tool", json_new_string(tool_name));
        char capped[16384];
        size_t plen = strlen(preview);
        if (plen > (size_t)max_chars) { memcpy(capped, preview, max_chars); capped[max_chars] = '\0'; }
        else memcpy(capped, preview, plen + 1);
        json_object_set(entry, "preview", json_new_string(capped));
        json_object_set(entry, "is_error", json_new_bool(is_error));
        json_array_append(tail, entry);
        collected++;
    }
    /* reverse to chronological */
    size_t n = tail->c.count;
    for (size_t a = 0; a < n / 2; a++) {
        json_node_t *tmp = tail->c.items[a];
        tail->c.items[a] = tail->c.items[n - 1 - a];
        tail->c.items[n - 1 - a] = tmp;
    }
    return tail;
}

/* ---------------------------------------------------------------------------
 * Config getters (Python: _get_max_concurrent_children / _get_max_async_children /
 * _get_child_timeout / _get_max_spawn_depth / _get_orchestrator_enabled /
 * _get_inherit_mcp_toolsets / _get_subagent_approval_callback handled above)
 * --------------------------------------------------------------------------- */
static bool g_high_concurrency_warned = false;

/* PoP: _get_max_concurrent_children @ tools/delegate_tool.py:_get_max_concurrent_children */
static int delegate_get_max_concurrent_children(void)
{
    const char *val = tool_config_get("delegation", "max_concurrent_children");
    if (val && val[0]) {
        char *end = NULL;
        long r = strtol(val, &end, 10);
        if (end != val && *end == '\0') {
            int result = (int)((r < 1) ? 1 : r);
            if (result > 10) {
                if (!g_high_concurrency_warned) {
                    g_high_concurrency_warned = true;
                    hermes_log(LOG_WARNING, "delegate",
                        "delegation.max_concurrent_children=%d: each child consumes API tokens "
                        "independently. High values multiply cost linearly.", result);
                }
            }
            return result;
        }
        hermes_log(LOG_WARNING, "delegate",
            "delegation.max_concurrent_children=%s is not a valid integer; using default %d",
            val, DEL_DEFAULT_MAX_CONCURRENT_CHILDREN);
        return DEL_DEFAULT_MAX_CONCURRENT_CHILDREN;
    }
    const char *env = getenv("DELEGATION_MAX_CONCURRENT_CHILDREN");
    if (env && env[0]) {
        long r = strtol(env, NULL, 10);
        if (r >= 1) return (int)r;
    }
    return DEL_DEFAULT_MAX_CONCURRENT_CHILDREN;
}

/* PoP: _get_max_async_children @ tools/delegate_tool.py:_get_max_async_children */
static int delegate_get_max_async_children(void)
{
    const char *val = tool_config_get("delegation", "max_async_children");
    if (val && val[0]) {
        long r = strtol(val, NULL, 10);
        if (r >= 1) return (int)r;
        hermes_log(LOG_WARNING, "delegate",
            "delegation.max_async_children=%s is not a valid integer; using default %d",
            val, DEL_DEFAULT_MAX_ASYNC_CHILDREN);
        return DEL_DEFAULT_MAX_ASYNC_CHILDREN;
    }
    const char *env = getenv("DELEGATION_MAX_ASYNC_CHILDREN");
    if (env && env[0]) {
        long r = strtol(env, NULL, 10);
        if (r >= 1) return (int)r;
    }
    return DEL_DEFAULT_MAX_ASYNC_CHILDREN;
}

/* PoP: _get_child_timeout @ tools/delegate_tool.py:_get_child_timeout */
static double delegate_get_child_timeout(void)
{
    const char *val = tool_config_get("delegation", "child_timeout_seconds");
    if (val && val[0]) {
        char *end = NULL;
        double parsed = strtod(val, &end);
        if (end != val && *end == '\0')
            return (parsed <= 0.0) ? 0.0 : (parsed < 30.0 ? 30.0 : parsed);
        hermes_log(LOG_WARNING, "delegate",
            "delegation.child_timeout_seconds=%s is not a valid number; using default (no timeout)", val);
    }
    const char *env = getenv("DELEGATION_CHILD_TIMEOUT_SECONDS");
    if (env && env[0]) {
        double parsed = strtod(env, NULL);
        return (parsed <= 0.0) ? 0.0 : (parsed < 30.0 ? 30.0 : parsed);
    }
    return 0.0; /* None -> no timeout */
}

/* PoP: _get_max_spawn_depth @ tools/delegate_tool.py:_get_max_spawn_depth */
static int delegate_get_max_spawn_depth(void)
{
    const char *val = tool_config_get("delegation", "max_spawn_depth");
    if (!val || !val[0]) return DEL_DEFAULT_MAX_SPAWN_DEPTH;
    char *end = NULL;
    long ival = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        hermes_log(LOG_WARNING, "delegate",
            "delegation.max_spawn_depth=%s is not a valid integer; using default %d", val, DEL_DEFAULT_MAX_SPAWN_DEPTH);
        return DEL_DEFAULT_MAX_SPAWN_DEPTH;
    }
    long floored = (ival < DEL_MIN_SPAWN_DEPTH) ? DEL_MIN_SPAWN_DEPTH : ival;
    if (floored != ival) {
        hermes_log(LOG_WARNING, "delegate",
            "delegation.max_spawn_depth=%ld below floor %d; using %ld", ival, DEL_MIN_SPAWN_DEPTH, floored);
    }
    return (int)floored;
}

/* PoP: _get_orchestrator_enabled @ tools/delegate_tool.py:_get_orchestrator_enabled */
static bool delegate_get_orchestrator_enabled(void)
{
    const char *val = tool_config_get("delegation", "orchestrator_enabled");
    if (!val) return true;
    if (strcmp(val, "true") == 0 || strcmp(val, "1") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) return true;
    if (strcmp(val, "false") == 0 || strcmp(val, "0") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) return false;
    return true; /* default */
}

/* PoP: _get_inherit_mcp_toolsets @ tools/delegate_tool.py:_get_inherit_mcp_toolsets */
static bool delegate_get_inherit_mcp_toolsets(void)
{
    const char *val = tool_config_get("delegation", "inherit_mcp_toolsets");
    if (!val) return true;
    if (strcmp(val, "true") == 0 || strcmp(val, "1") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) return true;
    return false;
}

/* ---------------------------------------------------------------------------
 * Toolset helpers (Python: _is_mcp_toolset_name / _expand_parent_toolsets /
 * _preserve_parent_mcp_toolsets / _strip_blocked_tools)
 * --------------------------------------------------------------------------- */
/* PoP: _is_mcp_toolset_name @ tools/delegate_tool.py:_is_mcp_toolset_name */
static bool delegate_is_mcp_toolset_name(const char *name)
{
    if (!name || !name[0]) return false;
    /* Canonical MCP toolsets are mcp-* prefixed. */
    if (strncmp(name, "mcp-", 4) == 0) return true;
    return false;
}

/* PoP: _expand_parent_toolsets @ tools/delegate_tool.py:_expand_parent_toolsets */
/* Expand composite toolsets so individual toolset names are recognized. */
static void delegate_expand_parent_toolsets(const char **parent_toolsets, int parent_n,
                                            char out[256][64], int *out_n)
{
    int n = 0;
    /* First collect all tools belonging to the parent's toolsets. */
    char parent_tool_names[1024][64];
    int ptn = 0;
    for (int p = 0; p < parent_n; p++) {
        const char *pts = parent_toolsets[p];
        if (!pts) continue;
        size_t count = registry_count();
        for (size_t i = 0; i < count && ptn < 1024; i++) {
            const char *tool_name = registry_get_name(i);
            const char *ts = registry_get_toolset(tool_name);
            if (ts && strcmp(ts, pts) == 0) {
                bool dup = false;
                for (int k = 0; k < ptn; k++) if (strcmp(parent_tool_names[k], tool_name) == 0) { dup = true; break; }
                if (!dup) snprintf(parent_tool_names[ptn++], sizeof(parent_tool_names[0]), "%s", tool_name);
            }
        }
    }
    /* Seed output with the original parent toolset names. */
    for (int p = 0; p < parent_n && n < 256; p++) {
        if (parent_toolsets[p]) snprintf(out[n++], sizeof(out[0]), "%s", parent_toolsets[p]);
    }
    if (ptn == 0) { *out_n = n; return; }
    size_t count = registry_count();
    /* Enumerate toolsets by scanning distinct toolset labels among registered tools. */
    char seen_toolsets[256][64];
    int st_n = 0;
    for (size_t i = 0; i < count && st_n < 256; i++) {
        const char *tool_name = registry_get_name(i);
        const char *ts = registry_get_toolset(tool_name);
        if (!ts || !ts[0]) continue;
        bool dup = false;
        for (int k = 0; k < st_n; k++) if (strcmp(seen_toolsets[k], ts) == 0) { dup = true; break; }
        if (dup) continue;
        /* skip if already in expanded set */
        bool already = false;
        for (int k = 0; k < n; k++) if (strcmp(out[k], ts) == 0) { already = true; break; }
        if (already) { snprintf(seen_toolsets[st_n++], sizeof(seen_toolsets[0]), "%s", ts); continue; }
        /* check if every tool in this toolset is a subset of parent tool names */
        int ts_tool_n = 0; bool all_subset = true;
        for (size_t j = 0; j < count; j++) {
            const char *jn = registry_get_name(j);
            const char *jts = registry_get_toolset(jn);
            if (jts && strcmp(jts, ts) == 0) {
                ts_tool_n++;
                bool found = false;
                for (int k = 0; k < ptn; k++) if (strcmp(parent_tool_names[k], jn) == 0) { found = true; break; }
                if (!found) { all_subset = false; break; }
            }
        }
        if (ts_tool_n > 0 && all_subset) {
            snprintf(out[n++], sizeof(out[0]), "%s", ts);
        }
        snprintf(seen_toolsets[st_n++], sizeof(seen_toolsets[0]), "%s", ts);
    }
    *out_n = n;
}

/* PoP: _preserve_parent_mcp_toolsets @ tools/delegate_tool.py:_preserve_parent_mcp_toolsets */
/* Append any parent MCP toolsets missing from a narrowed child. */
static void delegate_preserve_parent_mcp_toolsets(const char **child_toolsets, int child_n,
                                                  const char **parent_toolsets, int parent_n,
                                                  char out[256][64], int *out_n)
{
    int n = 0;
    for (int i = 0; i < child_n && n < 256; i++)
        if (child_toolsets[i]) snprintf(out[n++], sizeof(out[0]), "%s", child_toolsets[i]);
    for (int p = 0; p < parent_n && n < 256; p++) {
        const char *pn = parent_toolsets[p];
        if (!pn) continue;
        if (!delegate_is_mcp_toolset_name(pn)) continue;
        bool present = false;
        for (int k = 0; k < n; k++) if (strcmp(out[k], pn) == 0) { present = true; break; }
        if (!present) snprintf(out[n++], sizeof(out[0]), "%s", pn);
    }
    *out_n = n;
}

/* PoP: _strip_blocked_tools @ tools/delegate_tool.py:_strip_blocked_tools */
/* Remove toolsets that contain only blocked tools + composite blocked toolsets. */
static void delegate_strip_blocked_tools(const char **toolsets, int n,
                                         char out[256][64], int *out_n)
{
    int on = 0;
    for (int i = 0; i < n && on < 256; i++) {
        const char *ts = toolsets[i];
        if (!ts) continue;
        /* composite blocked toolsets */
        bool composite = false;
        for (int c = 0; g_delegate_composite_blocked_toolsets[c]; c++) {
            if (strcmp(ts, g_delegate_composite_blocked_toolsets[c]) == 0) { composite = true; break; }
        }
        if (composite) continue;
        /* toolset whose every tool is in the blocked-tool list */
        size_t count = registry_count();
        int ts_tool_n = 0; bool all_blocked = true; bool has_any = false;
        for (size_t j = 0; j < count; j++) {
            const char *jn = registry_get_name(j);
            const char *jts = registry_get_toolset(jn);
            if (jts && strcmp(jts, ts) == 0) {
                has_any = true; ts_tool_n++;
                bool blocked = false;
                for (int b = 0; g_delegate_blocked_tool_names[b]; b++) {
                    if (strcmp(jn, g_delegate_blocked_tool_names[b]) == 0) { blocked = true; break; }
                }
                if (!blocked) { all_blocked = false; break; }
            }
        }
        if (has_any && ts_tool_n > 0 && all_blocked) continue;
        snprintf(out[on++], sizeof(out[0]), "%s", ts);
    }
    *out_n = on;
}

/* ---------------------------------------------------------------------------
 * Misc pure helpers (Python: check_delegate_requirements / _normalize_role /
 * _build_child_system_prompt / _resolve_workspace_hint / _build_*
 * _description / _acp_binary_available / _model_background_value)
 * --------------------------------------------------------------------------- */
/* PoP: check_delegate_requirements @ tools/delegate_tool.py:check_delegate_requirements */
/* Delegation has no external requirements -- always available. */
static bool delegate_check_requirements(void) { return true; }

/* PoP: _normalize_role @ tools/delegate_tool.py:_normalize_role */
void delegate_normalize_role(const char *r, char *out, size_t out_sz)
{
    if (!r || !r[0]) { snprintf(out, out_sz, "leaf"); return; }
    char norm[64];
    snprintf(norm, sizeof(norm), "%s", r);
    for (char *p = norm; *p; p++) *p = (char)tolower((unsigned char)*p);
    if (strcmp(norm, "leaf") == 0 || strcmp(norm, "orchestrator") == 0) {
        snprintf(out, out_sz, "%s", norm);
    } else {
        hermes_log(LOG_WARNING, "delegate", "Unknown delegate_task role=%s, coercing to 'leaf'", r);
        snprintf(out, out_sz, "leaf");
    }
}

/* PoP: _build_child_system_prompt @ tools/delegate_tool.py:_build_child_system_prompt */
static char *delegate_build_child_system_prompt(const char *goal, const char *context,
                                                const char *workspace_path, const char *role,
                                                int max_spawn_depth, int child_depth)
{
    size_t cap = 8192;
    char *buf = (char *)malloc(cap);
    if (!buf) return strdup("");
    size_t len = 0;
    len += snprintf(buf + len, cap - len,
        "You are a focused subagent working on a specific delegated task.\n\n"
        "YOUR TASK:\n%s", goal ? goal : "");
    if (context && context[0]) {
        len += snprintf(buf + len, cap - len, "\n\nCONTEXT:\n%s", context);
    }
    if (workspace_path && workspace_path[0]) {
        len += snprintf(buf + len, cap - len,
            "\n\nWORKSPACE PATH:\n%s\n"
            "Use this exact path for local repository/workdir operations unless the task explicitly says otherwise.",
            workspace_path);
    }
    len += snprintf(buf + len, cap - len,
        "\n\nComplete this task using the tools available to you. "
        "When finished, provide a clear, concise summary of:\n"
        "- What you did\n- What you found or accomplished\n"
        "- Any files you created or modified\n- Any issues encountered\n\n"
        "Important workspace rule: Never assume a repository lives at /workspace/... or any other "
        "container-style path unless the task/context explicitly gives that path. "
        "If no exact local path is provided, discover it first before issuing git/workdir-specific commands.\n\n"
        "Keep your final summary tight: lead with outcomes, prefer bullet points over paragraphs, "
        "and don't replay your whole process. Your response is returned to the parent agent as a "
        "summary, and overlong summaries crowd out the parent's context window.");
    if (role && strcmp(role, "orchestrator") == 0) {
        const char *child_note;
        if (child_depth + 1 >= max_spawn_depth) {
            child_note = "Your own children MUST be leaves (cannot delegate further) because they would "
                         "be at the depth floor - you cannot pass role='orchestrator' to your own "
                         "delegate_task calls.";
        } else {
            child_note = "Your own children can themselves be orchestrators or leaves, depending on the "
                         "`role` you pass to delegate_task. Default is 'leaf'; pass role='orchestrator' "
                         "explicitly when a child needs to further decompose its work.";
        }
        len += snprintf(buf + len, cap - len,
            "\n\n## Subagent Spawning (Orchestrator Role)\n"
            "You have access to the `delegate_task` tool and CAN spawn your own subagents to "
            "parallelize independent work.\n\n"
            "WHEN to delegate:\n"
            "- The goal decomposes into 2+ independent subtasks that can run in parallel.\n"
            "- A subtask is reasoning-heavy and would flood your context with intermediate data.\n\n"
            "WHEN NOT to delegate:\n"
            "- Single-step mechanical work - do it directly.\n"
            "- Trivial tasks you can execute in one or two tool calls.\n"
            "- Re-delegating your entire assigned goal to one worker (that's just pass-through).\n\n"
            "Coordinate your workers' results and synthesize them before reporting back to your parent. "
            "You are responsible for the final summary, not your workers.\n\n"
            "NOTE: You are at depth %d. The delegation tree is capped at max_spawn_depth=%d. %s",
            child_depth, max_spawn_depth, child_note);
    }
    return buf;
}

/* PoP: _resolve_workspace_hint @ tools/delegate_tool.py:_resolve_workspace_hint */
static char *delegate_resolve_workspace_hint(const char *terminal_cwd, const char *cwd)
{
    const char *candidates[4];
    candidates[0] = getenv("TERMINAL_CWD");
    candidates[1] = terminal_cwd;
    candidates[2] = cwd;
    candidates[3] = NULL;
    for (int i = 0; candidates[i]; i++) {
        const char *c = candidates[i];
        if (!c || !c[0]) continue;
        if (c[0] == '/') {
            /* best-effort: require an existing absolute dir */
            if (access(c, F_OK) == 0 && access(c, R_OK) == 0) return strdup(c);
        }
    }
    return NULL;
}

/* PoP: _build_top_level_description @ tools/delegate_tool.py:_build_top_level_description */
static char *delegate_build_top_level_description(void)
{
    int max_children = delegate_get_max_concurrent_children();
    int max_depth = delegate_get_max_spawn_depth();
    bool orch_on = delegate_get_orchestrator_enabled();
    char nesting[512];
    if (max_depth >= 2 && orch_on) {
        snprintf(nesting, sizeof(nesting),
            "Nested delegation IS enabled for this user "
            "(max_spawn_depth=%d): pass role='orchestrator' on a child to let it spawn its "
            "own workers, up to %d additional level(s) deep.", max_depth, max_depth - 1);
    } else if (max_depth >= 2 && !orch_on) {
        snprintf(nesting, sizeof(nesting),
            "Nested delegation is DISABLED on this install "
            "(delegation.orchestrator_enabled=false), even though max_spawn_depth=%d. "
            "role='orchestrator' is silently forced to 'leaf'.", max_depth);
    } else {
        snprintf(nesting, sizeof(nesting),
            "Nested delegation is OFF for this user (max_spawn_depth=%d): every child is a leaf "
            "and cannot delegate further. Raise delegation.max_spawn_depth in config.yaml to "
            "enable nesting.", max_depth);
    }
    size_t cap = 4096;
    char *buf = (char *)malloc(cap);
    if (!buf) return strdup("");
    snprintf(buf, cap,
        "Spawn one or more subagents to work on tasks in isolated contexts. "
        "Each subagent gets its own conversation, terminal session, and toolset. "
        "Only the final summary is returned -- intermediate tool results never enter your context window.\n\n"
        "TWO MODES (one of 'goal' or 'tasks' is required):\n"
        "1. Single task: provide 'goal' (+ optional context, toolsets).\n"
        "2. Batch (parallel): provide 'tasks' array with up to %d items concurrently for this user "
        "(configured via delegation.max_concurrent_children in config.yaml). %s\n\n"
        "BOTH MODES RUN IN THE BACKGROUND. delegate_task returns immediately - you and the user keep "
        "working, and each subagent's full result re-enters the conversation as its own new message "
        "when it finishes.\n\n"
        "Leaf subagents (role='leaf', the default) CANNOT call: delegate_task, clarify, memory, "
        "send_message, execute_code.\n"
        "Orchestrator subagents (role='orchestrator') retain delegate_task so they can spawn their own "
        "workers, but still cannot use clarify, memory, send_message, or execute_code. "
        "Orchestrators are bounded by max_spawn_depth=%d for this user and can be disabled globally via "
        "delegation.orchestrator_enabled=false.\n"
        "Results are always returned as an array, one entry per task.",
        max_children, nesting, max_depth);
    return buf;
}

/* PoP: _build_tasks_param_description @ tools/delegate_tool.py:_build_tasks_param_description */
static char *delegate_build_tasks_param_description(void)
{
    int max_children = delegate_get_max_concurrent_children();
    size_t cap = 512;
    char *buf = (char *)malloc(cap);
    if (!buf) return strdup("");
    snprintf(buf, cap,
        "Batch mode: tasks to run in parallel (up to %d for this user, set via "
        "delegation.max_concurrent_children). Each gets its own subagent with isolated context and "
        "terminal session. When provided, top-level goal/context/toolsets are ignored.",
        max_children);
    return buf;
}

/* PoP: _build_role_param_description @ tools/delegate_tool.py:_build_role_param_description */
static char *delegate_build_role_param_description(void)
{
    int max_depth = delegate_get_max_spawn_depth();
    bool orch_on = delegate_get_orchestrator_enabled();
    char nesting[512];
    if (max_depth >= 2 && orch_on)
        snprintf(nesting, sizeof(nesting),
            "Nesting IS enabled for this user (max_spawn_depth=%d): orchestrator children can "
            "themselves delegate up to %d more level(s) deep.", max_depth, max_depth - 1);
    else if (max_depth >= 2 && !orch_on)
        snprintf(nesting, sizeof(nesting),
            "Nesting is currently disabled (delegation.orchestrator_enabled=false); 'orchestrator' "
            "is silently forced to 'leaf'.");
    else
        snprintf(nesting, sizeof(nesting),
            "Nesting is OFF for this user (max_spawn_depth=%d); 'orchestrator' is silently forced "
            "to 'leaf'. Raise delegation.max_spawn_depth in config.yaml to enable.", max_depth);
    size_t cap = 512;
    char *buf = (char *)malloc(cap);
    if (!buf) return strdup("");
    snprintf(buf, cap,
        "Role of the child agent. 'leaf' (default) = focused worker, cannot delegate further. "
        "'orchestrator' = can use delegate_task to spawn its own workers. %s", nesting);
    return buf;
}

/* PoP: _acp_binary_available @ tools/delegate_tool.py:_acp_binary_available */
static bool delegate_acp_binary_available(void)
{
    const char *bins[] = { "copilot", "claude", "codex", NULL };
    for (int i = 0; bins[i]; i++) {
        char path[512];
        snprintf(path, sizeof(path), "/usr/bin/%s", bins[i]);
        if (access(path, X_OK) == 0) return true;
        snprintf(path, sizeof(path), "/usr/local/bin/%s", bins[i]);
        if (access(path, X_OK) == 0) return true;
        const char *penv = getenv("PATH");
        if (penv) {
            char *tok = strtok(strdup(penv), ":");
            while (tok) {
                snprintf(path, sizeof(path), "%s/%s", tok, bins[i]);
                if (access(path, X_OK) == 0) { free(tok); return true; }
                tok = strtok(NULL, ":");
            }
        }
    }
    return false;
}

/* PoP: _model_background_value @ tools/delegate_tool.py:_model_background_value */
/* Background flag for the model-facing dispatch path. */
static bool delegate_model_background_value(int delegate_depth)
{
    return delegate_depth <= 0;
}

/* PoP: delegate_task @ src/tools/delegate.c:delegate_handler */
/* Port of Python tools/delegate_tool.py:delegate_task(). */
char *delegate_handler(const char *args_json, const char *task_id) {
    (void)task_id; /* reserved for future multi-tenant support */

    if (!args_json || !args_json[0]) {
        return strdup("{\"error\":\"No args\"}");
    }

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) {
        if (err) return (char *)err;  /* Caller must free */
        return strdup("JSON parse error");
    }

    const char *action = json_object_get_string(args, "action", "start");
    json_node_t *result = json_new_object();

    if (strcmp(action, "start") == 0) {
        const char *goal = json_object_get_string(args, "goal", NULL);
        const char *context = json_object_get_string(args, "context", NULL);
        json_node_t *subtasks = json_object_get(args, "subtasks");
        int max_concurrent = (int)json_object_get_number(args, "max_concurrent_children", 0);
        int child_max_turns = (int)json_object_get_number(args, "child_max_turns", 0);
        bool orchestrator = json_object_get_bool(args, "orchestrator", false);
        delegate_start(goal, context, subtasks, max_concurrent, child_max_turns, orchestrator, result);
    } else if (strcmp(action, "status") == 0) {
        int session_id = (int)json_object_get_number(args, "session_id", 0);
        delegate_status(session_id, result);
    } else if (strcmp(action, "kill") == 0) {
        int session_id = (int)json_object_get_number(args, "session_id", 0);
        delegate_kill(session_id, result);
    } else if (strcmp(action, "list") == 0) {
        delegate_list(result);
    } else if (strcmp(action, "pause") == 0) {
        bool paused = json_object_get_bool(args, "paused", false);
        delegate_pause(paused, result);
    } else if (strcmp(action, "health") == 0) {
        delegate_health(result);
    } else {
        json_object_set(result, "error", json_new_string("Unknown action"));
    }

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}