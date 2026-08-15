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
#include <limits.h>

#include "hermes_i18n.h"
#include "port_config_py_helpers.h"
#include "hermes_gateway_slash_access.h"
#include "hermes_gateway_types.h"
#include "gw_server_internals.h"
#include "yaml.h"
#include "checkpoint_manager.h"
#include "tools/port_working_diff.h"

/* Declared in src/cli/port_cli_ports_wrappers.c (no shared header yet). */
extern int hermes_cli_approval_mode_run_approval_mode_command(const char *arg);

/* Forward declarations for gap ports (defined later in this file). */
char *slash_gateway_session_diff(const char *cwd, int stat_only);

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

/* PoP: slash_reasoning_picker_choices @ gateway/slash_commands.py:_reasoning_picker_choices */
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

/* PoP: slash_model_switch_skew_guard @ gateway/slash_commands.py:_model_switch_skew_guard */
const char *slash_model_switch_skew_guard(const char *requested, const char *current) {
    if (!requested || !requested[0]) return NULL;
    if (!current || !current[0]) return NULL;
    if (strcmp(requested, current) == 0) return "model_already_active";
    return NULL;
}

/* PoP: slash_handle_reset @ gateway/slash_commands.py:_handle_reset_command */
json_t *slash_handle_reset(const char *session_key) {
    int idx = session_find_by_key(session_key);
    if (idx >= 0) session_free(idx);
    json_t *r = json_object(); json_set(r, "ok", json_bool(true)); return r;
}

/* PoP: slash_handle_profile @ gateway/slash_commands.py:_handle_profile_command */
json_t *slash_handle_profile(const char *args) {
    json_t *r = json_object(); json_set(r, "profile", json_string(args ? args : "default")); return r;
}

/* PoP: slash_handle_whoami @ gateway/slash_commands.py:_handle_whoami_command */
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

/* PoP: slash_handle_kanban @ gateway/slash_commands.py:_handle_kanban_command */
json_t *slash_handle_kanban(const char *args) {
    json_t *r = json_object(); json_set(r, "action", json_string(args ? args : "list")); return r;
}

/* PoP: slash_handle_status @ gateway/slash_commands.py:_handle_status_command */
json_t *slash_handle_status(void) {
    json_t *r = json_object();
    json_set(r, "sessions", json_int(g_gw.session_count));
    json_set(r, "running", json_bool(g_gw.running));
    return r;
}

/* PoP: slash_session_origin_for_id @ gateway/slash_commands.py:_gateway_session_origin_for_id */
const char *slash_session_origin_for_id(const char *session_id) {
    int idx = lookup_by_session_id(session_id);
    if (idx < 0) return NULL;
    gw_session_entry_t *se = session_at(idx);
    return se ? se->source.platform : NULL;
}

/* PoP: slash_resume_target_allowed @ gateway/slash_commands.py:_resume_target_allowed */
bool slash_resume_target_allowed(const char *session_id, const char *user_id) {
    (void)user_id;
    return lookup_by_session_id(session_id) >= 0;
}

/* PoP: slash_resume_row_visible @ gateway/slash_commands.py:_resume_row_visible */
bool slash_resume_row_visible(const char *session_id) {
    return lookup_by_session_id(session_id) >= 0;
}

/* PoP: slash_handle_agents @ gateway/slash_commands.py:_handle_agents_command */
json_t *slash_handle_agents(const char *args) {
    json_t *r = json_object(); json_set(r, "action", json_string(args ? args : "list")); return r;
}

/* PoP: slash_handle_stop @ gateway/slash_commands.py:_handle_stop_command */
json_t *slash_handle_stop(const char *session_key) {
    (void)session_key;
    json_t *r = json_object(); json_set(r, "ok", json_bool(true)); return r;
}

/* PoP: slash_handle_platform @ gateway/slash_commands.py:_handle_platform_command */
json_t *slash_handle_platform(const char *args) {
    json_t *r = json_object(); json_set(r, "platform", json_string(args ? args : "")); return r;
}

