/**
 * port_kanban_tools.c — Port of Python: tools/kanban_tools.py
 *
 * Real C implementations for Kanban board tools.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

/* ================================================================
 *  Profile / gating helpers
 * ================================================================ */

/* PoP: profile_has_kanban_toolset @ tools/kanban_tools.py:_profile_has_kanban_toolset
 * Port of Python tools/kanban_tools.py:_profile_has_kanban_toolset().
 * Checks if the current profile has "kanban" in its toolsets config.
 * Returns true if the kanban toolset is enabled. */
bool profile_has_kanban_toolset(void)
{
    /* In C we don't have load_config() exposed; check env as proxy.
     * The Python check reads cfg.get("toolsets", []) and checks for "kanban".
     * For parity, we check if the profile was loaded with kanban toolset. */
    const char *toolsets = getenv("HERMES_PROFILE_TOOLSETS");
    if (!toolsets) return false;
    return strstr(toolsets, "kanban") != NULL;
}

/* PoP: check_kanban_mode @ tools/kanban_tools.py:_check_kanban_mode
 * Port of Python tools/kanban_tools.py:_check_kanban_mode().
 * True when: HERMES_KANBAN_TASK is set (dispatcher worker) OR
 * profile has kanban toolset (orchestrator). */
bool check_kanban_mode(void)
{
    if (getenv("HERMES_KANBAN_TASK")) return true;
    return profile_has_kanban_toolset();
}

/* PoP: check_kanban_orchestrator_mode @ tools/kanban_tools.py:_check_kanban_orchestrator_mode
 * Port of Python tools/kanban_tools.py:_check_kanban_orchestrator_mode().
 * Board-routing tools (list/unblock) are hidden from task workers.
 * Returns true only for orchestrator profiles (kanban toolset, no task env). */
bool check_kanban_orchestrator_mode(void)
{
    if (getenv("HERMES_KANBAN_TASK")) return false;
    return profile_has_kanban_toolset();
}

/* ================================================================
 *  Worker identity / task ownership
 * ================================================================ */

/* PoP: worker_run_id @ tools/kanban_tools.py:_worker_run_id
 * Port of Python tools/kanban_tools.py:_worker_run_id().
 * Returns the dispatcher run id when this worker is scoped to task_id.
 * Returns NULL (0) if not a dispatcher-spawned worker for this task. */
int worker_run_id(const char *task_id)
{
    if (!task_id) return 0;
    const char *env_tid = getenv("HERMES_KANBAN_TASK");
    if (!env_tid || strcmp(env_tid, task_id) != 0) return 0;
    const char *raw = getenv("HERMES_KANBAN_RUN_ID");
    if (!raw) return 0;
    char *endptr;
    long val = strtol(raw, &endptr, 10);
    if (endptr == raw || *endptr != '\0') return 0;
    return (int)val;
}

/* PoP: stamp_worker_session_metadata @ tools/kanban_tools.py:_stamp_worker_session_metadata
 * Port of Python tools/kanban_tools.py:_stamp_worker_session_metadata().
 * Adds trusted worker session id metadata for this worker's own task.
 * Returns json_t* object (caller owns) or NULL if not a dispatcher worker. */
json_t *stamp_worker_session_metadata(const char *task_id, const json_t *metadata)
{
    if (!task_id) return NULL;
    const char *env_tid = getenv("HERMES_KANBAN_TASK");
    if (!env_tid || strcmp(env_tid, task_id) != 0) return NULL;
    const char *session_id = getenv("HERMES_SESSION_ID");
    if (!session_id) return NULL;

    json_t *stamped = json_copy((json_t *)metadata);
    if (!stamped) stamped = json_object();
    json_set(stamped, "worker_session_id", json_string(session_id));
    return stamped;
}

/* PoP: enforce_worker_task_ownership @ tools/kanban_tools.py:_enforce_worker_task_ownership
 * Port of Python tools/kanban_tools.py:_enforce_worker_task_ownership().
 * Rejects worker-driven destructive calls on foreign task IDs.
 * Returns malloc'd error string on rejection, NULL when allowed. */
char *enforce_worker_task_ownership(const char *tid)
{
    if (!tid) return NULL;
    const char *env_tid = getenv("HERMES_KANBAN_TASK");
    if (!env_tid) return NULL;  /* Orchestrator or CLI context — no restriction */
    if (strcmp(tid, env_tid) != 0) {
        char *err = malloc(512);
        if (err) {
            snprintf(err, 512,
                "worker is scoped to task %s; refusing to mutate %s. "
                "Use kanban_comment to hand off information to other tasks, "
                "or kanban_create to spawn follow-up work.", env_tid, tid);
        }
        return err;
    }
    return NULL;
}

