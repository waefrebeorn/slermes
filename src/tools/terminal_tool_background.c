/*
 * terminal_tool_background.c - C11 port of upstream
 * tools/terminal_tool_background.py (gate w23_terminalto_ce074e).
 *
 * PoP: w23_terminalto_ce074e_spawn @ tools/terminal_tool_background.py:spawn_background_process
 * PoP: w23_terminalto_ce074e_async_support @ tools/terminal_tool_background.py:_apply_async_support
 * PoP: w23_terminalto_ce074e_watcher @ tools/terminal_tool_background.py:_register_completion_watcher
 * PoP: w23_terminalto_ce074e_routing @ tools/terminal_tool_background.py:_stamp_gateway_routing
 * PoP: w23_terminalto_ce074e_ci_poller @ tools/terminal_tool_background.py:_looks_like_homebrew_ci_poller
 * PoP: w23_terminalto_ce074e_yield @ tools/terminal_tool_background.py:yield_to_background_handler
 *
 * The registry, the cwd resolver, the conflict resolver, the redactor, the
 * gateway/branch predicates and the runner are seams: the fixture supplies
 * their answers as facts.  This file owns the decision ladder over those
 * facts, the exact result keys, and the note texts verbatim.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_terminalto_ce074e.h"

/* -- note texts, byte-identical to the module constants ----------------- */

static const char TTB_SILENT_HINT[] =
    "background=true without notify_on_complete=true means this process runs SILENTLY \xE2\x80\x94"
    " you will not be told when it exits. If this is a bounded task (test suite, build, CI polle"
    "r, deploy, anything with a defined end), you almost certainly wanted notify_on_complete=tru"
    "e so the system pings you on exit. Re-launch with notify_on_complete=true, or call process_"
    "manage(action='poll') / process_manage(action='wait') yourself to learn the outcome. Only i"
    "gnore this hint for genuine long-lived processes that never exit (servers, watchers, daemon"
    "s).";

static const char TTB_CI_HINT[] =
    "This looks like a homebrewed CI poller built from `gh pr view --json statusCheckRollup` and"
    "/or `gh pr checks | jq`. That shape has burned us repeatedly in hermes-agent dev work (PRs "
    "#31329, #31448, #31695, #31709, #31745, #32264, #33131) \xE2\x80\x94"
    " stdout buffering kills output capture, jq null-key edge cases silently exit the loop, conc"
    "lusion-vs-status field confusion exits early with bogus all-green verdicts, TTY-only summar"
    "y banners never appear when piped. Use the canonical snippets in the green-ci-policy skill "
    "instead: the exit-code-driven `gh pr checks $PR >/dev/null` (rc 0 = green, 8 = pending, els"
    "e fail) for exit-on-first-fail behavior, or the column-2 awk-on-tabs poller (`awk -F\"\\t\""
    " \"$2==\\\"pending\\\"\"`) for sharded matrices. Load skill_view(name='github/hermes-agent-"
    "dev', file_path='references/green-ci-policy.md') for the verbatim snippets. If you must rol"
    "l a custom loop with rich structured output, write each tick to a known file (`tee -a $TMPD"
    "IR/ci.log`) and rely on `process_manage(action='log')` to read THAT file \xE2\x80\x94"
    " do not rely on background-process stdout capture for line-buffered shell loops.";

static const char TTB_AUTO_BG_NOTE[] =
    "Ran in the background (the trailing & was converted to background=true; Hermes tracks the p"
    "rocess). Do NOT re-run it. Use process_manage (poll/log/wait/kill, session_id above) to che"
    "ck it.";

static const char TTB_ASYNC_UNSUPPORTED[] =
    "notify_on_complete / watch_patterns are not available in this session \xE2\x80\x94"
    " it cannot receive an async completion after the turn ends (a one-shot runner such as `herm"
    "es -z`, a /bg background task, a cron job, a Kanban worker, a hosted agent, or a stateless "
    "HTTP endpoint). The process is running in the background; retrieve its result with process_"
    "manage(action='poll') or process_manage(action='wait').";

static const char TTB_SUBAGENT_NOTE[] =
    "You are a subagent: this process's completion notice will NOT reach your parent, and the pr"
    "ocess is killed when you finish. Before you finish, either wait for it (process_manage wait"
    "), kill it, or hand it to your parent with process_manage(action='handoff', session_id=...,"
    " data='<purpose>') so the parent receives its completion. For CI watchers prefer returning "
    "the fact (PR number, SHA) and letting the parent watch.";

static const char TTB_HEARTBEAT_IGNORED[] =
    "heartbeat needs notify=true delivery, which this session cannot receive";

static const char TTB_SPAWNED[] = "Background process started";

/* _ROUTING_FIELDS as proc attributes: the watcher payload's middle keys and
 * exactly the attributes _stamp_gateway_routing sets. */
