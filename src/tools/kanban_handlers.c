/*
 * kanban_handlers.c — Kanban tool handlers + registration.
 *
 * This is the tool-facing half of what used to be the file-based monolith
 * src/tools/kanban.c (972 lines). It is split from the storage backend
 * (now src/tools/kanban_adapter.c, which delegates to the sqlite engine in
 * kanban_db.h). The handlers here contain NO storage logic — they parse args,
 * apply the worker/orchestrator scoping rules, and call the adapter API.
 *
 * Minimal includes. No god header.
 *
 * PoP: faithful port of tools/kanban_tools.py handlers.
 */

#include "hermes_kanban.h"
#include "hermes_json.h"
#include "hermes_core_types.h"
#include "hermes_agent.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_PATH 4096
#define KANBAN_LIST_LIMIT_DEFAULT 50
#define KANBAN_LIST_LIMIT_MAX 200

/* ---- scoping helpers (ported from kanban_tools.py) ---- */

static bool kanban_mode(void) {
    return getenv("HERMES_KANBAN_TASK") != NULL;
}
static bool kanban_orchestrator(void) {
    return getenv("HERMES_KANBAN_TASK") == NULL;
}
static const char *default_task_id(const char *arg) {
    if (arg && *arg) return arg;
    return getenv("HERMES_KANBAN_TASK");
}
static const char *enforce_ownership(const char *tid) {
    const char *env_tid = getenv("HERMES_KANBAN_TASK");
    if (!env_tid) return NULL;
    if (strcmp(tid, env_tid) != 0) return "worker is scoped to a different task";
    return NULL;
}
static const char *require_orchestrator(const char *tool_name) {
    (void)tool_name;
    if (kanban_mode()) return "orchestrator-only tool";
    return NULL;
}

/* ---- handlers ---- */

static char *handle_show(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");
    const char *tid = default_task_id(json_get_str(args, "task_id", ""));
    if (!tid || !*tid) {
        json_free(args);
        return strdup("{\"error\":\"task_id is required (or set HERMES_KANBAN_TASK)\"}");
    }
    json_t *task = kanban_read_task(tid);
    if (!task) {
        char err[256];
        snprintf(err, sizeof(err), "{\"error\":\"task %s not found\"}", tid);
        json_free(args);
        return strdup(err);
    }
    char *out = json_serialize(task);
    json_free(task);
    json_free(args);
    return out ? out : strdup("{\"error\":\"OOM\"}");
}

static char *handle_list(const char *args_json, const char *task_id) {
    (void)task_id;
    const char *guard = require_orchestrator("kanban_list");
    if (guard) return strdup("{\"error\":\"orchestrator-only\"}");

    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");

    const char *assignee = json_get_str(args, "assignee", "");
    const char *status   = json_get_str(args, "status", "");
    const char *tenant   = json_get_str(args, "tenant", "");
    bool include_archived = json_get_bool(args, "include_archived", false);
    int limit = (int)json_get_num(args, "limit", KANBAN_LIST_LIMIT_DEFAULT);
    if (limit < 1) limit = 1;
    if (limit > KANBAN_LIST_LIMIT_MAX) limit = KANBAN_LIST_LIMIT_MAX;

    /* The engine's list covers all boards; for the tool we filter by the
     * default board via the adapter. We gather via kanban_read_task per id. */
    char **ids = kanban_all_task_ids(&limit);  /* adapter helper */
    json_t *tasks = json_array();
    int count = 0;
    bool truncated = false;
    for (int i = 0; ids && ids[i]; i++) {
        json_t *t = kanban_read_task(ids[i]);
        if (!t) continue;
        const char *t_status = json_get_str(t, "status", "");
        const char *t_assignee = json_get_str(t, "assignee", "");
        const char *t_tenant = json_get_str(t, "tenant", "");
        bool skip = false;
        if (*status && strcmp(t_status, status) != 0) skip = true;
        if (*assignee && strcmp(t_assignee, assignee) != 0) skip = true;
        if (*tenant && strcmp(t_tenant, tenant) != 0) skip = true;
        if (!include_archived && strcmp(t_status, "archived") == 0) skip = true;
        if (!skip) {
            if (count >= limit) { truncated = true; json_free(t); break; }
            json_append(tasks, t);
            count++;
        } else json_free(t);
    }
    kanban_all_task_ids_free(ids);

    json_t *result = json_object();
    json_set(result, "tasks", tasks);
    json_set(result, "count", json_number((double)count));
    json_set(result, "limit", json_number((double)limit));
    json_set(result, "truncated", json_bool(truncated));
    char *out = json_serialize(result);
    json_free(result);
    json_free(args);
    return out ? out : strdup("{\"error\":\"OOM\"}");
}

