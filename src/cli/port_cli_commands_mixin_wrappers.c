/*
 * port_cli_commands_mixin_wrappers.c — C port of hermes_cli/cli_commands_mixin.py
 * 43 PoP-annotated handler methods for CLI interactive commands.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"
#include "profile_store.h"
#include "slermes_home.h"
#include "tools/process_registry.h"
#include "port_config_py_helpers.h"
#include "hermes_core_types.h"
#include "blueprint_cmd.h"
#include "cron_suggestions.h"
#include "port_hermes_cli_focus_view.h"
#include "tools/port_working_diff.h"

/* Real command handlers (cli_cmd_*.c / commands.c) — state-safe (ignore agent_state_t). */
extern void cmd_cron(const char *args, agent_state_t *state);
extern void cmd_kanban(const char *args, agent_state_t *state);
extern void cmd_skills(const char *args, agent_state_t *state);
extern void cmd_browser(const char *args, agent_state_t *state);
extern void cmd_tools(const char *args, agent_state_t *state);
extern void cmd_update(const char *args, agent_state_t *state);
extern void cmd_goal(agent_state_t *state, const char *args);
extern void cmd_subgoal(const char *args, agent_state_t *state);
extern void cmd_skin(const char *args, agent_state_t *state);
extern void cmd_voice(const char *args, agent_state_t *state);
extern void cmd_background(const char *args, agent_state_t *state);
extern void cmd_pet(const char *args, agent_state_t *state);
extern void cmd_personality(const char *args, agent_state_t *state);
extern int hermes_cli_journey_cmd_journey(const char *arg);
extern void cmd_copy(const char *args, agent_state_t *state);
extern void cmd_paste(const char *args, agent_state_t *state);
extern void cmd_image(const char *args, agent_state_t *state);
/* State-coupled live handlers (used by real dispatch); ccm_handle_* delegates with state. */
extern void cmd_rollback(const char *args, agent_state_t *state);
extern void cmd_snapshot(const char *args, agent_state_t *state);
extern void cmd_handoff(const char *args, agent_state_t *state);
extern void cmd_resume(const char *args, agent_state_t *state);
extern void cmd_sessions(const char *args, agent_state_t *state);
extern void cmd_branch(const char *args, agent_state_t *state);
extern void cmd_bundles(const char *args, agent_state_t *state);
extern void cmd_debug(const char *args, agent_state_t *state);
extern void cmd_memory(const char *args, agent_state_t *state);
extern void cmd_curator(const char *args, agent_state_t *state);
extern int hermes_cli_suggestions_cmd_handle_suggestions_command(const char *arg);
extern char *build_learn_prompt(const char *user_request);
extern blueprint_catalog_t *blueprint_catalog_load_json(const char *catalog_json);
extern const char *blueprint_catalog_raw_json(void);
extern char *blueprint_cmd_format_catalog(const blueprint_catalog_t *cat);
extern void blueprint_catalog_free(blueprint_catalog_t *cat);
extern json_t *cron_sugg_list_pending(void);
extern int hermes_cli_init_command_build_init_prompt_for_cwd(const char *arg);
extern const char *get_approval_mode(void);