static const char *const TTB_ROUTE_ATTR[6] = {
    "watcher_chat_id", "watcher_user_id", "watcher_user_name",
    "watcher_thread_id", "watcher_message_id", "parent_session_id"
};
/* The request key each env var's value is read from in the routing fixture. */
static const char *const TTB_ROUTE_REQ[7] = {
    "platform", "chat_id", "user_id", "user_name", "thread_id", "message_id", "session_id"
};

static int ttb_truthy(const json_t *v)
{
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return json_len(v) > 0;
        case JSON_OBJECT: return json_object_size(v) > 0;
        default: return 0;
    }
}

static int ttb_has(const json_t *obj, const char *key)
{
    return obj && obj->type == JSON_OBJECT && json_obj_get(obj, key) != NULL;
}

/* A copy of obj[key], or JSON null when the key is absent (json_set ignores a
 * NULL value, so a missing fact must be materialised explicitly). */
static json_t *ttb_copy_or_null(const json_t *obj, const char *key)
{
    json_t *v = obj ? json_obj_get(obj, key) : NULL;
    return v ? json_copy(v) : json_null();
}

/* _looks_like_homebrew_ci_poller. */
static int ttb_is_ci_poller(const char *command)
{
    int has_gh, has_jq;
    if (!command) return 0;
    has_gh = strstr(command, "gh pr view") != NULL || strstr(command, "gh pr checks") != NULL;
    has_jq = strstr(command, " jq ") != NULL || strstr(command, "| jq") != NULL ||
             strstr(command, "$(jq") != NULL;
    return strstr(command, "statusCheckRollup") != NULL || (has_gh && has_jq);
}

json_t *w23_terminalto_ce074e_ci_poller(const char *command)
{
    return json_bool(ttb_is_ci_poller(command) ? true : false);
}

/* The stamped payload: watcher_platform plus every routing attribute. */
static json_t *ttb_stamp_from(const json_t *src, const char *const *keys)
{
    json_t *o = json_object();
    const char *platform = json_get_str(src, keys[0], "");
    int i;
    if (!platform) platform = "";
    json_set(o, "watcher_platform", json_string(platform));
    for (i = 0; i < 6; i++) {
        const char *v = json_get_str(src, keys[i + 1], "");
        json_set(o, TTB_ROUTE_ATTR[i], json_string(v ? v : ""));
    }
    return o;
}

json_t *w23_terminalto_ce074e_routing(const json_t *req)
{
    const char *platform;
    if (!req || req->type != JSON_OBJECT) return json_null();
    platform = json_get_str(req, "platform", "");
    if (!platform || platform[0] == '\0') return json_null();
    return ttb_stamp_from(req, TTB_ROUTE_REQ);
}

/* proc.facts(): only the routing attributes the session actually carries. */
static json_t *ttb_world_facts(const json_t *world)
{
    json_t *o = json_object();
    int i;
    if (ttb_has(world, "watcher_platform"))
        json_set(o, "watcher_platform", ttb_copy_or_null(world, "watcher_platform"));
    for (i = 0; i < 6; i++)
        if (ttb_has(world, TTB_ROUTE_ATTR[i]))
            json_set(o, TTB_ROUTE_ATTR[i], json_copy(json_obj_get(world, TTB_ROUTE_ATTR[i])));
    if (json_object_size(o) == 0) { json_free(o); return json_null(); }
    return o;
}

json_t *w23_terminalto_ce074e_async_support(const json_t *req)
{
    json_t *world = req ? json_obj_get(req, "world") : NULL;
    json_t *result = json_object();
    json_t *out = json_object();
    json_t *watch = req ? json_obj_get(req, "watch_patterns") : NULL;
    json_t *routing;
    int notify = req ? ttb_truthy(json_obj_get(req, "notify_on_complete")) : 0;

    if (notify || ttb_truthy(watch)) {
        if (ttb_truthy(json_obj_get(world, "async_supported")) &&
            !ttb_truthy(json_obj_get(world, "hosted"))) {
            static const char *const keys[7] = {
                "watcher_platform", "watcher_chat_id", "watcher_user_id",
                "watcher_user_name", "watcher_thread_id", "watcher_message_id", "parent_session_id"
            };
            /* _stamp_gateway_routing sets every field, so all of them exist. */
            routing = ttb_stamp_from(world, keys);
        } else {
            json_set(result, "notify_on_complete", json_bool(false));
            json_set(result, "notify_unsupported", json_string(TTB_ASYNC_UNSUPPORTED));
            notify = 0;
            watch = NULL;
            routing = ttb_world_facts(world);
        }
    } else {
        routing = ttb_world_facts(world);
    }
    json_set(out, "notify", json_bool(notify ? true : false));
    json_set(out, "watch", (watch && watch->type == JSON_ARRAY) ? json_copy(watch) : json_null());
    json_set(out, "result", result);
    json_set(out, "routing", routing);
    return out;
}

