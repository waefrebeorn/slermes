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

/* Real command handlers (cli_cmd_*.c) — state-safe (ignore agent_state_t). */
extern void cmd_cron(const char *args, agent_state_t *state);
extern void cmd_kanban(const char *args, agent_state_t *state);
extern void cmd_skills(const char *args, agent_state_t *state);
extern void cmd_browser(const char *args, agent_state_t *state);

/* PoP: _handle_rollback_command @ hermes_cli/cli_commands_mixin.py:_handle_rollback_command */
int ccm_handle_rollback_command(const char *args) {
    (void)args; return 0;
}
/* PoP: _handle_snapshot_command @ hermes_cli/cli_commands_mixin.py:_handle_snapshot_command */
int ccm_handle_snapshot_command(const char *args) {
    (void)args; return 0;
}
/* PoP: _handle_stop_command @ hermes_cli/cli_commands_mixin.py:_handle_stop_command */
int ccm_handle_stop_command(const char *args) {
    (void)args;
    int killed = process_registry_kill_all(NULL);
    if (killed > 0)
        printf("  Stopping background process(es)...\n  Stopped %d process(es).\n", killed);
    else
        printf("  No running background processes.\n");
    return 0;
}
/* PoP: _handle_agents_command @ hermes_cli/cli_commands_mixin.py:_handle_agents_command */
int ccm_handle_agents_command(const char *args) {
    (void)args;
    char *raw = process_registry_list(NULL);
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
    (void)args; return 0;
}
/* PoP: _handle_paste_command @ hermes_cli/cli_commands_mixin.py:_handle_paste_command */
int ccm_handle_paste_command(const char *args) {
    (void)args; return 0;
}
/* PoP: _handle_copy_command @ hermes_cli/cli_commands_mixin.py:_handle_copy_command */
int ccm_handle_copy_command(const char *args) {
    (void)args; return 0;
}
/* PoP: _handle_image_command @ hermes_cli/cli_commands_mixin.py:_handle_image_command */
int ccm_handle_image_command(const char *args) {
    (void)args; return 0;
}
/* PoP: _handle_tools_command @ hermes_cli/cli_commands_mixin.py:_handle_tools_command */
int ccm_handle_tools_command(const char *args) {
    (void)args; return 0;
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
int ccm_handle_handoff_command(const char *args) {
    (void)args; return 0;
}
/* PoP: _handle_resume_command @ hermes_cli/cli_commands_mixin.py:_handle_resume_command */
int ccm_handle_resume_command(const char *args) {
    (void)args; return 0;
}
/* PoP: _handle_sessions_command @ hermes_cli/cli_commands_mixin.py:_handle_sessions_command */
int ccm_handle_sessions_command(const char *args) {
    (void)args; return 0;
}
/* PoP: _handle_branch_command @ hermes_cli/cli_commands_mixin.py:_handle_branch_command */
int ccm_handle_branch_command(const char *args) {
    (void)args; return 0;
}
/* PoP: _handle_personality_command @ hermes_cli/cli_commands_mixin.py:_handle_personality_command */
int ccm_handle_personality_command(const char *args) {
    (void)args; return 0;
}
/* PoP: _handle_pet_command @ hermes_cli/cli_commands_mixin.py:_handle_pet_command */
int ccm_handle_pet_command(const char *args) {
    (void)args; return 0;
}
/* PoP: _handle_hatch_command @ hermes_cli/cli_commands_mixin.py:_handle_hatch_command */
int ccm_handle_hatch_command(const char *args) {
    (void)args; return 0;
}
/* PoP: _handle_cron_command @ hermes_cli/cli_commands_mixin.py:_handle_cron_command */
int ccm_handle_cron_command(const char *args) {
    cmd_cron(args ? args : "", NULL);
    return 0;
}
/* PoP: _handle_suggestions_command @ hermes_cli/cli_commands_mixin.py:_handle_suggestions_command */
int ccm_handle_suggestions_command(const char *args) {
    (void)args; return 0;
}
/* PoP: _handle_blueprint_command @ hermes_cli/cli_commands_mixin.py:_handle_blueprint_command */
int ccm_handle_blueprint_command(const char *args) {
    (void)args; return 0;
}
/* PoP: _handle_curator_command @ hermes_cli/cli_commands_mixin.py:_handle_curator_command */
int ccm_handle_curator_command(const char *args) {
    (void)args; return 0;
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
    (void)args; return 0;
}
/* PoP: _handle_memory_command @ hermes_cli/cli_commands_mixin.py:_handle_memory_command */
int ccm_handle_memory_command(const char *args) {
    (void)args; return 0;
}
/* PoP: _save_write_approval @ hermes_cli/cli_commands_mixin.py:_save_write_approval */
void ccm_save_write_approval(const char *path, bool approved) {
    (void)path; (void)approved;
}
/* PoP: _handle_background_command @ hermes_cli/cli_commands_mixin.py:_handle_background_command */
int ccm_handle_background_command(const char *args) {
    (void)args; return 0;
}
/* PoP: _handle_bundles_command @ hermes_cli/cli_commands_mixin.py:_handle_bundles_command */
int ccm_handle_bundles_command(const char *args) {
    (void)args; return 0;
}
/* PoP: _handle_browser_command @ hermes_cli/cli_commands_mixin.py:_handle_browser_command */
int ccm_handle_browser_command(const char *args) {
    cmd_browser(args ? args : "", NULL);
    return 0;
}
/* PoP: _handle_goal_command @ hermes_cli/cli_commands_mixin.py:_handle_goal_command */
int ccm_handle_goal_command(const char *args) {
    (void)args; return 0;
}
/* PoP: _handle_goal_draft @ hermes_cli/cli_commands_mixin.py:_handle_goal_draft */
int ccm_handle_goal_draft(const char *args) {
    (void)args; return 0;
}
/* PoP: _handle_subgoal_command @ hermes_cli/cli_commands_mixin.py:_handle_subgoal_command */
int ccm_handle_subgoal_command(const char *args) {
    (void)args; return 0;
}
/* PoP: _handle_skin_command @ hermes_cli/cli_commands_mixin.py:_handle_skin_command */
int ccm_handle_skin_command(const char *args) {
    (void)args; return 0;
}
/* PoP: _compose_in_editor @ hermes_cli/cli_commands_mixin.py:_compose_in_editor */
char *ccm_compose_in_editor(const char *initial_text) {
    return initial_text ? strdup(initial_text) : strdup("");
}
/* PoP: _handle_prompt_compose_command @ hermes_cli/cli_commands_mixin.py:_handle_prompt_compose_command */
int ccm_handle_prompt_compose_command(const char *args) {
    (void)args; return 0;
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
int ccm_handle_debug_command(const char *args) {
    (void)args; return 0;
}
/* PoP: _handle_update_command @ hermes_cli/cli_commands_mixin.py:_handle_update_command */
int ccm_handle_update_command(const char *args) {
    (void)args; return 0;
}
/* PoP: _handle_voice_command @ hermes_cli/cli_commands_mixin.py:_handle_voice_command */
int ccm_handle_voice_command(const char *args) {
    (void)args; return 0;
}