static char *handle_complete(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");
    const char *tid = default_task_id(json_get_str(args, "task_id", ""));
    if (!tid || !*tid) { json_free(args); return strdup("{\"error\":\"task_id is required\"}"); }
    const char *ownership = enforce_ownership(tid);
    if (ownership) { json_free(args); return strdup("{\"error\":\"ownership violation\"}"); }
    const char *summary = json_get_str(args, "summary", "");
    const char *result_str = json_get_str(args, "result", "");
    const char *metadata = json_get_str(args, "metadata", "");
    if (!*summary && !*result_str) {
        json_free(args);
        return strdup("{\"error\":\"provide at least one of: summary or result\"}");
    }
    bool ok = kanban_complete_task(tid, *summary ? summary : NULL,
                                   *result_str ? result_str : NULL,
                                   *metadata ? metadata : NULL);
    json_free(args);
    if (!ok) { char err[256]; snprintf(err,sizeof(err),"{\"error\":\"task %s not found\"}",tid); return strdup(err); }
    return strdup("{\"ok\":true,\"status\":\"done\"}");
}

static char *handle_block(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");
    const char *tid = default_task_id(json_get_str(args, "task_id", ""));
    const char *reason = json_get_str(args, "reason", "");
    bool sticky = json_get_bool(args, "sticky", false);
    if (!tid || !*tid) { json_free(args); return strdup("{\"error\":\"task_id is required\"}"); }
    const char *ownership = enforce_ownership(tid);
    if (ownership) { json_free(args); return strdup("{\"error\":\"ownership violation\"}"); }
    json_t *t = kanban_read_task(tid);
    if (!t) { json_free(args); char err[256]; snprintf(err,sizeof(err),"{\"error\":\"task %s not found\"}",tid); return strdup(err); }
    /* engine block */
    bool ok = kanban_block_task(tid, reason, sticky ? "sticky" : NULL) ? true : false;
    json_free(t);
    json_free(args);
    if (!ok) return strdup("{\"error\":\"block failed\"}");
    char r[256]; snprintf(r,sizeof(r),"{\"ok\":true,\"status\":\"blocked\",\"sticky\":%s}", sticky?"true":"false");
    return strdup(r);
}

static char *handle_heartbeat(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");
    const char *tid = default_task_id(json_get_str(args, "task_id", ""));
    if (!tid || !*tid) { json_free(args); return strdup("{\"error\":\"task_id is required\"}"); }
    const char *ownership = enforce_ownership(tid);
    if (ownership) { json_free(args); return strdup("{\"error\":\"ownership violation\"}"); }
    json_t *t = kanban_read_task(tid);
    if (!t) { json_free(args); char err[256]; snprintf(err,sizeof(err),"{\"error\":\"task %s not found\"}",tid); return strdup(err); }
    kanban_heartbeat(tid);
    json_free(t);
    json_free(args);
    return strdup("{\"ok\":true}");
}

static char *handle_comment(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");
    const char *tid = default_task_id(json_get_str(args, "task_id", ""));
    const char *body = json_get_str(args, "body", "");
    if (!tid || !*tid) { json_free(args); return strdup("{\"error\":\"task_id is required\"}"); }
    if (!*body) { json_free(args); return strdup("{\"error\":\"body is required\"}"); }
    bool ok = kanban_add_comment(tid, "worker", body);
    json_free(args);
    if (!ok) { char err[256]; snprintf(err,sizeof(err),"{\"error\":\"task %s not found\"}",tid); return strdup(err); }
    return strdup("{\"ok\":true}");
}

static char *handle_create(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!kanban_orchestrator())
        return strdup("{\"error\":\"Kanban orchestration blocked: sub-agent scope (HERMES_KANBAN_TASK set)\"}");
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");
    kanban_task_spec_t spec;
    memset(&spec, 0, sizeof(spec));
    spec.title = json_get_str(args, "title", "");
    spec.assignee = json_get_str(args, "assignee", "");
    spec.body = json_get_str(args, "body", "");
    spec.priority = (int)json_get_num(args, "priority", 0);
    spec.tenant = json_get_str(args, "tenant", "");
    spec.workspace_kind = json_get_str(args, "workspace_kind", "");
    spec.workspace_path = json_get_str(args, "workspace_path", "");
    spec.triage = json_get_bool(args, "triage", false);
    spec.initial_status = json_get_str(args, "initial_status", "");
    spec.created_by = getenv("HERMES_PROFILE");
    if (spec.created_by) spec.created_by = "worker";
    if (!*spec.title || !*spec.assignee) {
        json_free(args);
        return strdup("{\"error\":\"title and assignee are required\"}");
    }
    char *new_id = kanban_create_task(&spec);
    json_free(args);
    if (!new_id) return strdup("{\"error\":\"create failed\"}");
    char out[256];
    snprintf(out, sizeof(out),
             "{\"ok\":true,\"task_id\":\"%s\",\"status\":\"%s\"}",
             new_id, spec.initial_status && *spec.initial_status ? spec.initial_status
                      : (spec.triage ? "triage" : "running"));
    free(new_id);
    return strdup(out);
}