/* ================================================================
 *  Goal judge availability
 * ================================================================ */

/* PoP: goal_judge_available @ tools/kanban_tools.py:_goal_judge_available
 * Port of Python tools/kanban_tools.py:_goal_judge_available().
 * True when an auxiliary client is configured for the goal judge.
 * In C, we check if the auxiliary client is available via env proxy. */
bool goal_judge_available(void)
{
    /* Python checks agent.auxiliary_client.get_text_auxiliary_client("goal_judge").
     * C proxy: check if aux client was configured at startup. */
    const char *aux = getenv("HERMES_AUX_GOAL_JUDGE");
    return aux && *aux;
}

/* ================================================================
 *  Auto-heartbeat bridge
 * ================================================================ */

static double s_auto_heartbeat_last = 0.0;
#define AUTO_HEARTBEAT_MIN_INTERVAL 60.0

/* PoP: heartbeat_current_worker_from_env @ tools/kanban_tools.py:heartbeat_current_worker_from_env
 * Port of Python tools/kanban_tools.py:heartbeat_current_worker_from_env().
 * Best-effort: extend kanban claim + bump board heartbeat for current
 * dispatcher-spawned worker, using identity from env vars.
 * Rate-limited to one DB write per 60s per-process. Returns true if attempted. */
bool heartbeat_current_worker_from_env(void)
{
    const char *tid = getenv("HERMES_KANBAN_TASK");
    if (!tid) return false;

    /* Rate limit: monotonic time check */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double now = ts.tv_sec + ts.tv_nsec / 1e9;
    if (now - s_auto_heartbeat_last < AUTO_HEARTBEAT_MIN_INTERVAL) return false;
    s_auto_heartbeat_last = now;

    /* Real auto-heartbeat opens the kanban store and calls heartbeat_worker.
     * That DB write is not wired into this C port, so we cannot claim an
     * attempt was made. Report honestly (return false) rather than pretending
     * an attempt happened. */
    hermes_log(LOG_DEBUG, "kanban",
        "auto-heartbeat: task=%s run_id=%s claim_lock=%s — kanban DB write not wired in C port; skipping",
        tid,
        getenv("HERMES_KANBAN_RUN_ID") ? getenv("HERMES_KANBAN_RUN_ID") : "(none)",
        getenv("HERMES_KANBAN_CLAIM_LOCK") ? getenv("HERMES_KANBAN_CLAIM_LOCK") : "(default)");

    return false;
}

/* ================================================================
 *  Response / arg helpers
 * ================================================================ */

/* PoP: ok @ tools/kanban_tools.py:_ok
 * Port of Python tools/kanban_tools.py:_ok().
 * Builds a JSON success response: {"ok": true, **fields}.
 * Mirrors hermes_ok in hermes_error.h but with extra fields. */
json_t *ok(const json_t *fields)
{
    json_t *obj = json_object();
    json_set(obj, "ok", json_bool(true));
    if (fields && fields->type == JSON_OBJECT) {
        /* libjson has no json_obj_keys; iterate known keys from fields.
         * Since we control the callers, we know the fields are flat. */
        const char *known_keys[] = {
            "id", "task_id", "title", "status", "message", "note", "worker_id",
            "run_id", "claim_lock", "heartbeat_at", "created_at", "updated_at",
            "parent", "child", "assignee", "priority", "tenant", "workspace_kind",
            "workspace_path", "created_by", "sticky_blocked", "error", NULL
        };
        for (size_t i = 0; known_keys[i]; i++) {
            const char *k = known_keys[i];
            json_t *v = json_obj_get(fields, k);
            if (v) json_set(obj, k, json_copy(v));
        }
    }
    return obj;
}

/* PoP: normalize_profile @ tools/kanban_tools.py:_normalize_profile
 * Port of Python tools/kanban_tools.py:_normalize_profile().
 * Normalizes CLI-compatible assignee sentinels for the tool surface.
 * Returns malloc'd string (caller frees) or NULL for none/null/empty. */
char *normalize_profile(const char *value)
{
    if (!value) return NULL;
    const char *p = value;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return NULL;
    char *lower = strdup(p);
    if (!lower) return NULL;
    for (char *c = lower; *c; c++) *c = tolower((unsigned char)*c);
    if (strcmp(lower, "none") == 0 || strcmp(lower, "-") == 0 || strcmp(lower, "null") == 0) {
        free(lower);
        return NULL;
    }
    /* Trim trailing whitespace */
    char *end = lower + strlen(lower) - 1;
    while (end > lower && isspace((unsigned char)*end)) *end-- = '\0';
    return lower;
}

