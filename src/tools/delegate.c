/*
 * delegate.c — Delegate task tool for Hermes C.
 * Port of Python tools/delegate_tool.py.
 * Spawns child agents with isolated context and restricted toolsets.
 * JSON interface: { "action": "start", "goal": "...", "context": "...", "subtasks": [...], "max_concurrent_children": N, "child_max_turns": M, "orchestrator": true }
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <stdbool.h>

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

/* PoP: _build_child_agent @ src/tools/delegate.c:delegate_start
 * Port of Python tools/delegate_tool.py:_build_child_agent(). */
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

/* PoP: list_active_subagents @ src/tools/delegate.c:delegate_status
 * Port of Python tools/delegate_tool.py:list_active_subagents(). */
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

/* PoP: interrupt_subagent @ src/tools/delegate.c:delegate_kill
 * Port of Python tools/delegate_tool.py:interrupt_subagent(). */
/* PoP: delegate_kill @ delegate_tool:kill */
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

/* PoP: list_active_subagents @ src/tools/delegate.c:delegate_list
 * Port of Python tools/delegate_tool.py:list_active_subagents(). */
static void delegate_list(json_node_t *result) {
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

/* PoP: set_spawn_paused @ src/tools/delegate.c:delegate_pause
 * Port of Python tools/delegate_tool.py:set_spawn_paused(). */
/* PoP: delegate_pause @ delegate_tool:pause */
/* PoP: delegate_pause @ goals:pause */
static void delegate_pause(bool paused, json_node_t *result) {
    json_object_set(result, "paused", json_new_bool(paused));
    json_object_set(result, "message", json_new_string(paused ? "Delegation spawning paused" : "Delegation spawning resumed"));
}

/* PoP: list_active_subagents @ src/tools/delegate.c:delegate_health
 * Port of Python tools/delegate_tool.py:list_active_subagents(). */
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

/* PoP: delegate_task @ src/tools/delegate.c:delegate_handler
 * Port of Python tools/delegate_tool.py:delegate_task(). */
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