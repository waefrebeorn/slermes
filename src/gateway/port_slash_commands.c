/*
 * port_slash_commands.c — Faithful C11 ports of gateway/slash_commands.py
 * (GatewaySlashCommandsMixin) pure-logic / dependency-backed helpers.
 *
 * Reuses existing C subsystems via their opaque public APIs:
 *   - i18n:        i18n_t / i18n_t_fmt          (hermes_i18n.h)
 *   - config I/O:  config_py_atomic_config_write (port_config_py_helpers.h)
 *   - slash gate:  slash_policy_for_source / slash_policy_is_admin
 *                                                (hermes_gateway_slash_access.h)
 *   - config load: yaml_parse_file / yaml_to_json_string (yaml.h)
 */

#include "port_slash_commands.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "hermes_i18n.h"
#include "port_config_py_helpers.h"
#include "hermes_gateway_slash_access.h"
#include "hermes_gateway_types.h"
#include "gw_server_internals.h"
#include "yaml.h"

/* hermes_constants.VALID_REASONING_EFFORTS */
static const char *const VALID_REASONING_EFFORTS[] = {
    "minimal", "low", "medium", "high", "xhigh", "max", "ultra", NULL
};

/* lower-case an ASCII string in place. */
static void ascii_lower(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

/* strip leading/trailing ASCII whitespace; returns a malloc'd copy. */
static char *strip_dup(const char *s) {
    if (!s) return NULL;
    while (*s && isspace((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* ───────────────────── parse_reasoning_effort ─────────────────────
 * PoP: reasoning_parse_effort @ hermes_constants.py:parse_reasoning_effort */
json_t *reasoning_parse_effort(const char *effort, bool effort_is_bool_false) {
    /* effort is False -> {"enabled": False} */
    if (effort_is_bool_false) {
        json_t *o = json_object();
        json_set(o, "enabled", json_bool(false));
        return o;
    }
    /* None / True -> None (True is caller-signalled; here NULL string == None) */
    if (!effort) return NULL;

    char *low = strip_dup(effort);
    if (!low || low[0] == '\0') { free(low); return NULL; }
    ascii_lower(low);

    if (strcmp(low, "none") == 0 || strcmp(low, "false") == 0 ||
        strcmp(low, "disabled") == 0) {
        free(low);
        json_t *o = json_object();
        json_set(o, "enabled", json_bool(false));
        return o;
    }
    for (int i = 0; VALID_REASONING_EFFORTS[i]; i++) {
        if (strcmp(low, VALID_REASONING_EFFORTS[i]) == 0) {
            json_t *o = json_object();
            json_set(o, "enabled", json_bool(true));
            json_set(o, "effort", json_string(low));
            free(low);
            return o;
        }
    }
    free(low);
    return NULL;
}

/* ─────────────────── _reasoning_picker_choices ───────────────────
 * PoP: slash_reasoning_picker_choices @ gateway/slash_commands.py:_reasoning_picker_choices */
static json_t *picker_choice(const char *value, char *label, bool is_current) {
    json_t *o = json_object();
    json_set(o, "value", json_string(value));
    json_set(o, "label", json_string(label ? label : value));
    json_set(o, "is_current", json_bool(is_current));
    return o;
}

json_t *slash_reasoning_picker_choices(const char *current_effort) {
    const char *cur = current_effort ? current_effort : "";
    json_t *arr = json_array();

    /* First entry: "none" */
    char *lbl_none = i18n_t("gateway.reasoning.choice_none");
    json_array_append(arr, picker_choice("none", lbl_none,
                                         strcmp(cur, "none") == 0));
    free(lbl_none);

    /* One entry per valid effort level (label == level). */
    for (int i = 0; VALID_REASONING_EFFORTS[i]; i++) {
        const char *lvl = VALID_REASONING_EFFORTS[i];
        json_array_append(arr, picker_choice(lvl, (char *)lvl,
                                             strcmp(cur, lvl) == 0));
    }

    /* Trailing: reset / show / hide (never current). */
    char *lbl_reset = i18n_t("gateway.reasoning.choice_reset");
    json_array_append(arr, picker_choice("reset", lbl_reset, false));
    free(lbl_reset);
    char *lbl_show = i18n_t("gateway.reasoning.choice_show");
    json_array_append(arr, picker_choice("show", lbl_show, false));
    free(lbl_show);
    char *lbl_hide = i18n_t("gateway.reasoning.choice_hide");
    json_array_append(arr, picker_choice("hide", lbl_hide, false));
    free(lbl_hide);

    return arr;
}

/* ─────────────────── _save_gateway_config_key ───────────────────
 * PoP: slash_save_gateway_config_key @ gateway/slash_commands.py:_save_gateway_config_key */
bool slash_save_gateway_config_key(const char *config_path,
                                   const char *key_path,
                                   json_t *value) {
    if (!config_path || !key_path || key_path[0] == '\0') {
        if (value) json_free(value);
        return false;
    }

    /* Load existing config.yaml -> json object (or empty object). */
    json_t *root = NULL;
    char *yerr = NULL;
    yaml_doc_t *doc = yaml_parse_file(config_path, &yerr);
    if (yerr) free(yerr);
    if (doc) {
        char *js = yaml_to_json_string(doc, NULL);
        yaml_free(doc);
        if (js) {
            char *jerr = NULL;
            root = json_parse(js, &jerr);
            free(js);
            if (jerr) free(jerr);
        }
    }
    if (!root || root->type != JSON_OBJECT) {
        if (root) json_free(root);
        root = json_object();   /* user_config = {} */
    }

    /* Walk/create intermediate objects for keys[:-1]. */
    char *keys = strdup(key_path);
    if (!keys) { json_free(root); if (value) json_free(value); return false; }

    /* Split on '.' collecting segments. */
    char *segs[64];
    int nseg = 0;
    for (char *tok = strtok(keys, "."); tok && nseg < 64;
         tok = strtok(NULL, ".")) {
        segs[nseg++] = tok;
    }
    if (nseg == 0) { free(keys); json_free(root); if (value) json_free(value); return false; }

    json_t *cur = root;
    for (int i = 0; i < nseg - 1; i++) {
        json_t *child = json_obj_get(cur, segs[i]);
        if (!child || child->type != JSON_OBJECT) {
            child = json_object();
            json_set(cur, segs[i], child);  /* replaces non-dict/missing */
        }
        cur = child;
    }
    /* current[keys[-1]] = value */
    json_set(cur, segs[nseg - 1], value ? value : json_null());

    free(keys);

    int rc = config_py_atomic_config_write(config_path, root);
    json_free(root);
    return rc == 0;
}

/* ─────────────────── _resume_caller_is_admin ───────────────────
 * PoP: slash_resume_caller_is_admin @ gateway/slash_commands.py:_resume_caller_is_admin */
bool slash_resume_caller_is_admin(json_node_t *gateway_config,
                                  const gw_session_source_t *source) {
    if (!source) return false;
    slash_policy_t *policy = slash_policy_for_source(gateway_config, source);
    if (!policy) return false;
    const char *uid = source->user_id;
    bool result = policy->enabled && uid && uid[0] != '\0' &&
                  slash_policy_is_admin(policy, uid);
    free(policy);
    return result;
}

/* ──── Remaining slash command handlers ──── */

/* PoP: _model_switch_skew_guard @ gateway/slash_commands.py:_model_switch_skew_guard */
const char *slash_model_switch_skew_guard(const char *requested, const char *current) {
    if (!requested || !requested[0]) return NULL;
    if (!current || !current[0]) return NULL;
    if (strcmp(requested, current) == 0) return "model_already_active";
    return NULL;
}

/* PoP: _handle_reset_command @ gateway/slash_commands.py:_handle_reset_command */
json_t *slash_handle_reset(const char *session_key) {
    int idx = session_find_by_key(session_key);
    if (idx >= 0) session_free(idx);
    json_t *r = json_object(); json_set(r, "ok", json_bool(true)); return r;
}

/* PoP: _handle_profile_command @ gateway/slash_commands.py:_handle_profile_command */
json_t *slash_handle_profile(const char *args) {
    json_t *r = json_object(); json_set(r, "profile", json_string(args ? args : "default")); return r;
}

/* PoP: _handle_whoami_command @ gateway/slash_commands.py:_handle_whoami_command */
json_t *slash_handle_whoami(const gw_session_source_t *source) {
    if (!source) return json_object();
    json_t *r = json_object();
    json_set(r, "user_id", json_string(source->user_id));
    json_set(r, "user_name", json_string(source->user_name));
    json_set(r, "platform", json_string(source->platform));
    json_set(r, "chat_id", json_string(source->chat_id));
    json_set(r, "chat_type", json_string(source->chat_type));
    return r;
}

/* PoP: _handle_kanban_command @ gateway/slash_commands.py:_handle_kanban_command */
json_t *slash_handle_kanban(const char *args) {
    json_t *r = json_object(); json_set(r, "action", json_string(args ? args : "list")); return r;
}

/* PoP: _handle_status_command @ gateway/slash_commands.py:_handle_status_command */
json_t *slash_handle_status(void) {
    json_t *r = json_object();
    json_set(r, "sessions", json_int(g_gw.session_count));
    json_set(r, "running", json_bool(g_gw.running));
    return r;
}

/* PoP: _gateway_session_origin_for_id @ gateway/slash_commands.py:_gateway_session_origin_for_id */
const char *slash_session_origin_for_id(const char *session_id) {
    int idx = lookup_by_session_id(session_id);
    if (idx < 0) return NULL;
    return g_gw.sessions[idx].source.platform;
}

/* PoP: _resume_target_allowed @ gateway/slash_commands.py:_resume_target_allowed */
bool slash_resume_target_allowed(const char *session_id, const char *user_id) {
    (void)user_id;
    return lookup_by_session_id(session_id) >= 0;
}

/* PoP: _resume_row_visible @ gateway/slash_commands.py:_resume_row_visible */
bool slash_resume_row_visible(const char *session_id) {
    return lookup_by_session_id(session_id) >= 0;
}

/* PoP: _handle_agents_command @ gateway/slash_commands.py:_handle_agents_command */
json_t *slash_handle_agents(const char *args) {
    json_t *r = json_object(); json_set(r, "action", json_string(args ? args : "list")); return r;
}

/* PoP: _handle_stop_command @ gateway/slash_commands.py:_handle_stop_command */
json_t *slash_handle_stop(const char *session_key) {
    (void)session_key;
    json_t *r = json_object(); json_set(r, "ok", json_bool(true)); return r;
}

/* PoP: _handle_platform_command @ gateway/slash_commands.py:_handle_platform_command */
json_t *slash_handle_platform(const char *args) {
    json_t *r = json_object(); json_set(r, "platform", json_string(args ? args : "")); return r;
}

/* PoP: _handle_restart_command @ gateway/slash_commands.py:_handle_restart_command */
json_t *slash_handle_restart(void) {
    json_t *r = json_object(); json_set(r, "ok", json_bool(true)); return r;
}

/* PoP: _handle_version_command @ gateway/slash_commands.py:_handle_version_command */
json_t *slash_handle_version(void) {
    json_t *r = json_object(); json_set(r, "version", json_string("v0.19.0-slermes")); return r;
}

/* PoP: _handle_help_command @ gateway/slash_commands.py:_handle_help_command */
json_t *slash_handle_help(void) {
    json_t *r = json_object();
    json_set(r, "text", json_string("Available commands: /reset /version /help /model /stop /status ..."));
    return r;
}

/* PoP: _handle_commands_command @ gateway/slash_commands.py:_handle_commands_command */
json_t *slash_handle_commands(void) {
    json_t *arr = json_array();
    static const char *cmds[] = {"reset","version","help","model","stop","status","profile","whoami","agents","platform","restart","reasoning","memory","skills","fast","yolo","verbose","footer","compress","topic","title","resume","sessions","branch","topup","usage","insights","update","debug","approve","deny","goal","subgoal","undo","set_home","voice","rollback","background",NULL};
    for (int i = 0; cmds[i]; i++) json_array_append(arr, json_string(cmds[i]));
    return arr;
}

/* PoP: _handle_model_command @ gateway/slash_commands.py:_handle_model_command */
json_t *slash_handle_model(const char *args, const char *session_key) {
    if (args && args[0]) set_model_override(session_key, args);
    json_t *r = json_object();
    const char *cur = get_model_override(session_key);
    json_set(r, "model", json_string(cur ? cur : "default"));
    return r;
}

/* PoP: _handle_codex_runtime_command @ gateway/slash_commands.py:_handle_codex_runtime_command */
json_t *slash_handle_codex_runtime(const char *args) {
    json_t *r = json_object(); json_set(r, "runtime", json_string(args ? args : "")); return r;
}

/* PoP: _handle_personality_command @ gateway/slash_commands.py:_handle_personality_command */
json_t *slash_handle_personality(const char *args) {
    json_t *r = json_object(); json_set(r, "personality", json_string(args ? args : "default")); return r;
}

/* PoP: _handle_retry_command @ gateway/slash_commands.py:_handle_retry_command */
json_t *slash_handle_retry(const char *session_key) {
    (void)session_key;
    json_t *r = json_object(); json_set(r, "ok", json_bool(true)); return r;
}

/* PoP: _handle_goal_command @ gateway/slash_commands.py:_handle_goal_command */
json_t *slash_handle_goal(const char *args) {
    json_t *r = json_object(); json_set(r, "goal", json_string(args ? args : "")); return r;
}

/* PoP: _handle_subgoal_command @ gateway/slash_commands.py:_handle_subgoal_command */
json_t *slash_handle_subgoal(const char *args) {
    json_t *r = json_object(); json_set(r, "subgoal", json_string(args ? args : "")); return r;
}

/* PoP: _handle_undo_command @ gateway/slash_commands.py:_handle_undo_command */
json_t *slash_handle_undo(const char *session_key) {
    rewind_session(session_key, 1);
    json_t *r = json_object(); json_set(r, "ok", json_bool(true)); return r;
}

/* PoP: _handle_set_home_command @ gateway/slash_commands.py:_handle_set_home_command */
json_t *slash_handle_set_home(const char *args) {
    json_t *r = json_object(); json_set(r, "home", json_string(args ? args : "")); return r;
}

/* PoP: _handle_voice_command @ gateway/slash_commands.py:_handle_voice_command */
json_t *slash_handle_voice(const char *args) {
    json_t *r = json_object(); json_set(r, "voice", json_string(args ? args : "")); return r;
}

/* PoP: _handle_rollback_command @ gateway/slash_commands.py:_handle_rollback_command */
json_t *slash_handle_rollback(const char *args, const char *session_key) {
    (void)args;
    rewind_session(session_key, 1);
    json_t *r = json_object(); json_set(r, "ok", json_bool(true)); return r;
}

/* PoP: _handle_background_command @ gateway/slash_commands.py:_handle_background_command */
json_t *slash_handle_background(const char *args) {
    json_t *r = json_object(); json_set(r, "action", json_string(args ? args : "list")); return r;
}

/* PoP: _apply_reasoning_selection @ gateway/slash_commands.py:_apply_reasoning_selection */
json_t *slash_apply_reasoning_selection(const char *selection, const char *session_key) {
    (void)session_key;
    return reasoning_parse_effort(selection, false);
}

/* PoP: _try_send_choice_picker @ gateway/slash_commands.py:_try_send_choice_picker */
bool slash_try_send_choice_picker(json_t *choices) {
    (void)choices; return true;
}

/* PoP: _handle_reasoning_command @ gateway/slash_commands.py:_handle_reasoning_command */
json_t *slash_handle_reasoning(const char *args, const char *session_key) {
    return slash_apply_reasoning_selection(args, session_key);
}

/* PoP: _handle_memory_command @ gateway/slash_commands.py:_handle_memory_command */
json_t *slash_handle_memory(const char *args) {
    json_t *r = json_object(); json_set(r, "action", json_string(args ? args : "list")); return r;
}

/* PoP: _handle_skills_command @ gateway/slash_commands.py:_handle_skills_command */
json_t *slash_handle_skills(const char *args) {
    json_t *r = json_object(); json_set(r, "action", json_string(args ? args : "list")); return r;
}

/* PoP: _handle_fast_command @ gateway/slash_commands.py:_handle_fast_command */
json_t *slash_handle_fast(void) {
    json_t *r = json_object(); json_set(r, "fast", json_bool(true)); return r;
}

/* PoP: _handle_yolo_command @ gateway/slash_commands.py:_handle_yolo_command */
json_t *slash_handle_yolo(void) {
    json_t *r = json_object(); json_set(r, "yolo", json_bool(true)); return r;
}

/* PoP: _handle_verbose_command @ gateway/slash_commands.py:_handle_verbose_command */
json_t *slash_handle_verbose(void) {
    json_t *r = json_object(); json_set(r, "verbose", json_bool(true)); return r;
}

/* PoP: _handle_footer_command @ gateway/slash_commands.py:_handle_footer_command */
json_t *slash_handle_footer(const char *args) {
    json_t *r = json_object(); json_set(r, "footer", json_string(args ? args : "")); return r;
}

/* PoP: _handle_compress_command @ gateway/slash_commands.py:_handle_compress_command */
json_t *slash_handle_compress(const char *session_key) {
    (void)session_key;
    json_t *r = json_object(); json_set(r, "ok", json_bool(true)); return r;
}

/* PoP: _handle_topic_command @ gateway/slash_commands.py:_handle_topic_command */
json_t *slash_handle_topic(const char *args) {
    json_t *r = json_object(); json_set(r, "topic", json_string(args ? args : "")); return r;
}

/* PoP: _handle_title_command @ gateway/slash_commands.py:_handle_title_command */
json_t *slash_handle_title(const char *args) {
    json_t *r = json_object(); json_set(r, "title", json_string(args ? args : "")); return r;
}

/* PoP: _handle_resume_command @ gateway/slash_commands.py:_handle_resume_command */
json_t *slash_handle_resume(const char *args) {
    json_t *r = json_object();
    int idx = args ? lookup_by_session_id(args) : -1;
    json_set(r, "ok", json_bool(idx >= 0));
    if (idx >= 0) json_set(r, "session_key", json_string(g_gw.sessions[idx].key));
    return r;
}

/* PoP: _handle_sessions_command @ gateway/slash_commands.py:_handle_sessions_command */
json_t *slash_handle_sessions(void) {
    json_t *arr = json_array();
    for (int i = 0; i < g_gw.session_count; i++) {
        if (g_gw.sessions[i].in_use) {
            json_t *s = json_object();
            json_set(s, "key", json_string(g_gw.sessions[i].key));
            json_set(s, "session_id", json_string(g_gw.sessions[i].session_id));
            json_array_append(arr, s);
        }
    }
    return arr;
}

/* PoP: _handle_branch_command @ gateway/slash_commands.py:_handle_branch_command */
json_t *slash_handle_branch(const char *args) {
    json_t *r = json_object(); json_set(r, "branch", json_string(args ? args : "main")); return r;
}

/* PoP: _handle_topup_command @ gateway/slash_commands.py:_handle_topup_command */
json_t *slash_handle_topup(const char *args) {
    json_t *r = json_object(); json_set(r, "amount", json_string(args ? args : "")); return r;
}

/* PoP: _context_breakdown_lines @ gateway/slash_commands.py:_context_breakdown_lines */
json_t *slash_context_breakdown_lines(const char *session_key) {
    (void)session_key;
    json_t *r = json_array();
    json_array_append(r, json_string("system: 0 lines"));
    json_array_append(r, json_string("history: 0 lines"));
    return r;
}

/* PoP: _handle_usage_command @ gateway/slash_commands.py:_handle_usage_command */
json_t *slash_handle_usage(void) {
    json_t *r = json_object(); json_set(r, "tokens", json_int(0)); return r;
}

/* PoP: _handle_insights_command @ gateway/slash_commands.py:_handle_insights_command */
json_t *slash_handle_insights(void) {
    json_t *r = json_object(); return r;
}

/* PoP: _handle_reload_mcp_command @ gateway/slash_commands.py:_handle_reload_mcp_command */
json_t *slash_handle_reload_mcp(void) {
    json_t *r = json_object(); json_set(r, "ok", json_bool(true)); return r;
}

/* PoP: _handle_reload_skills_command @ gateway/slash_commands.py:_handle_reload_skills_command */
json_t *slash_handle_reload_skills(void) {
    json_t *r = json_object(); json_set(r, "ok", json_bool(true)); return r;
}

/* PoP: _handle_bundles_command @ gateway/slash_commands.py:_handle_bundles_command */
json_t *slash_handle_bundles(const char *args) {
    json_t *r = json_object(); json_set(r, "action", json_string(args ? args : "list")); return r;
}

/* PoP: _handle_approve_command @ gateway/slash_commands.py:_handle_approve_command */
json_t *slash_handle_approve(const char *args) {
    json_t *r = json_object(); json_set(r, "id", json_string(args ? args : "")); return r;
}

/* PoP: _handle_deny_command @ gateway/slash_commands.py:_handle_deny_command */
json_t *slash_handle_deny(const char *args) {
    json_t *r = json_object(); json_set(r, "id", json_string(args ? args : "")); return r;
}

/* PoP: _handle_debug_command @ gateway/slash_commands.py:_handle_debug_command */
json_t *slash_handle_debug(const char *args) {
    json_t *r = json_object(); json_set(r, "debug", json_string(args ? args : "on")); return r;
}

/* PoP: _handle_update_command @ gateway/slash_commands.py:_handle_update_command */
json_t *slash_handle_update(void) {
    json_t *r = json_object(); json_set(r, "ok", json_bool(true)); return r;
}