/* The watcher descriptor _register_completion_watcher builds. */
static json_t *ttb_watcher(const json_t *world, const char *session_key)
{
    json_t *w = json_object();
    json_set(w, "session_id", ttb_copy_or_null(world, "proc_id"));
    json_set(w, "check_interval", json_number(5));
    json_set(w, "session_key", json_string(session_key ? session_key : ""));
    json_set(w, "platform", ttb_copy_or_null(world, "watcher_platform"));
    json_set(w, "chat_id", ttb_copy_or_null(world, "watcher_chat_id"));
    json_set(w, "user_id", ttb_copy_or_null(world, "watcher_user_id"));
    json_set(w, "user_name", ttb_copy_or_null(world, "watcher_user_name"));
    json_set(w, "thread_id", ttb_copy_or_null(world, "watcher_thread_id"));
    json_set(w, "message_id", ttb_copy_or_null(world, "watcher_message_id"));
    json_set(w, "notify_on_complete", json_bool(true));
    json_set(w, "parent_session_id", ttb_copy_or_null(world, "parent_session_id"));
    return w;
}

json_t *w23_terminalto_ce074e_watcher(const json_t *req)
{
    json_t *world = req ? json_obj_get(req, "world") : NULL;
    json_t *out = json_object();
    json_t *w = ttb_watcher(world, req ? json_get_str(req, "session_key", "") : "");
    int armed = req ? ttb_truthy(json_obj_get(req, "armed")) : 0;

    json_set(out, "watcher", w);
    json_set(out, "armed", json_bool(armed ? true : false));
    json_set(out, "pending", json_number(armed ? 0 : 1));
    json_set(out, "interval", json_number(5));
    return out;
}

json_t *w23_terminalto_ce074e_spawn(const json_t *req)
{
    json_t *world = req ? json_obj_get(req, "world") : NULL;
    const char *command = req ? json_get_str(req, "command", NULL) : NULL;
    const char *effective_cwd = world ? json_get_str(world, "effective_cwd", NULL) : NULL;
    const char *spawn_error = world ? json_get_str(world, "spawn_error", NULL) : NULL;
    const char *approval = req ? json_get_str(req, "approval_note", NULL) : NULL;
    const char *pty_reason = req ? json_get_str(req, "pty_disabled_reason", NULL) : NULL;
    json_t *watch = req ? json_obj_get(req, "watch_patterns") : NULL;
    json_t *out;
    int notify = req ? ttb_truthy(json_obj_get(req, "notify_on_complete")) : 0;
    int heartbeat = req ? (int)json_get_num(req, "heartbeat_seconds", 0.0) : 0;

    if (spawn_error && spawn_error[0]) {
        size_t n = strlen("Failed to start background process: ") + strlen(spawn_error) + 1;
        char *msg = (char *)malloc(n);
        out = json_object();
        if (msg) {
            snprintf(msg, n, "Failed to start background process: %s", spawn_error);
            json_set(out, "error", json_string(msg));
            free(msg);
        } else {
            json_set(out, "error", json_null());
        }
        json_set(out, "output", json_string(""));
        json_set(out, "exit_code", json_number(-1));
        return out;
    }

    out = json_object();
    json_set(out, "output", json_string(TTB_SPAWNED));
    json_set(out, "session_id", ttb_copy_or_null(world, "proc_id"));
    json_set(out, "pid", json_copy(json_obj_get(world, "proc_pid")));
    json_set(out, "exit_code", json_number(0));
    json_set(out, "error", json_null());

    if (req && ttb_truthy(json_obj_get(req, "persist_on_release")))
        json_set(out, "persist_on_release", json_bool(true));

    if (effective_cwd && effective_cwd[0]) {
        const char *used = json_get_str(world, "used_cwd", NULL);
        const char *msys = json_get_str(world, "msys_path", NULL);
        if (!msys) msys = effective_cwd;
        if (used && used[0] && strcmp(used, effective_cwd) != 0 && strcmp(used, msys) != 0) {
            json_t *note = json_obj_get(world, "cwd_fallback_note");
            json_set(out, "cwd_fallback", note ? json_copy(note) : json_string(""));
        }
    }
    if (approval && approval[0]) json_set(out, "approval", json_string(approval));
    if (pty_reason && pty_reason[0]) json_set(out, "pty_note", json_string(pty_reason));

    if (req && ttb_truthy(json_obj_get(req, "auto_backgrounded"))) {
        json_set(out, "auto_backgrounded", json_string(TTB_AUTO_BG_NOTE));
    } else if (!notify && !ttb_truthy(watch)) {
        json_set(out, "hint", json_string(TTB_SILENT_HINT));
    }

    if (command && ttb_is_ci_poller(command)) {
        json_t *existing = json_obj_get(out, "hint");
        if (existing && existing->type == JSON_STRING && existing->str_val && existing->str_val[0]) {
            size_t n = strlen(existing->str_val) + strlen(TTB_CI_HINT) + 3;
            char *merged = (char *)malloc(n);
            if (merged) {
                snprintf(merged, n, "%s\n\n%s", existing->str_val, TTB_CI_HINT);
                json_set(out, "hint", json_string(merged));
                free(merged);
            }
        } else {
            json_set(out, "hint", json_string(TTB_CI_HINT));
        }
    }

    /* _apply_async_support: a session that cannot receive an async delivery
     * loses the flags and is told to poll instead. */
    if ((notify || ttb_truthy(watch)) &&
        !(ttb_truthy(json_obj_get(world, "async_supported")) &&
          !ttb_truthy(json_obj_get(world, "hosted")))) {
        json_set(out, "notify_on_complete", json_bool(false));
        json_set(out, "notify_unsupported", json_string(TTB_ASYNC_UNSUPPORTED));
        notify = 0;
        watch = NULL;
    }

    /* _resolve_notification_flag_conflict: the seam may hand back a different
     * pattern list plus the note explaining why the flags collide. */
    if (world && ttb_has(world, "conflict_watch")) watch = json_obj_get(world, "conflict_watch");
    if (world) {
        const char *note = json_get_str(world, "conflict_note", NULL);
        if (note && note[0]) json_set(out, "watch_patterns_ignored", json_string(note));
    }

    if (notify) {
        json_set(out, "notify_on_complete", json_bool(true));
        if (world && ttb_truthy(json_obj_get(world, "delegated"))) {
            json_set(out, "notify_on_complete", json_bool(false));
            json_set(out, "subagent_note", json_string(TTB_SUBAGENT_NOTE));
        } else if (heartbeat) {
            json_t *ret = world ? json_obj_get(world, "heartbeat_ret") : NULL;
            json_set(out, "heartbeat_seconds", ret ? json_copy(ret) : json_number(heartbeat));
        }
    } else if (heartbeat) {
        json_set(out, "heartbeat_ignored", json_string(TTB_HEARTBEAT_IGNORED));
    }
    if (ttb_truthy(watch)) json_set(out, "watch_patterns", json_copy(watch));
    return out;
}