/* PoP: parse_bool_arg @ tools/kanban_tools.py:_parse_bool_arg
 * Port of Python tools/kanban_tools.py:_parse_bool_arg().
 * Parses a string arg as boolean: "1"/"true"/"yes"/"on" -> true,
 * "0"/"false"/"no"/"off" -> false, empty/NULL -> default_val. */
bool parse_bool_arg(const char *arg, bool default_val)
{
    if (!arg || !*arg) return default_val;
    const char *p = arg;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return default_val;
    char buf[32];
    size_t n = strlen(p);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    for (size_t i = 0; i < n; i++) buf[i] = tolower((unsigned char)p[i]);
    buf[n] = '\0';
    if (strcmp(buf, "1") == 0 || strcmp(buf, "true") == 0 ||
        strcmp(buf, "yes") == 0 || strcmp(buf, "on") == 0) return true;
    if (strcmp(buf, "0") == 0 || strcmp(buf, "false") == 0 ||
        strcmp(buf, "no") == 0 || strcmp(buf, "off") == 0) return false;
    return default_val;
}

/* PoP: require_orchestrator_tool @ tools/kanban_tools.py:_require_orchestrator_tool
 * Port of Python tools/kanban_tools.py:_require_orchestrator_tool().
 * Returns error string (caller frees) if called from worker context (has HERMES_KANBAN_TASK).
 * Returns NULL when called from orchestrator/CLI context. */
char *require_orchestrator_tool(const char *tool_name)
{
    if (getenv("HERMES_KANBAN_TASK")) {
        char *err = malloc(256);
        if (err) snprintf(err, 256, "%s is orchestrator-only", tool_name);
        return err;
    }
    return NULL;
}

/* PoP: task_summary_dict @ tools/kanban_tools.py:_task_summary_dict
 * Port of Python tools/kanban_tools.py:_task_summary_dict().
 * Builds a summary JSON object from a task JSON.
 * Returns json_t* object with id, title, status, assignee, priority, etc. */
json_t *task_summary_dict(const json_t *task)
{
    if (!task || task->type != JSON_OBJECT) return json_object();
    json_t *obj = json_object();
    json_t *id = json_obj_get(task, "id");
    json_t *title = json_obj_get(task, "title");
    json_t *status = json_obj_get(task, "status");
    json_t *assignee = json_obj_get(task, "assignee");
    json_t *priority = json_obj_get(task, "priority");
    json_t *created = json_obj_get(task, "created_at");
    json_t *updated = json_obj_get(task, "updated_at");

    if (id) json_set(obj, "id", json_copy(id));
    if (title) json_set(obj, "title", json_copy(title));
    if (status) json_set(obj, "status", json_copy(status));
    if (assignee) json_set(obj, "assignee", json_copy(assignee));
    if (priority) json_set(obj, "priority", json_copy(priority));
    if (created) json_set(obj, "created_at", json_copy(created));
    if (updated) json_set(obj, "updated_at", json_copy(updated));
    return obj;
}

/* ================================================================
 *  Original stubs (kept for backward compat)
 * ================================================================ */

/* Port of Python: _board_schema_prop */
char *board_schema_prop(void)
{
    const char *schema = "{\"type\": \"object\", \"properties\": {\"title\": {\"type\": \"string\"}, \"status\": {\"type\": \"string\"}, \"priority\": {\"type\": \"number\"}}}";
    char *result = strdup(schema);
    hermes_log(LOG_DEBUG, "port", "board_schema_prop: returned schema");
    return result;
}

/* Port of Python: _maybe_auto_subscribe */
bool maybe_auto_subscribe(const char *conn, const char *task_id)
{
    if (!conn) {
        hermes_log(LOG_WARNING, "port", "maybe_auto_subscribe: null conn");
        return false;
    }
    if (strstr(conn, "subscribe") || strstr(conn, "auto")) {
        hermes_log(LOG_INFO, "port", "maybe_auto_subscribe: task=%s", task_id ? task_id : "(none)");
        return true;
    }
    return false;
}

/* ================================================================
 *  _default_task_id
 * ================================================================ */
/* PoP: default_task_id @ tools/kanban_tools.py:_default_task_id */
/* Resolve `task_id` arg or fall back to the env var the dispatcher set
 * (HERMES_KANBAN_TASK). Returns a malloc'd string (caller frees) or NULL. */
char *default_task_id(const char *arg)
{
    if (arg && arg[0]) return strdup(arg);
    const char *env_tid = getenv("HERMES_KANBAN_TASK");
    if (env_tid && env_tid[0]) return strdup(env_tid);
    return NULL;
}