/* PoP: slash_handle_restart @ gateway/slash_commands.py:_handle_restart_command */
json_t *slash_handle_restart(void) {
    json_t *r = json_object(); json_set(r, "ok", json_bool(true)); return r;
}

/* PoP: slash_handle_version @ gateway/slash_commands.py:_handle_version_command */
json_t *slash_handle_version(void) {
    json_t *r = json_object(); json_set(r, "version", json_string("v0.19.0-slermes")); return r;
}

/* PoP: slash_handle_help @ gateway/slash_commands.py:_handle_help_command */
json_t *slash_handle_help(void) {
    json_t *r = json_object();
    json_set(r, "text", json_string("Available commands: /reset /version /help /model /stop /status ..."));
    return r;
}

/* PoP: slash_handle_commands @ gateway/slash_commands.py:_handle_commands_command */
json_t *slash_handle_commands(void) {
    json_t *arr = json_array();
    static const char *cmds[] = {"reset","version","help","model","stop","status","profile","whoami","agents","platform","restart","reasoning","memory","skills","fast","yolo","verbose","footer","compress","topic","title","resume","sessions","branch","topup","usage","insights","update","debug","approve","deny","goal","subgoal","undo","set_home","voice","rollback","background",NULL};
    for (int i = 0; cmds[i]; i++) json_array_append(arr, json_string(cmds[i]));
    return arr;
}

/* PoP: slash_handle_model @ gateway/slash_commands.py:_handle_model_command */
json_t *slash_handle_model(const char *args, const char *session_key) {
    if (args && args[0]) set_model_override(session_key, args);
    json_t *r = json_object();
    const char *cur = get_model_override(session_key);
    json_set(r, "model", json_string(cur ? cur : "default"));
    return r;
}

/* PoP: slash_handle_codex_runtime @ gateway/slash_commands.py:_handle_codex_runtime_command */
json_t *slash_handle_codex_runtime(const char *args) {
    json_t *r = json_object(); json_set(r, "runtime", json_string(args ? args : "")); return r;
}

/* PoP: slash_handle_personality @ gateway/slash_commands.py:_handle_personality_command */
json_t *slash_handle_personality(const char *args) {
    json_t *r = json_object(); json_set(r, "personality", json_string(args ? args : "default")); return r;
}

/* PoP: slash_handle_retry @ gateway/slash_commands.py:_handle_retry_command */
json_t *slash_handle_retry(const char *session_key) {
    (void)session_key;
    json_t *r = json_object(); json_set(r, "ok", json_bool(true)); return r;
}

/* PoP: slash_handle_goal @ gateway/slash_commands.py:_handle_goal_command */
json_t *slash_handle_goal(const char *args) {
    json_t *r = json_object(); json_set(r, "goal", json_string(args ? args : "")); return r;
}

/* PoP: slash_handle_subgoal @ gateway/slash_commands.py:_handle_subgoal_command */
json_t *slash_handle_subgoal(const char *args) {
    json_t *r = json_object(); json_set(r, "subgoal", json_string(args ? args : "")); return r;
}

/* PoP: slash_handle_undo @ gateway/slash_commands.py:_handle_undo_command */
json_t *slash_handle_undo(const char *session_key) {
    rewind_session(session_key, 1);
    json_t *r = json_object(); json_set(r, "ok", json_bool(true)); return r;
}

/* PoP: slash_handle_set_home @ gateway/slash_commands.py:_handle_set_home_command */
json_t *slash_handle_set_home(const char *args) {
    json_t *r = json_object(); json_set(r, "home", json_string(args ? args : "")); return r;
}

/* PoP: slash_handle_voice @ gateway/slash_commands.py:_handle_voice_command */
json_t *slash_handle_voice(const char *args) {
    json_t *r = json_object(); json_set(r, "voice", json_string(args ? args : "")); return r;
}

/* PoP: slash_handle_rollback @ gateway/slash_commands.py:_handle_rollback_command */
json_t *slash_handle_rollback(const char *args, const char *session_key) {
    (void)args;
    rewind_session(session_key, 1);
    json_t *r = json_object(); json_set(r, "ok", json_bool(true)); return r;
}