json_t *w23_terminalto_ce074e_yield(const json_t *req)
{
    json_t *world = req ? json_obj_get(req, "world") : NULL;
    const char *env_type = req ? json_get_str(req, "env_type", "local") : "local";
    const char *session_key = req ? json_get_str(req, "session_key", "sk") : "sk";
    const char *effective_task_id = req ? json_get_str(req, "effective_task_id", "t0") : "t0";
    const char *task_id = req ? json_get_str(req, "task_id", NULL) : NULL;
    json_t *out, *adopted;

    if (!env_type || strcmp(env_type, "local") != 0) return json_null();

    adopted = json_object();
    json_set(adopted, "kind", json_string("adopt"));
    json_set(adopted, "command", json_string(req ? json_get_str(req, "command", "") : ""));
    json_set(adopted, "cwd", ttb_copy_or_null(req, "cwd"));
    json_set(adopted, "task_id", json_string(effective_task_id ? effective_task_id : ""));
    json_set(adopted, "owner_task_id",
             json_string((task_id && task_id[0]) ? task_id
                                                 : (effective_task_id ? effective_task_id : "")));
    json_set(adopted, "session_key", json_string(session_key ? session_key : ""));
    json_set(adopted, "output_so_far",
             json_string(req ? json_get_str(req, "output_so_far", "") : ""));

    out = json_object();
    json_set(out, "adopted", adopted);
    if (!ttb_truthy(json_obj_get(world, "async_supported"))) {
        /* No async delivery: the adopted session is told not to promise one. */
        json_set(out, "pending", json_number(0));
        json_set(out, "watcher", json_null());
        json_set(out, "notify_on_complete", json_bool(false));
        return out;
    }
    if (ttb_truthy(json_obj_get(world, "watcher_platform"))) {
        int armed = (!world || !ttb_has(world, "arm_watcher") ||
                     ttb_truthy(json_obj_get(world, "arm_watcher"))) ? 1 : 0;
        json_set(out, "pending", json_number(armed ? 0 : 1));
        json_set(out, "watcher", ttb_watcher(world, session_key));
    } else {
        json_set(out, "pending", json_number(0));
        json_set(out, "watcher", json_null());
    }
    json_set(out, "notify_on_complete", json_bool(true));
    return out;
}