static char *handle_unblock(const char *args_json, const char *task_id) {
    (void)task_id;
    const char *guard = require_orchestrator("kanban_unblock");
    if (guard) return strdup("{\"error\":\"orchestrator-only\"}");
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");
    const char *tid = json_get_str(args, "task_id", "");
    if (!*tid) { json_free(args); return strdup("{\"error\":\"task_id is required\"}"); }
    bool ok = kanban_unblock_task( tid) ? true : false;
    json_free(args);
    if (!ok) { char err[256]; snprintf(err,sizeof(err),"{\"error\":\"task %s not found\"}",tid); return strdup(err); }
    char out[256];
    snprintf(out, sizeof(out), "{\"ok\":true,\"task_id\":\"%s\",\"status\":\"ready\"}", tid);
    return strdup(out);
}

static char *handle_link(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");
    const char *parent_id = json_get_str(args, "parent_id", "");
    const char *child_id  = json_get_str(args, "child_id", "");
    if (!*parent_id || !*child_id) {
        json_free(args);
        return strdup("{\"error\":\"both parent_id and child_id are required\"}");
    }
    if (strcmp(parent_id, child_id) == 0) {
        json_free(args); return strdup("{\"error\":\"self-links are not allowed\"}");
    }
    bool ok = kanban_link_tasks(parent_id, child_id);
    json_free(args);
    if (!ok) {
        json_t *p = kanban_read_task(parent_id);
        json_t *c = kanban_read_task(child_id);
        bool missing = (!p || !c);
        json_free(p); json_free(c);
        if (missing) {
            char err[256];
            snprintf(err,sizeof(err),"{\"error\":\"task %s not found\"}", !p?parent_id:child_id);
            return strdup(err);
        }
        return strdup("{\"error\":\"cycle detected\"}");
    }
    char out[256];
    snprintf(out, sizeof(out),
             "{\"ok\":true,\"parent_id\":\"%s\",\"child_id\":\"%s\"}", parent_id, child_id);
    return strdup(out);
}

/* ---- registration ---- */

void registry_init_kanban(void) {
    registry_register("kanban_show",
        "Read a task's full state — title, body, assignee, parent task handoffs, your prior attempts on this task if any, comments, and recent events. Use this to (re)orient yourself before starting work, especially on retries.",
        "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"string\",\"description\":\"Task id. If omitted, defaults to HERMES_KANBAN_TASK from the env.\"}},\"required\":[]}",
        handle_show);
    registry_register("kanban_list",
        "List Kanban task summaries so an orchestrator profile can discover work to route. Supports assignee, status, tenant, include_archived, and limit filters. Orchestrator-only.",
        "{\"type\":\"object\",\"properties\":{\"assignee\":{\"type\":\"string\"},\"status\":{\"type\":\"string\",\"enum\":[\"triage\",\"todo\",\"ready\",\"running\",\"blocked\",\"done\",\"archived\"]},\"tenant\":{\"type\":\"string\"},\"include_archived\":{\"type\":\"boolean\"},\"limit\":{\"type\":\"integer\"}},\"required\":[]}",
        handle_list);
    registry_register("kanban_complete",
        "Mark your current task done with a structured handoff. At least one of summary or result is required.",
        "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"string\"},\"summary\":{\"type\":\"string\"},\"result\":{\"type\":\"string\"},\"metadata\":{\"type\":\"object\"},\"created_cards\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}},\"artifacts\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}}},\"required\":[]}",
        handle_complete);
    registry_register("kanban_block",
        "Mark your current task as blocked. Provide a reason. Use sticky=true if this should survive auto-promotion.",
        "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"string\"},\"reason\":{\"type\":\"string\"},\"sticky\":{\"type\":\"boolean\"}},\"required\":[]}",
        handle_block);
    registry_register("kanban_heartbeat",
        "Extend the liveness window for this task.",
        "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"string\"}},\"required\":[]}",
        handle_heartbeat);
    registry_register("kanban_comment",
        "Add a structured comment to a Kanban task.",
        "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"string\"},\"body\":{\"type\":\"string\"}},\"required\":[\"body\"]}",
        handle_comment);
    registry_register("kanban_create",
        "Create a new Kanban task. title and assignee are required.",
        "{\"type\":\"object\",\"properties\":{\"title\":{\"type\":\"string\"},\"assignee\":{\"type\":\"string\"},\"body\":{\"type\":\"string\"},\"priority\":{\"type\":\"integer\"},\"tenant\":{\"type\":\"string\"},\"workspace_kind\":{\"type\":\"string\",\"enum\":[\"scratch\",\"dir\",\"worktree\"]},\"workspace_path\":{\"type\":\"string\"},\"triage\":{\"type\":\"boolean\"},\"initial_status\":{\"type\":\"string\",\"enum\":[\"running\",\"blocked\"]}},\"required\":[\"title\",\"assignee\"]}",
        handle_create);
    registry_register("kanban_unblock",
        "Move a blocked Kanban task back to ready. Orchestrator-only.",
        "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"string\"}},\"required\":[\"task_id\"]}",
        handle_unblock);
    registry_register("kanban_link",
        "Add a parent->child dependency edge. Cycles and self-links rejected.",
        "{\"type\":\"object\",\"properties\":{\"parent_id\":{\"type\":\"string\"},\"child_id\":{\"type\":\"string\"}},\"required\":[\"parent_id\",\"child_id\"]}",
        handle_link);
}