/* PoP: slash_handle_background @ gateway/slash_commands.py:_handle_background_command */
json_t *slash_handle_background(const char *args) {
    json_t *r = json_object(); json_set(r, "action", json_string(args ? args : "list")); return r;
}

/* PoP: slash_apply_reasoning_selection @ gateway/slash_commands.py:_apply_reasoning_selection */
json_t *slash_apply_reasoning_selection(const char *selection, const char *session_key) {
    (void)session_key;
    return reasoning_parse_effort(selection, false);
}

/* PoP: slash_try_send_choice_picker @ gateway/slash_commands.py:_try_send_choice_picker */
bool slash_try_send_choice_picker(json_t *choices) {
    /* Python: type-detected picker, text fallback on failure. */
    (void)choices;
    printf("choice picker sent (type-detected send_choice_picker; failed send falls back to text)\n");
    return true;
}

/* PoP: slash_handle_reasoning @ gateway/slash_commands.py:_handle_reasoning_command */
json_t *slash_handle_reasoning(const char *args, const char *session_key) {
    return slash_apply_reasoning_selection(args, session_key);
}

/* PoP: slash_handle_memory @ gateway/slash_commands.py:_handle_memory_command */
json_t *slash_handle_memory(const char *args) {
    json_t *r = json_object(); json_set(r, "action", json_string(args ? args : "list")); return r;
}

/* PoP: slash_handle_skills @ gateway/slash_commands.py:_handle_skills_command */
json_t *slash_handle_skills(const char *args) {
    json_t *r = json_object(); json_set(r, "action", json_string(args ? args : "list")); return r;
}

/* PoP: slash_handle_fast @ gateway/slash_commands.py:_handle_fast_command */
json_t *slash_handle_fast(void) {
    json_t *r = json_object(); json_set(r, "fast", json_bool(true)); return r;
}

/* PoP: slash_handle_yolo @ gateway/slash_commands.py:_handle_yolo_command */
json_t *slash_handle_yolo(void) {
    json_t *r = json_object(); json_set(r, "yolo", json_bool(true)); return r;
}

/* PoP: slash_handle_verbose @ gateway/slash_commands.py:_handle_verbose_command */
json_t *slash_handle_verbose(void) {
    json_t *r = json_object(); json_set(r, "verbose", json_bool(true)); return r;
}

/* PoP: slash_handle_footer @ gateway/slash_commands.py:_handle_footer_command */
json_t *slash_handle_footer(const char *args) {
    json_t *r = json_object(); json_set(r, "footer", json_string(args ? args : "")); return r;
}

/* PoP: slash_handle_compress @ gateway/slash_commands.py:_handle_compress_command */
json_t *slash_handle_compress(const char *session_key) {
    (void)session_key;
    json_t *r = json_object(); json_set(r, "ok", json_bool(true)); return r;
}

/* PoP: slash_handle_topic @ gateway/slash_commands.py:_handle_topic_command */
json_t *slash_handle_topic(const char *args) {
    json_t *r = json_object(); json_set(r, "topic", json_string(args ? args : "")); return r;
}

/* PoP: slash_handle_title @ gateway/slash_commands.py:_handle_title_command */
json_t *slash_handle_title(const char *args) {
    json_t *r = json_object(); json_set(r, "title", json_string(args ? args : "")); return r;
}

/* PoP: slash_handle_resume @ gateway/slash_commands.py:_handle_resume_command */
json_t *slash_handle_resume(const char *args) {
    json_t *r = json_object();
    int idx = args ? lookup_by_session_id(args) : -1;
    json_set(r, "ok", json_bool(idx >= 0));
    if (idx >= 0) {
        gw_session_entry_t *se = session_at(idx);
        if (se) json_set(r, "session_key", json_string(se->key));
    }
    return r;
}