/* PoP: _handle_rollback_command @ hermes_cli/cli_commands_mixin.py:_handle_rollback_command */
int ccm_handle_rollback_command(agent_state_t *state, const char *args) {
    cmd_rollback(args ? args : "", state);
    return 0;
}
/* PoP: _handle_snapshot_command @ hermes_cli/cli_commands_mixin.py:_handle_snapshot_command */
int ccm_handle_snapshot_command(agent_state_t *state, const char *args) {
    cmd_snapshot(args ? args : "", state);
    return 0;
}
/* PoP: _handle_stop_command @ hermes_cli/cli_commands_mixin.py:_handle_stop_command */
int ccm_handle_stop_command(const char *args) {
    (void)args;
    int killed = process_registry_kill_all(NULL, NULL, true);
    if (killed > 0)
        printf("  Stopping background process(es)...\n  Stopped %d process(es).\n", killed);
    else
        printf("  No running background processes.\n");
    return 0;
}
/* PoP: _handle_agents_command @ hermes_cli/cli_commands_mixin.py:_handle_agents_command */
int ccm_handle_agents_command(const char *args) {
    (void)args;
    char *raw = process_registry_list(NULL, NULL);
    if (!raw) { printf("  No background process information available.\n"); return 0; }
    char *jerr = NULL;
    json_t *arr = json_parse(raw, &jerr);
    free(jerr);
    free(raw);
    int running = 0, finished = 0;
    if (arr && arr->type == JSON_ARRAY) {
        size_t n = json_len(arr);
        for (size_t i = 0; i < n; i++) {
            json_t *p = json_array_get(arr, i);
            const char *status = json_get_str(p, "status", "");
            const char *sid = json_get_str(p, "session_id", "?");
            const char *cmd = json_get_str(p, "command", "");
            int up_secs = (int)json_get_num(p, "uptime_seconds", 0);
            int h = up_secs / 3600, m = (up_secs % 3600) / 60, s = up_secs % 60;
            char up[32];
            if (h > 0) snprintf(up, sizeof(up), "%dh%dm", h, m);
            else if (m > 0) snprintf(up, sizeof(up), "%dm%ds", m, s);
            else snprintf(up, sizeof(up), "%ds", s);
            if (strcmp(status, "running") == 0) {
                running++;
                printf("    %s · %s · %.80s\n", sid, up, cmd);
            } else {
                finished++;
            }
        }
    }
    if (arr) json_free(arr);
    printf("  Running processes: %d\n", running);
    if (finished) printf("  Recently finished: %d\n", finished);
    return 0;
}
/* PoP: _handle_journey_command @ hermes_cli/cli_commands_mixin.py:_handle_journey_command */
int ccm_handle_journey_command(const char *args) {
    (void)args;
    /* Faithful port of `hermes journey`: show the learning timeline. The
     * Python build aggregates learned skills + memory nodes; C does not yet
     * port that aggregation, so we read the journey journal (one JSON object
     * per line) under HERMES_HOME and print it as a timeline. */
    char home[HERMES_PATH_MAX];
    const char *h = slermes_home();
    if (!h) h = ".";
    strncpy(home, h, sizeof(home) - 1);
    home[sizeof(home) - 1] = '\0';
    char path[HERMES_PATH_MAX];
    snprintf(path, sizeof(path), "%s/journey.jsonl", home);
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("  Journey is empty — nothing learned/recorded yet.\n");
        return 0;
    }
    printf("  Learning journey:\n");
    char line[2048];
    int n = 0;
    while (fgets(line, sizeof(line), f)) {
        size_t L = strlen(line);
        while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
        if (!*line) continue;
        printf("    - %s\n", line);
        if (++n >= 200) { printf("    ... (truncated at 200 entries)\n"); break; }
    }
    fclose(f);
    if (n == 0) printf("  Journey is empty — nothing learned/recorded yet.\n");
    return 0;
}
/* PoP: _handle_paste_command @ hermes_cli/cli_commands_mixin.py:_handle_paste_command */
int ccm_handle_paste_command(const char *args) {
    cmd_paste(args ? args : "", NULL);
    return 0;
}
/* PoP: _handle_copy_command @ hermes_cli/cli_commands_mixin.py:_handle_copy_command */
int ccm_handle_copy_command(const char *args) {
    cmd_copy(args ? args : "", NULL);
    return 0;
}
/* PoP: _handle_image_command @ hermes_cli/cli_commands_mixin.py:_handle_image_command */
int ccm_handle_image_command(const char *args) {
    cmd_image(args ? args : "", NULL);
    return 0;
}
/* PoP: _handle_tools_command @ hermes_cli/cli_commands_mixin.py:_handle_tools_command */
int ccm_handle_tools_command(const char *args) {
    cmd_tools(args ? args : "", NULL);
    return 0;
}
/* PoP: _handle_profile_command @ hermes_cli/cli_commands_mixin.py:_handle_profile_command */
int ccm_handle_profile_command(const char *args) {
    (void)args;
    char *name = profile_get_active_name();
    const char *home = slermes_home();
    printf("\n  Profile: %s\n  Home:    %s\n\n", name ? name : "default", home ? home : "");
    free(name);
    return 0;
}
/* PoP: _handle_handoff_command @ hermes_cli/cli_commands_mixin.py:_handle_handoff_command */
int ccm_handle_handoff_command(agent_state_t *state, const char *args) {
    cmd_handoff(args ? args : "", state);
    return 0;
}
/* PoP: _handle_resume_command @ hermes_cli/cli_commands_mixin.py:_handle_resume_command */
int ccm_handle_resume_command(agent_state_t *state, const char *args) {
    cmd_resume(args ? args : "", state);
    return 0;
}
/* PoP: _handle_sessions_command @ hermes_cli/cli_commands_mixin.py:_handle_sessions_command */
int ccm_handle_sessions_command(agent_state_t *state, const char *args) {
    cmd_sessions(args ? args : "", state);
    return 0;
}
/* PoP: _handle_branch_command @ hermes_cli/cli_commands_mixin.py:_handle_branch_command */
int ccm_handle_branch_command(agent_state_t *state, const char *args) {
    cmd_branch(args ? args : "", state);
    return 0;
}
/* PoP: _handle_personality_command @ hermes_cli/cli_commands_mixin.py:_handle_personality_command */
int ccm_handle_personality_command(const char *args) {
    cmd_personality(args ? args : "", NULL);
    return 0;
}
/* PoP: _handle_pet_command @ hermes_cli/cli_commands_mixin.py:_handle_pet_command */
int ccm_handle_pet_command(const char *args) {
    cmd_pet(args ? args : "", NULL);
    return 0;
}
/* PoP: _handle_hatch_command @ hermes_cli/cli_commands_mixin.py:_handle_hatch_command */
int ccm_handle_hatch_command(const char *args) {
    cmd_pet("hatch", NULL);
    (void)args;
    return 0;
}
/* PoP: _handle_cron_command @ hermes_cli/cli_commands_mixin.py:_handle_cron_command */
int ccm_handle_cron_command(const char *args) {
    cmd_cron(args ? args : "", NULL);
    return 0;
}
/* PoP: _handle_suggestions_command @ hermes_cli/cli_commands_mixin.py:_handle_suggestions_command */
/* PoP: ccm_handle_suggestions_command @ gateway/run.py:_handle_suggestions_command */
int ccm_handle_suggestions_command(const char *args) {
    (void)args;
    json_t *pending = cron_sugg_list_pending();
    if (!pending || pending->type != JSON_ARRAY || json_len(pending) == 0) {
        printf("  No pending suggestions. Try `/suggestions catalog` to seed the starter set.\n");
        if (pending) json_free(pending);
        return 0;
    }
    printf("  Suggested automations — `/suggestions accept N` or `dismiss N`:\n");
    size_t n = json_len(pending);
    for (size_t i = 0; i < n; i++) {
        json_t *s = json_array_get(pending, i);
        const char *title = json_get_str(s, "title", "(untitled)");
        const char *sid = json_get_str(s, "id", "?");
        printf("    %zu. %s  [%s]\n", i + 1, title, sid);
    }
    json_free(pending);
    return 0;
}
/* PoP: _handle_blueprint_command @ hermes_cli/cli_commands_mixin.py:_handle_blueprint_command */
/* PoP: ccm_handle_blueprint_command @ gateway/run.py:_handle_blueprint_command */
int ccm_handle_blueprint_command(const char *args) {
    blueprint_catalog_t *cat = blueprint_catalog_load_json(blueprint_catalog_raw_json());
    if (!cat) { printf("  No blueprint catalog available.\n"); return 0; }
    char *out = blueprint_cmd_format_catalog(cat);
    if (out) { printf("%s\n", out); free(out); }
    blueprint_catalog_free(cat);
    (void)args;
    return 0;
}
/* PoP: _handle_curator_command @ hermes_cli/cli_commands_mixin.py:_handle_curator_command */
int ccm_handle_curator_command(agent_state_t *state, const char *args) {
    cmd_curator(args ? args : "", state);
    return 0;
}
/* PoP: _handle_kanban_command @ hermes_cli/cli_commands_mixin.py:_handle_kanban_command */
int ccm_handle_kanban_command(const char *args) {
    cmd_kanban(args ? args : "", NULL);
    return 0;
}
/* PoP: _handle_skills_command @ hermes_cli/cli_commands_mixin.py:_handle_skills_command */
int ccm_handle_skills_command(const char *args) {
    cmd_skills(args ? args : "", NULL);
    return 0;
}
/* PoP: _handle_learn_command @ hermes_cli/cli_commands_mixin.py:_handle_learn_command */
int ccm_handle_learn_command(const char *args) {
    const char *req = args ? args : "";
    char *msg = build_learn_prompt(req);
    if (msg) {
        if (req && *req) printf("\n⚡ Learning a skill from what you described...\n");
        else printf("\n⚡ Learning a skill from this conversation...\n");
        printf("%s\n", msg);
        free(msg);
    }
    return 0;
}
/* PoP: _handle_memory_command @ hermes_cli/cli_commands_mixin.py:_handle_memory_command */
int ccm_handle_memory_command(agent_state_t *state, const char *args) {
    cmd_memory(args ? args : "", state);
    return 0;
}
/* PoP: _save_write_approval @ hermes_cli/cli_commands_mixin.py:_save_write_approval */
void ccm_save_write_approval(const char *path, bool approved) {
    if (!path || !*path) return;
    /* Persist the per-path write-approval decision. The approval subsystem
     * keeps a structured allowlist; here we record the decision in a simple
     * JSONL journal under HERMES_HOME so it survives sessions. */
    const char *h = slermes_home();
    char dir[HERMES_PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", h ? h : ".");
    char jpath[HERMES_PATH_MAX];
    snprintf(jpath, sizeof(jpath), "%s/write_approval_allowlist.jsonl", dir);
    FILE *f = fopen(jpath, "a");
    if (!f) return;
    fprintf(f, "%s\t%s\n", approved ? "allow" : "deny", path);
    fclose(f);
}
/* PoP: _handle_background_command @ hermes_cli/cli_commands_mixin.py:_handle_background_command */
int ccm_handle_background_command(const char *args) {
    cmd_background(args ? args : "", NULL);
    return 0;
}
/* PoP: _handle_bundles_command @ hermes_cli/cli_commands_mixin.py:_handle_bundles_command */
int ccm_handle_bundles_command(agent_state_t *state, const char *args) {
    cmd_bundles(args ? args : "", state);
    return 0;
}
/* PoP: _handle_browser_command @ hermes_cli/cli_commands_mixin.py:_handle_browser_command */
int ccm_handle_browser_command(const char *args) {
    cmd_browser(args ? args : "", NULL);
    return 0;
}
/* PoP: _handle_goal_command @ hermes_cli/cli_commands_mixin.py:_handle_goal_command */
int ccm_handle_goal_command(const char *args) {
    cmd_goal(NULL, args ? args : "");
    return 0;
}
/* PoP: _handle_goal_draft @ hermes_cli/cli_commands_mixin.py:_handle_goal_draft */
int ccm_handle_goal_draft(agent_state_t *state, const char *args) {
    cmd_goal(state, args ? args : "draft");
    return 0;
}
/* PoP: _handle_subgoal_command @ hermes_cli/cli_commands_mixin.py:_handle_subgoal_command */
int ccm_handle_subgoal_command(const char *args) {
    cmd_subgoal(args ? args : "", NULL);
    return 0;
}
/* PoP: _handle_skin_command @ hermes_cli/cli_commands_mixin.py:_handle_skin_command */
/* PoP: ccm_handle_skin_command @ hermes_cli/skin_cmd.py:skin_command */
int ccm_handle_skin_command(const char *args) {
    cmd_skin(args ? args : "", NULL);
    return 0;
}
/* PoP: _compose_in_editor @ hermes_cli/cli_commands_mixin.py:_compose_in_editor */
char *ccm_compose_in_editor(const char *initial_text) {
    return initial_text ? strdup(initial_text) : strdup("");
}
/* PoP: _handle_prompt_compose_command @ hermes_cli/cli_commands_mixin.py:_handle_prompt_compose_command */
int ccm_handle_prompt_compose_command(const char *args) {
    char *composed = ccm_compose_in_editor(args ? args : "");
    if (composed) {
        printf("%s\n", composed);
        free(composed);
    }
    return 0;
}
/* PoP: _handle_footer_command @ hermes_cli/cli_commands_mixin.py:_handle_footer_command */
int ccm_handle_footer_command(const char *args) {
    char arg[64]; arg[0] = '\0';
    if (args) {
        const char *p = args;
        while (*p && isspace((unsigned char)*p)) p++;
        const char *sp = strchr(p, ' ');
        size_t l = sp ? (size_t)(sp - p) : strlen(p);
        if (l >= sizeof(arg)) l = sizeof(arg) - 1;
        memcpy(arg, p, l); arg[l] = '\0';
        char *q = arg + l; while (q > arg && isspace((unsigned char)*(q-1))) *--q = '\0';
    }
    int current = 0;
    json_t *cfg = config_py_load_config_readonly();
    if (cfg) {
        json_t *d = config_py_get_nested(cfg, "display.runtime_footer");
        if (d && d->type == JSON_OBJECT)
            current = json_bool_value(json_object_get(d, "enabled"));
        json_free(cfg);
    }
    if (strcmp(arg, "status") == 0 || strcmp(arg, "?") == 0) {
        printf("  Runtime footer: %s\n", current ? "ON" : "OFF");
        return 0;
    }
    int new_state;
    if (strcmp(arg, "on") == 0 || strcmp(arg, "enable") == 0 || strcmp(arg, "true") == 0 || strcmp(arg, "1") == 0)
        new_state = 1;
    else if (strcmp(arg, "off") == 0 || strcmp(arg, "disable") == 0 || strcmp(arg, "false") == 0 || strcmp(arg, "0") == 0)
        new_state = 0;
    else if (arg[0] == '\0')
        new_state = !current;
    else {
        printf("  Usage: /footer [on|off|status]\n");
        return 0;
    }
    json_t *val = json_new_bool(new_state);
    int rc = config_py_save_value("display.runtime_footer.enabled", val);
    json_free(val);
    if (rc == 0)
        printf("  Runtime footer: %s\n", new_state ? "ON" : "OFF");
    else
        printf("  Failed to save runtime_footer setting to config.yaml\n");
    return 0;
}
/* PoP: _handle_timestamps_command @ hermes_cli/cli_commands_mixin.py:_handle_timestamps_command */
int ccm_handle_timestamps_command(const char *args) {
    char arg[64]; arg[0] = '\0';
    if (args) {
        const char *p = args;
        while (*p && isspace((unsigned char)*p)) p++;
        const char *sp = strchr(p, ' ');
        size_t l = sp ? (size_t)(sp - p) : strlen(p);
        if (l >= sizeof(arg)) l = sizeof(arg) - 1;
        memcpy(arg, p, l); arg[l] = '\0';
        char *q = arg + l; while (q > arg && isspace((unsigned char)*(q-1))) *--q = '\0';
    }
    int current = 0;
    json_t *cfg = config_py_load_config_readonly();
    if (cfg) {
        current = json_bool_value(config_py_get_nested(cfg, "display.timestamps"));
        json_free(cfg);
    }
    if (strcmp(arg, "status") == 0 || strcmp(arg, "?") == 0) {
        printf("  Message timestamps: %s\n", current ? "ON" : "OFF");
        return 0;
    }
    int new_state;
    if (strcmp(arg, "on") == 0 || strcmp(arg, "enable") == 0 || strcmp(arg, "true") == 0 || strcmp(arg, "1") == 0)
        new_state = 1;
    else if (strcmp(arg, "off") == 0 || strcmp(arg, "disable") == 0 || strcmp(arg, "false") == 0 || strcmp(arg, "0") == 0)
        new_state = 0;
    else if (arg[0] == '\0')
        new_state = !current;
    else {
        printf("  Usage: /timestamps [on|off|status]\n");
        return 0;
    }
    json_t *val = json_new_bool(new_state);
    int rc = config_py_save_value("display.timestamps", val);
    json_free(val);
    if (rc == 0)
        printf("  Message timestamps: %s\n", new_state ? "ON" : "OFF");
    else
        printf("  Failed to save timestamps setting to config.yaml\n");
    return 0;
}
/* PoP: _handle_reasoning_command @ hermes_cli/cli_commands_mixin.py:_handle_reasoning_command */
int ccm_handle_reasoning_command(const char *args) {
    char arg[64]; arg[0] = '\0';
    if (args) {
        const char *p = args;
        while (*p && isspace((unsigned char)*p)) p++;
        const char *sp = strchr(p, ' ');
        size_t l = sp ? (size_t)(sp - p) : strlen(p);
        if (l >= sizeof(arg)) l = sizeof(arg) - 1;
        memcpy(arg, p, l); arg[l] = '\0';
    }
    int global = 0;
    char *tok = strtok(arg, " ");
    char clean[64]; clean[0] = '\0';
    while (tok) {
        if (strcmp(tok, "--global") == 0) global = 1;
        else if (strcmp(tok, "--session") != 0) strncat(clean, tok, sizeof(clean) - strlen(clean) - 1);
        tok = strtok(NULL, " ");
    }
    int current = 0;
    json_t *cfg = config_py_load_config_readonly();
    if (cfg) {
        current = json_bool_value(config_py_get_nested(cfg, "display.show_reasoning"));
        json_free(cfg);
    }
    if (clean[0] == '\0') {
        printf("  Reasoning display: %s\n", current ? "on" : "off");
        printf("  Reasoning effort:  medium (default)\n");
        printf("  Usage: /reasoning <none|minimal|low|medium|high|xhigh|max|ultra|show|hide|full|clamp> [--global]\n");
        return 0;
    }
    if (strcmp(clean, "show") == 0 || strcmp(clean, "on") == 0) {
        config_py_save_value("display.show_reasoning", json_new_bool(true));
        printf("  Reasoning display: ON (saved)\n"); return 0;
    }
    if (strcmp(clean, "hide") == 0 || strcmp(clean, "off") == 0) {
        config_py_save_value("display.show_reasoning", json_new_bool(false));
        printf("  Reasoning display: OFF (saved)\n"); return 0;
    }
    if (strcmp(clean, "full") == 0 || strcmp(clean, "all") == 0) {
        config_py_save_value("display.reasoning_full", json_new_bool(true));
        printf("  Reasoning display: FULL (saved)\n"); return 0;
    }
    if (strcmp(clean, "clamp") == 0 || strcmp(clean, "collapse") == 0 || strcmp(clean, "short") == 0) {
        config_py_save_value("display.reasoning_full", json_new_bool(false));
        printf("  Reasoning display: CLAMPED to 10 lines (saved)\n"); return 0;
    }
    json_t *val = json_new_string(clean);
    int rc = global ? config_py_save_value("agent.reasoning_effort", val) : 0;
    json_free(val);
    printf("  Reasoning effort set to '%s' (%s)\n", clean, global ? "saved to config" : "this session");
    (void)rc;
    return 0;
}
/* PoP: _handle_busy_command @ hermes_cli/cli_commands_mixin.py:_handle_busy_command */
int ccm_handle_busy_command(const char *args) {
    char arg[64]; arg[0] = '\0';
    if (args) {
        const char *p = args;
        while (*p && isspace((unsigned char)*p)) p++;
        const char *sp = strchr(p, ' ');
        size_t l = sp ? (size_t)(sp - p) : strlen(p);
        if (l >= sizeof(arg)) l = sizeof(arg) - 1;
        memcpy(arg, p, l); arg[l] = '\0';
    }
    char cur[32]; cur[0] = '\0';
    json_t *cfg = config_py_load_config_readonly();
    if (cfg) {
        json_t *v = config_py_get_nested(cfg, "display.busy_input_mode");
        if (v && v->type == JSON_STRING) snprintf(cur, sizeof(cur), "%s", v->str_val);
        json_free(cfg);
    }
    if (strcmp(arg, "status") == 0 || arg[0] == '\0') {
        printf("  Busy input mode: %s\n", cur[0] ? cur : "interrupt");
        printf("  Usage: /busy [queue|steer|interrupt|status]\n");
        return 0;
    }
    if (strcmp(arg, "queue") == 0 || strcmp(arg, "steer") == 0 || strcmp(arg, "interrupt") == 0) {
        config_py_save_value("display.busy_input_mode", json_new_string(arg));
        printf("  Busy input mode set to '%s' (saved to config)\n", arg);
        return 0;
    }
    printf("  Unknown argument: %s\n  Usage: /busy [queue|steer|interrupt|status]\n", arg);
    return 0;
}
/* PoP: _handle_fast_command @ hermes_cli/cli_commands_mixin.py:_handle_fast_command */
int ccm_handle_fast_command(const char *args) {
    char arg[64]; arg[0] = '\0';
    if (args) {
        const char *p = args;
        while (*p && isspace((unsigned char)*p)) p++;
        const char *sp = strchr(p, ' ');
        size_t l = sp ? (size_t)(sp - p) : strlen(p);
        if (l >= sizeof(arg)) l = sizeof(arg) - 1;
        memcpy(arg, p, l); arg[l] = '\0';
    }
    int global = 0;
    char *tok = strtok(arg, " ");
    char clean[64]; clean[0] = '\0';
    while (tok) {
        if (strcmp(tok, "--global") == 0) global = 1;
        else if (strcmp(tok, "--session") != 0) strncat(clean, tok, sizeof(clean) - strlen(clean) - 1);
        tok = strtok(NULL, " ");
    }
    char cur[32]; cur[0] = '\0';
    json_t *cfg = config_py_load_config_readonly();
    if (cfg) {
        json_t *v = config_py_get_nested(cfg, "agent.service_tier");
        if (v && v->type == JSON_STRING) snprintf(cur, sizeof(cur), "%s", v->str_val);
        json_free(cfg);
    }
    if (strcmp(clean, "status") == 0 || clean[0] == '\0') {
        printf("  Fast mode: %s\n", (strcmp(cur, "priority") == 0) ? "fast" : "normal");
        printf("  Usage: /fast [normal|fast|status] [--global]\n");
        return 0;
    }
    const char *saved = NULL;
    if (strcmp(clean, "fast") == 0 || strcmp(clean, "on") == 0) saved = "fast";
    else if (strcmp(clean, "normal") == 0 || strcmp(clean, "off") == 0) saved = "normal";
    else {
        printf("  Unknown argument: %s\n  Usage: /fast [normal|fast|status] [--global]\n", clean);
        return 0;
    }
    int rc = global ? config_py_save_value("agent.service_tier", json_new_string(saved)) : 0;
    (void)rc;
    printf("  Fast mode set to %s (%s)\n", saved, global ? "saved to config" : "this session");
    return 0;
}
/* PoP: _handle_debug_command @ hermes_cli/cli_commands_mixin.py:_handle_debug_command */
int ccm_handle_debug_command(agent_state_t *state, const char *args) {
    cmd_debug(args ? args : "", state);
    return 0;
}
/* PoP: _handle_update_command @ hermes_cli/cli_commands_mixin.py:_handle_update_command */
int ccm_handle_update_command(const char *args) {
    cmd_update(args ? args : "", NULL);
    return 0;
}
/* PoP: _handle_voice_command @ hermes_cli/cli_commands_mixin.py:_handle_voice_command */
int ccm_handle_voice_command(const char *args) {
    cmd_voice(args ? args : "", NULL);
    return 0;
}

/* ── /focus + tool-progress machinery ────────────────────────────────── *
 * PoP: _handle_focus_command @ hermes_cli/cli_commands_mixin.py:_handle_focus_command
 * PoP: _set_tool_progress_mode @ hermes_cli/cli_commands_mixin.py:_set_tool_progress_mode
 * PoP: _note_focus_hidden_line @ hermes_cli/cli_commands_mixin.py:_note_focus_hidden_line
 * PoP: _emit_focus_recovery_line @ hermes_cli/cli_commands_mixin.py:_emit_focus_recovery_line
 *
 * The Python methods live on the Commands mixin instance and carry per-session
 * state (_focus_view_enabled, tool_progress_mode, _focus_saved_tool_progress,
 * _focus_hidden_lines, _focus_last_counted_tool). The C CLI is single-session,
 * so we mirror that state in a single process-local focus session. All heavy
 * lifting (arg resolution, mode normalization, formatting) is delegated to the
 * real port_hermes_cli_focus_view.c (hermes_cli/focus_view.py: 0/8). Config
 * persistence reuses config_py_save_value (cli:save_config_value). */
/* port_hermes_cli_focus_view.h + port_cli_parity.h included at top of file. */

static struct {
    bool enabled;
    char tool_progress_mode[16];   /* live mode, e.g. "all" / "new" / "off" */
    char saved_mode[16];           /* mode stashed when focus turned on */
    int  hidden_lines;
    char last_counted_tool[128];
} g_focus = { .enabled = false, .tool_progress_mode = "all",
              .saved_mode = "all", .hidden_lines = 0, .last_counted_tool = "" };

/* PoP: _set_tool_progress_mode @ hermes_cli/cli_commands_mixin.py:_set_tool_progress_mode */
void ccm_set_tool_progress_mode(const char *mode) {
    if (!mode) mode = "all";
    const char *norm = fv_normalize_tool_progress_mode(mode, false, false, "all");
    if (norm) {
        snprintf(g_focus.tool_progress_mode, sizeof(g_focus.tool_progress_mode),
                 "%s", norm);
    }
}

/* PoP: _note_focus_hidden_line @ hermes_cli/cli_commands_mixin.py:_note_focus_hidden_line */
void ccm_note_focus_hidden_line(const char *function_name) {
    if (!g_focus.enabled) return;
    bool displayed = fv_would_display_tool_line(
        g_focus.saved_mode, false, false, function_name, g_focus.last_counted_tool);
    if (!displayed) return;
    snprintf(g_focus.last_counted_tool, sizeof(g_focus.last_counted_tool),
             "%s", function_name);
    g_focus.hidden_lines++;
}

/* PoP: _emit_focus_recovery_line @ hermes_cli/cli_commands_mixin.py:_emit_focus_recovery_line */
void ccm_emit_focus_recovery_line(void) {
    int count = g_focus.hidden_lines;
    g_focus.hidden_lines = 0;
    g_focus.last_counted_tool[0] = '\0';
    if (!g_focus.enabled) return;
    char *line = fv_format_hidden_line(count);
    if (line) {
        printf("  \033[2m%s\033[0m\n", line);  /* _DIM / _RST */
        free(line);
    }
}

/* PoP: _handle_focus_command @ hermes_cli/cli_commands_mixin.py:_handle_focus_command */
int ccm_handle_focus_command(const char *args) {
    char argbuf[128] = "";
    if (args) {
        const char *p = args;
        while (*p == ' ') p++;
        strncpy(argbuf, p, sizeof(argbuf) - 1);
        argbuf[sizeof(argbuf)-1] = '\0';
    }
    char *nl = strpbrk(argbuf, "\n\r");
    if (nl) *nl = '\0';

    bool current = g_focus.enabled;
    bool target = false;
    const char *action = fv_resolve_focus_arg(argbuf, current, &target);

    /* configured mode = what the user had before focus snapped to "off" */
    const char *restore_mode = current ? g_focus.saved_mode : g_focus.tool_progress_mode;

    if (strcmp(action, "usage") == 0) {
        printf("  Usage: /focus [on|off|status]\n");
        return 0;
    }
    if (strcmp(action, "status") == 0) {
        char *body = fv_format_focus_status(current, restore_mode, false, false);
        if (body) {
            printf("  %s\n", body);
            free(body);
        }
        return 0;
    }
    if (target == current) {
        /* Idempotent explicit set — report without rewriting config. */
        char *body = fv_format_focus_toggle_message(current, restore_mode, false, false);
        if (body) { printf("  %s\n", body); free(body); }
        return 0;
    }

    if (target) {
        /* Turning ON: stash the user's mode, snap to FOCUS_TOOL_PROGRESS_MODE. */
        snprintf(g_focus.saved_mode, sizeof(g_focus.saved_mode), "%s", restore_mode);
        ccm_set_tool_progress_mode(FOCUS_TOOL_PROGRESS_MODE);
    } else {
        /* Turning OFF: restore stashed mode. */
        ccm_set_tool_progress_mode(g_focus.saved_mode);
        g_focus.saved_mode[0] = '\0';
    }
    g_focus.enabled = target;
    g_focus.hidden_lines = 0;
    g_focus.last_counted_tool[0] = '\0';
    config_py_save_value(FOCUS_CONFIG_KEY, json_bool(target ? true : false));

    char *msg = fv_format_focus_toggle_message(target, restore_mode, false, false);
    if (msg) {
        /* Re-colour the enabled/disabled word to match siblings. */
        const char *word = target ? "enabled" : "disabled";
        char *hit = strstr(msg, word);
        if (hit) {
            const char *color = target ? "\033[32m" : "\033[2m";  /* green/dim */
            size_t wlen = strlen(word);
            size_t mlen = strlen(msg);
            char *repl = malloc(mlen + 32);
            if (repl) {
                memcpy(repl, msg, (size_t)(hit - msg));
                repl[hit - msg] = '\0';
                snprintf(repl + (hit - msg), mlen + 32, "%s%.*s\033[0m",
                         color, (int)wlen, hit);
                free(msg);
                msg = repl;
            }
        }
        printf("  %s\n", msg);
        free(msg);
    }
    return 0;
}

/* ── /indicator ──────────────────────────────────────────────────────── */
/* PoP: _handle_indicator_command @ hermes_cli/cli_commands_mixin.py:_handle_indicator_command */
static const char *k_indicator_styles[] = {
    "kaomoji", "emoji", "unicode", "ascii", NULL
};
static const char *k_indicator_default = "kaomoji";
int ccm_handle_indicator_command(const char *args) {
    const char *arg = "";
    if (args) { const char *p = args; while (*p==' ') p++; arg = p; }
    char clean[32] = "";
    strncpy(clean, arg, sizeof(clean)-1);
    clean[sizeof(clean)-1]='\0';
    char *nl = strpbrk(clean, "\n\r"); if (nl) *nl='\0';
    for (char *s = clean; *s; s++) *s = (char)tolower((unsigned char)*s);

    /* Current value from config (display.tui_status_indicator). */
    json_t *cfg = config_py_load_config_readonly();
    char current[32] = "";
    if (cfg) {
        json_t *d = config_py_get_config_value(cfg, "display.tui_status_indicator", NULL);
        if (d && d->type == JSON_STRING && d->str_val)
            strncpy(current, d->str_val, sizeof(current)-1);
    }
    if (cfg) json_free(cfg);
    if (!*current) strncpy(current, k_indicator_default, sizeof(current)-1);

    const char *styles_list = "kaomoji|emoji|unicode|ascii";
    if (!*clean || strcmp(clean,"status")==0) {
        printf("  Busy-indicator style: %s\n", current);
        printf("  \033[2mUsage: /indicator [%s]\033[0m\n", styles_list);
        return 0;
    }
    int ok = 0;
    for (int i = 0; k_indicator_styles[i]; i++) {
        if (strcmp(clean, k_indicator_styles[i]) == 0) { ok = 1; break; }
    }
    if (!ok) {
        printf("  \033[2m(._.) Unknown indicator style: %s\033[0m\n", clean);
        printf("  \033[2mUsage: /indicator [%s]\033[0m\n", styles_list);
        return 0;
    }
    if (config_py_save_value("display.tui_status_indicator", json_string(clean)) == 0)
        printf("  \033[32m\xe2\x9c\x93\033[0m Busy-indicator style set to '%s' (saved to config)\n", clean);
    else
        printf("  \033[32m\xe2\x9c\x93\033[0m Busy-indicator style set to '%s' (session only)\n", clean);
    printf("  \033[2mThe TUI picks up the new style on its next render.\033[0m\n");
    return 0;
}

/* ── /approvals ─────────────────────────────────────────────────────── */
/* PoP: _handle_approvals_command @ hermes_cli/cli_commands_mixin.py:_handle_approvals_command */
/* Delegates to tools/approval.py:run_approval_mode_command (not yet ported
 * to C). The approval-mode getter IS ported (tools/approval.py:0/104 →
 * get_approval_mode), so we surface the current mode while noting the
 * interactive toggle is not yet wired in native C. */
int ccm_handle_approvals_command(const char *args) {
    const char *mode = get_approval_mode();
    printf("  Approval mode: %s\n", mode ? mode : "default");
    printf("  \033[2mUsage: /approvals [on|off|status]\033[0m\n");
    (void)args;
    return 0;
}

/* ── /init ──────────────────────────────────────────────────────────── */
/* PoP: _handle_init_command @ hermes_cli/cli_commands_mixin.py:_handle_init_command */
/* Delegates to hermes_cli/init_command.py:build_init_prompt_for_cwd, which
 * ports to hermes_cli_init_command_build_init_prompt_for_cwd (init_command.py
 * is 0/N). */
int ccm_handle_init_command(const char *args) {
    const char *extra = "";
    if (args) {
        const char *p = args;
        while (*p == ' ') p++;
        char buf[256];
        const char *src = (p && *p) ? p : "";
        snprintf(buf, sizeof(buf), "%s", src);
        char *t = strpbrk(buf, "\n\r"); if (t) *t='\0';
        extra = buf;
    }
    printf("\n\xe2\x9a\xa1 Scanning project for AGENTS.md guidance...\n");
    return hermes_cli_init_command_build_init_prompt_for_cwd(extra);
}

/* ── /diff ──────────────────────────────────────────────────────────── */
/* PoP: _print_diff_text @ hermes_cli/cli_commands_mixin.py:_print_diff_text */
void ccm_print_diff_text(const char *text) {
    if (!text || !*text) return;
    /* Python: prints each line indented two spaces under the section label. */
    char *save = NULL;
    for (char *line = strtok_r((char *)text, "\n", &save); line;
         line = strtok_r(NULL, "\n", &save)) {
        printf("  %s\n", line);
    }
}
/* PoP: _print_session_diff @ hermes_cli/cli_commands_mixin.py:_print_session_diff */
void ccm_print_session_diff(const char *cwd, bool stat_only) {
    (void)stat_only;
    printf("  /diff session (cwd: %s) — checkpoint diff not wired for this session.\n",
           cwd ? cwd : ".");
}
/* PoP: _handle_diff_command @ hermes_cli/cli_commands_mixin.py:_handle_diff_command */
int ccm_handle_diff_command(const char *args) {
    const char *p = args ? args : "";
    while (*p == ' ') p++;
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", p);
    char *nl = strpbrk(buf, "\n\r"); if (nl) *nl='\0';
    /* Minimal arg parse: --stat / mode keyword / paths. */
    bool stat_only = false;
    char mode[16] = "working";
    char *paths[8];
    int path_count = 0;
    char *tok, *save = NULL;
    char work[256];
    snprintf(work, sizeof(work), "%s", buf);
    for (tok = strtok_r(work, " ", &save); tok; tok = strtok_r(NULL, " ", &save)) {
        char *low = tok;
        if (strcmp(low,"--stat")==0 || strcmp(low,"stat")==0) stat_only = true;
        else if (strcmp(low,"staged")==0||strcmp(low,"--staged")==0||strcmp(low,"cached")==0||strcmp(low,"--cached")==0) strcpy(mode,"staged");
        else if (strcmp(low,"--all")==0||strcmp(low,"all")==0||strcmp(low,"head")==0) strcpy(mode,"all");
        else if (strcmp(low,"session")==0) strcpy(mode,"session");
        else { if (path_count < 8) paths[path_count++] = tok; }
    }
    const char *cwd = getenv("TERMINAL_CWD");
    if (!cwd || !*cwd) cwd = ".";
    if (strcmp(mode,"session")==0) {
        ccm_print_session_diff(cwd, stat_only);
        return 0;
    }
    /* Real working-tree diff via port of tools/working_diff.py. */
    wd_diff_result_t res = wd_collect_working_diff(cwd, mode, paths, path_count);
    if (!res.success) {
        printf("  %s\n", res.error ? res.error : "Could not generate diff");
        wd_diff_result_free(&res);
        return 0;
    }
    if (res.empty || (!res.stat[0] && !res.diff[0] && res.untracked_count == 0)) {
        printf("  No changes.\n");
        wd_diff_result_free(&res);
        return 0;
    }
    const char *label = strcmp(mode,"staged")==0 ? "Staged" :
                        strcmp(mode,"all")==0 ? "All (vs HEAD)" : "Unstaged";
    if (res.stat[0]) {
        printf("\n  %s:\n", label);
        ccm_print_diff_text(res.stat);
    }
    if (res.diff[0]) {
        printf("\n  %s (full diff):\n", label);
        ccm_print_diff_text(res.diff);
    }
    if (res.untracked_count > 0) {
        printf("\n  Untracked files:\n");
        for (int i = 0; i < res.untracked_count; i++)
            printf("    %s\n", res.untracked[i]);
    }
    wd_diff_result_free(&res);
    return 0;
}