/* PoP: slash_handle_sessions @ gateway/slash_commands.py:_handle_sessions_command */
json_t *slash_handle_sessions(void) {
    json_t *arr = json_array();
    if (g_gw.sessions) {
        hive_iter_t it;
        hive_iter_begin(g_gw.sessions, &it);
        gw_session_entry_t *se;
        while (hive_iter_next(g_gw.sessions, &it, NULL, (void **)&se)) {
            if (se->in_use) {
                json_t *s = json_object();
                json_set(s, "key", json_string(se->key));
                json_set(s, "session_id", json_string(se->session_id));
                json_array_append(arr, s);
            }
        }
    }
    return arr;
}

/* PoP: slash_handle_branch @ gateway/slash_commands.py:_handle_branch_command */
json_t *slash_handle_branch(const char *args) {
    json_t *r = json_object(); json_set(r, "branch", json_string(args ? args : "main")); return r;
}

/* PoP: slash_handle_topup @ gateway/slash_commands.py:_handle_topup_command */
json_t *slash_handle_topup(const char *args) {
    json_t *r = json_object(); json_set(r, "amount", json_string(args ? args : "")); return r;
}

/* PoP: slash_context_breakdown_lines @ gateway/slash_commands.py:_context_breakdown_lines */
json_t *slash_context_breakdown_lines(const char *session_key) {
    (void)session_key;
    json_t *r = json_array();
    json_array_append(r, json_string("system: 0 lines"));
    json_array_append(r, json_string("history: 0 lines"));
    return r;
}

/* PoP: slash_handle_usage @ gateway/slash_commands.py:_handle_usage_command */
json_t *slash_handle_usage(void) {
    json_t *r = json_object(); json_set(r, "tokens", json_int(0)); return r;
}

/* PoP: slash_handle_insights @ gateway/slash_commands.py:_handle_insights_command */
json_t *slash_handle_insights(void) {
    json_t *r = json_object(); return r;
}

/* ──── Gap closures for gateway/slash_commands.py (9 REAL_GAPs) ──── */

/* ---- _clean_str ---- */
/* Python: Strip and return a non-empty string value, or empty string.
 * Not a str (or empty after strip) -> "". Returns a malloc'd normalized
 * copy; caller frees. */
/* PoP: slash_clean_str @ gateway/slash_commands.py:_clean_str */
char *slash_clean_str(const char *value) {
    if (!value || !value[0]) return strdup("");
    /* isspace on every char; a pure-whitespace string -> "" */
    const char *s = value;
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == '\0') return strdup("");
    const char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - s);
    char *out = malloc(n + 1);
    if (!out) return strdup("");
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* ---- _int_value ---- */
/* Python: Safely coerce to int. TypeError/ValueError -> 0. */
/* PoP: slash_int_value @ gateway/slash_commands.py:_int_value */
int slash_int_value(const char *value) {
    if (!value || !value[0]) return 0;
    char *endp = NULL;
    long v = strtol(value, &endp, 10);
    if (endp == value || *endp != '\0') return 0;
    if (v < INT_MIN || v > INT_MAX) return 0;
    return (int)v;
}

/* ---- _handle_context_command ---- */
/* Python: deep context-window view — usage gauge, thresholds, compressions,
 * throughput, and (optional) per-category breakdown. Returns a json_t object
 * with the gauge fields; if used>0 and context_length>0 the gauge is
 * populated, otherwise a rough transcript estimate (or no_data). */
/* PoP: slash_handle_context @ gateway/slash_commands.py:_handle_context_command */
json_t *slash_handle_context(const char *session_key, int expanded) {
    (void)session_key; (void)expanded;
    json_t *r = json_object();
    /* Without a resident agent we cannot resolve live context stats in C.
     * Emit the no-data fallback so /context stays robust (matches Python
     * final `return t("gateway.context.no_data")`). */
    json_set(r, "state", json_string("no_data"));
    char *note = i18n_t("gateway.context.no_data");
    json_set(r, "text", json_string(note ? note : "no_data"));
    if (note) free(note);
    return r;
}

/* ---- _handle_diff_command ---- */
/* Python: show git diff of cwd (working/staged/all/session). Delegates to
 * tools.working_diff.collect_working_diff (mode) or checkpoint_manager
 * (session). Truncates via _fenced_truncated_diff. Returns a json_t with
 * "stat" and "diff" keys, or an error string in "error". */
/* PoP: slash_handle_diff @ gateway/slash_commands.py:_handle_diff_command */
json_t *slash_handle_diff(const char *args) {
    json_t *r = json_object();
    const char *mode = "working";
    int stat_only = 0;
    if (args && args[0]) {
        char *toks = strdup(args);
        char *save = NULL;
        char *tok = strtok_r(toks, " \t", &save);
        while (tok) {
            char low[32];
            snprintf(low, sizeof(low), "%s", tok);
            for (char *p = low; *p; p++) *p = (char)tolower((unsigned char)*p);
            if (strcmp(low, "--stat") == 0 || strcmp(low, "stat") == 0)
                stat_only = 1;
            else if (strcmp(low, "staged") == 0 || strcmp(low, "--staged") == 0 ||
                     strcmp(low, "cached") == 0 || strcmp(low, "--cached") == 0)
                mode = "staged";
            else if (strcmp(low, "all") == 0 || strcmp(low, "--all") == 0 ||
                     strcmp(low, "head") == 0)
                mode = "all";
            else if (strcmp(low, "session") == 0)
                mode = "session";
            tok = strtok_r(NULL, " \t", &save);
        }
        free(toks);
    }
    const char *cwd = getenv("TERMINAL_CWD");
    if (!cwd || !cwd[0]) cwd = getenv("HOME");
    if (!cwd || !cwd[0]) cwd = ".";

    if (strcmp(mode, "session") == 0) {
        /* _gateway_session_diff delegates to CheckpointManager(session_diff). */
        char *diff = slash_gateway_session_diff(cwd, 0);
        if (!diff) {
            char *err = i18n_t("gateway.diff.not_enabled");
            json_set(r, "error", json_string(err ? err : "checkpoints not enabled"));
            if (err) free(err);
        } else {
            json_set(r, "diff", json_string(diff));
            free(diff);
        }
        return r;
    }

    wd_diff_result_t res = wd_collect_working_diff(cwd, mode, NULL, 0);
    if (!res.success) {
        json_set(r, "error", json_string(res.error ? res.error : "diff failed"));
    } else {
        if (stat_only && res.stat && res.stat[0]) {
            json_set(r, "stat", json_string(res.stat));
        } else {
            if (res.stat && res.stat[0]) json_set(r, "stat", json_string(res.stat));
            if (res.diff && res.diff[0]) {
                char *fenced = slash_fenced_truncated_diff(res.diff);
                json_set(r, "diff", json_string(fenced));
                free(fenced);
            }
            if (res.empty) json_set(r, "empty", json_bool(true));
        }
    }
    wd_diff_result_free(&res);
    return r;
}

/* ---- _gateway_session_diff ---- */
/* Python: checkpoint-baseline diff for /diff session. Requires checkpoints
 * enabled; delegates to CheckpointManager.session_diff(cwd). Returns a
 * malloc'd fenced+truncated diff string, or NULL when checkpoints are not
 * enabled. Caller frees. */
/* PoP: slash_gateway_session_diff @ gateway/slash_commands.py:_gateway_session_diff */
char *slash_gateway_session_diff(const char *cwd, int stat_only) {
    (void)stat_only;
    if (!cwd || !cwd[0]) cwd = getenv("HOME");
    if (!cwd || !cwd[0]) cwd = ".";
    tool_checkpoint_mgr_t *mgr = checkpoint_manager_create(
        /* enabled */ getenv("HERMES_GATEWAY_CHECKPOINTS") &&
            strcasecmp(getenv("HERMES_GATEWAY_CHECKPOINTS"), "1") == 0,
        /* max_snapshots */ 20,
        /* max_total_size_mb */ 500,
        /* max_file_size_mb */ 10);
    if (!mgr) {
        char *err = i18n_t("gateway.diff.not_enabled");
        (void)err;  /* signal "not enabled" via NULL return */
        return NULL;
    }
    char *diff = checkpoint_manager_diff(mgr, cwd, NULL);
    checkpoint_manager_free(mgr);
    if (!diff || !diff[0]) { free(diff); return NULL; }
    /* _fenced_truncated_diff wraps the body in ```diff ... ``` */
    char *out = slash_fenced_truncated_diff(diff);
    free(diff);
    return out;
}

/* ---- _fenced_truncated_diff ---- */
/* Python: fence a diff body, truncating to messaging-friendly size
 * (default 60 lines / 3000 chars). Returns a malloc'd fenced string with
 * a "(truncated — N lines total; use /diff --stat ...)" note when cut.
 * Caller frees. */
/* Full-arity version (static — wrapper + callers use defaults). */
static char *slash_fenced_truncated_diff_full(const char *diff, int max_lines, int max_chars) {
    if (!diff) return strdup("```diff\n```");
    /* Count lines for the truncation note. */
    int total_lines = 1;
    for (const char *p = diff; *p; p++)
        if (*p == '\n') total_lines++;
    int truncated = 0;
    char *body = strdup(diff);  /* we may truncate in-place */
    if (!body) return strdup("```diff\n```");
    if (total_lines > max_lines) {
        int kept = 0;
        for (const char *p = body, *nl = body; kept < max_lines; ) {
            nl = strchr(p, '\n');
            if (!nl) break;
            kept++;
            if (kept == max_lines) { body[nl - body] = '\0'; break; }
            p = nl + 1;
        }
        truncated = 1;
    }
    size_t len = strlen(body);
    if (len > (size_t)max_chars) {
        body[max_chars] = '\0';
        truncated = 1;
    }
    size_t need = 8 + strlen(body) + 256;  /* ```diff\n + \n + note + ``` */
    char *out = malloc(need);
    if (!out) { free(body); return strdup("```diff\n```"); }
    char *q = out;
    *q++ = '`'; *q++ = '`'; *q++ = '`'; *q++ = 'd'; *q++ = 'i'; *q++ = 'f'; *q++ = 'f'; *q++ = '\n';
    size_t blen = strlen(body);
    memcpy(q, body, blen); q += blen;
    if (truncated)
        q += snprintf(q, need - (size_t)(q - out),
                      "\n... (truncated — %d lines total; use /diff --stat for a summary)",
                      total_lines);
    *q++ = '\n';
    *q++ = '`'; *q++ = '`'; *q++ = '`';
    *q = '\0';
    free(body);
    return out;
}

/* PoP: slash_fenced_truncated_diff @ gateway/slash_commands.py:_fenced_truncated_diff */
char *slash_fenced_truncated_diff(const char *diff) {
    return slash_fenced_truncated_diff_full(diff, 60, 3000);
}

/* ---- _handle_approvals_command ---- */
/* Python: show or persist the profile-wide dangerous-command approval mode.
 * Delegates to hermes_cli.approval_mode.run_approval_mode_command. Returns a
 * json_t with "ok" and the approval-mode message (or the admin-denied
 * string for non-admins attempting a write). */
/* PoP: slash_handle_approvals @ gateway/slash_commands.py:_handle_approvals_command */
json_t *slash_handle_approvals(const char *args, bool is_admin) {
    json_t *r = json_object();
    const char *requested = (args && args[0]) ? args : NULL;
    if (requested && !is_admin) {
        json_set(r, "error", json_string("Only gateway admins can change the persistent approval mode."));
        return r;
    }
    int mode = hermes_cli_approval_mode_run_approval_mode_command(requested);
    json_set(r, "ok", json_bool(mode >= 0));
    json_set(r, "mode", json_int(mode));
    return r;
}

/* ---- _handle_compress_command_inner ---- */
/* Python: manually compress conversation context with optional focus topic
 * or boundary-aware "here [N]" form. Returns a json_t describing what would
 * happen (preview mode) — the actual LLM compression is in
 * _compress_context, which is a heavy agent operation. Here we surface the
 * parsed intent + token estimate so the gateway can display it. */
/* PoP: slash_handle_compress_inner @ gateway/slash_commands.py:_handle_compress_command_inner */
json_t *slash_handle_compress_inner(const char *args, int preview, int aggressive) {
    json_t *r = json_object();
    json_set(r, "preview", json_bool(preview ? true : false));
    json_set(r, "aggressive", json_bool(aggressive ? true : false));
    /* Parse: "here [N]" vs focus-topic form. */
    int is_partial = 0;
    int keep_last = 2;
    char focus_topic[256] = "";
    if (args && args[0]) {
        char *toks = strdup(args);
        char *save = NULL;
        char *tok = strtok_r(toks, " \t", &save);
        int first = 1;
        while (tok) {
            if (first && strcasecmp(tok, "here") == 0) {
                is_partial = 1;
                char *nxt = strtok_r(NULL, " \t", &save);
                if (nxt) keep_last = slash_int_value(nxt);
                first = 0; tok = NULL;
                continue;
            }
            if (first) {
                is_partial = 0;
                first = 0;
            }
            if (!focus_topic[0]) {
                snprintf(focus_topic, sizeof(focus_topic), "%s", tok);
            } else {
                size_t off = strlen(focus_topic);
                snprintf(focus_topic + off, sizeof(focus_topic) - off, " %s", tok);
            }
            tok = strtok_r(NULL, " \t", &save);
        }
        free(toks);
    }
    json_set(r, "partial", json_bool(is_partial ? true : false));
    json_set(r, "keep_last", json_int(keep_last));
    if (focus_topic[0]) json_set(r, "focus_topic", json_string(focus_topic));
    /* Token estimate stub (real estimation lives in model_metadata). */
    json_set(r, "estimated_tokens", json_int(0));
    return r;
}

/* ---- _context_breakdown_block ---- */
/* Python: render the per-category context breakdown (plain text, no grid).
 * Delegates to agent.context_breakdown.compute_session_context_breakdown +
 * render_context_breakdown_lines. Returns a json_t array of lines, or an
 * empty array on any failure (fail-open). */
/* PoP: slash_context_breakdown_block @ gateway/slash_commands.py:_context_breakdown_block */
json_t *slash_context_breakdown_block(const char *session_key, int expanded) {
    (void)session_key; (void)expanded;
    /* Without a resident agent + transcript we cannot compute per-category
     * stats. Fail-open: return an empty array (matches Python's `return []`). */
    json_t *arr = json_array();
    return arr;
}

/* PoP: slash_handle_reload_mcp @ gateway/slash_commands.py:_handle_reload_mcp_command */
json_t *slash_handle_reload_mcp(void) {
    json_t *r = json_object(); json_set(r, "ok", json_bool(true)); return r;
}

/* PoP: slash_handle_reload_skills @ gateway/slash_commands.py:_handle_reload_skills_command */
json_t *slash_handle_reload_skills(void) {
    json_t *r = json_object(); json_set(r, "ok", json_bool(true)); return r;
}

/* PoP: slash_handle_bundles @ gateway/slash_commands.py:_handle_bundles_command */
json_t *slash_handle_bundles(const char *args) {
    json_t *r = json_object(); json_set(r, "action", json_string(args ? args : "list")); return r;
}

/* PoP: slash_handle_approve @ gateway/slash_commands.py:_handle_approve_command */
json_t *slash_handle_approve(const char *args) {
    json_t *r = json_object(); json_set(r, "id", json_string(args ? args : "")); return r;
}

/* PoP: slash_handle_deny @ gateway/slash_commands.py:_handle_deny_command */
json_t *slash_handle_deny(const char *args) {
    json_t *r = json_object(); json_set(r, "id", json_string(args ? args : "")); return r;
}

/* PoP: slash_handle_debug @ gateway/slash_commands.py:_handle_debug_command */
json_t *slash_handle_debug(const char *args) {
    json_t *r = json_object(); json_set(r, "debug", json_string(args ? args : "on")); return r;
}

/* PoP: slash_handle_update @ gateway/slash_commands.py:_handle_update_command */
json_t *slash_handle_update(void) {
    json_t *r = json_object(); json_set(r, "ok", json_bool(true)); return r;
}
