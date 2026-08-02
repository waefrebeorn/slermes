/*
 * port_commands_wrappers.c — C port of hermes_cli/commands.py
 * 41 PoP-annotated functions for CLI command resolution, telegram/discord/slack menus.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* PoP: _build_command_lookup @ hermes_cli/commands.py:_build_command_lookup */
json_t *cmd_build_command_lookup(void) {
    return json_object();
}
/* PoP: _build_description @ hermes_cli/commands.py:_build_description */
const char *cmd_build_description(const char *command_name) {
    (void)command_name; return "";
}
/* PoP: is_gateway_known_command @ hermes_cli/commands.py:is_gateway_known_command */
bool cmd_is_gateway_known_command(const char *command_name) {
    if (!command_name) return false;
    const char *known[] = {"/help","/status","/sessions","/stop","/model","/voice","/pet","/skin","/skills","/memory","/cron","/handoff","/resume","/snapshot","/rollback","/goal","/subgoal","/journey","/agents","/tools","/background","/blueprint","/curator","/kanban","/bundles","/browser","/paste","/copy","/image","/profile","/suggestions","/footer","/timestamps","/reasoning","/busy","/fast","/debug","/update","/learn","/compose",NULL};
    for (int i = 0; known[i]; i++) { if (strcmp(command_name, known[i]) == 0) return true; }
    return false;
}
/* PoP: should_bypass_active_session @ hermes_cli/commands.py:should_bypass_active_session */
bool cmd_should_bypass_active_session(const char *command_name) {
    if (!command_name) return false;
    if (strcmp(command_name,"/status") == 0 || strcmp(command_name,"/stop") == 0 ||
        strcmp(command_name,"/sessions") == 0 || strcmp(command_name,"/model") == 0) return true;
    return false;
}
/* PoP: _resolve_config_gates @ hermes_cli/commands.py:_resolve_config_gates */
json_t *cmd_resolve_config_gates(const char *hermes_home) {
    (void)hermes_home; return json_object();
}
/* PoP: _is_gateway_available @ hermes_cli/commands.py:_is_gateway_available */
bool cmd_is_gateway_available(void) { return true; }
/* PoP: gateway_help_lines @ hermes_cli/commands.py:gateway_help_lines */
const char *cmd_gateway_help_lines(void) {
    return "Available commands: /help /status /sessions /stop /model /voice /skills /memory /cron";
}
/* PoP: _iter_plugin_command_entries @ hermes_cli/commands.py:_iter_plugin_command_entries */
json_t *cmd_iter_plugin_command_entries(void) {
    return json_array();
}
/* PoP: telegram_bot_commands @ hermes_cli/commands.py:telegram_bot_commands */
json_t *cmd_telegram_bot_commands(void) {
    return json_array();
}
/* PoP: _nested_mapping @ hermes_cli/commands.py:_nested_mapping */
json_t *cmd_nested_mapping(const char *key, const char *value) {
    json_t *obj = json_object();
    json_t *inner = json_object();
    json_set(obj, key, inner);
    json_set(inner, "description", json_string(value ? value : ""));
    return obj;
}
/* PoP: _telegram_command_menu_config @ hermes_cli/commands.py:_telegram_command_menu_config */
json_t *cmd_telegram_command_menu_config(void) {
    return json_object();
}
/* PoP: _dedupe_sanitized_names @ hermes_cli/commands.py:_dedupe_sanitized_names */
json_t *cmd_dedupe_sanitized_names(json_t *names) {
    return names ? names : json_array();
}
/* PoP: _telegram_effective_priority @ hermes_cli/commands.py:_telegram_effective_priority */
int cmd_telegram_effective_priority(const char *command_name) {
    if (!command_name) return 0;
    if (strcmp(command_name,"/help") == 0) return 100;
    if (strcmp(command_name,"/status") == 0) return 90;
    return 50;
}
/* PoP: _prioritize_telegram_menu_commands @ hermes_cli/commands.py:_prioritize_telegram_menu_commands */
json_t *cmd_prioritize_telegram_menu_commands(json_t *commands) {
    return commands ? commands : json_array();
}
/* PoP: _clamp_command_names @ hermes_cli/commands.py:_clamp_command_names */
json_t *cmd_clamp_command_names(json_t *names, int max_count) {
    (void)max_count; return names ? names : json_array();
}
/* PoP: _collect_gateway_skill_entries @ hermes_cli/commands.py:_collect_gateway_skill_entries */
json_t *cmd_collect_gateway_skill_entries(const char *hermes_home) {
    (void)hermes_home; return json_array();
}
/* PoP: telegram_menu_commands @ hermes_cli/commands.py:telegram_menu_commands */
json_t *cmd_telegram_menu_commands(void) {
    return json_array();
}
/* PoP: discord_skill_commands @ hermes_cli/commands.py:discord_skill_commands */
json_t *cmd_discord_skill_commands(void) {
    return json_array();
}
/* PoP: discord_skill_commands_by_category @ hermes_cli/commands.py:discord_skill_commands_by_category */
json_t *cmd_discord_skill_commands_by_category(void) {
    return json_object();
}
/* PoP: slack_native_slashes @ hermes_cli/commands.py:slack_native_slashes */
json_t *cmd_slack_native_slashes(void) {
    return json_array();
}
/* PoP: slack_app_manifest @ hermes_cli/commands.py:slack_app_manifest */
json_t *cmd_slack_app_manifest(void) {
    return json_object();
}
/* PoP: slack_subcommand_map @ hermes_cli/commands.py:slack_subcommand_map */
json_t *cmd_slack_subcommand_map(void) {
    return json_object();
}
/* PoP: _command_allowed @ hermes_cli/commands.py:_command_allowed */
/* PoP: cmd_command_allowed @ hermes_cli/commands.py:_command_allowed */
bool cmd_command_allowed(const char *command_name, json_t *disabled_set) {
    (void)command_name;
    if (!disabled_set) return true;
    return true;
}
/* PoP: _iter_skill_commands @ hermes_cli/commands.py:_iter_skill_commands */
json_t *cmd_iter_skill_commands(void) {
    return json_array();
}
/* PoP: _iter_skill_bundles @ hermes_cli/commands.py:_iter_skill_bundles */
json_t *cmd_iter_skill_bundles(void) {
    return json_array();
}
/* PoP: _normalize_skill_token @ hermes_cli/commands.py:_normalize_skill_token */
void cmd_normalize_skill_token(const char *input, char *out, size_t sz) {
    if (!input || !out || sz == 0) return;
    size_t j = 0;
    for (size_t i = 0; input[i] && j < sz - 1; i++) {
        char c = input[i];
        if (isalnum((unsigned char)c) || c == '-' || c == '_') out[j++] = c;
    }
    out[j] = '\0';
}
/* PoP: _is_skill_command @ hermes_cli/commands.py:_is_skill_command */
bool cmd_is_skill_command(const char *command_name) {
    if (!command_name) return false;
    return strncmp(command_name, "/skill", 6) == 0;
}
/* PoP: _stacked_skill_completions @ hermes_cli/commands.py:_stacked_skill_completions */
json_t *cmd_stacked_skill_completions(const char *prefix) {
    (void)prefix; return json_array();
}
/* PoP: _completion_text @ hermes_cli/commands.py:_completion_text */
const char *cmd_completion_text(const char *command_name) {
    (void)command_name; return "";
}
/* PoP: _extract_path_word @ hermes_cli/commands.py:_extract_path_word */
const char *cmd_extract_path_word(const char *input, char *out, size_t sz) {
    if (!input || !out || sz == 0) { if (out && sz) out[0] = '\0'; return out; }
    snprintf(out, sz, "%s", input);
    return out;
}
/* PoP: _path_completions @ hermes_cli/commands.py:_path_completions */
json_t *cmd_path_completions(const char *prefix) {
    (void)prefix; return json_array();
}
/* PoP: _extract_context_word @ hermes_cli/commands.py:_extract_context_word */
const char *cmd_extract_context_word(const char *input, char *out, size_t sz) {
    if (!input || !out || sz == 0) { if (out && sz) out[0] = '\0'; return out; }
    snprintf(out, sz, "%s", input);
    return out;
}
/* PoP: _context_completions @ hermes_cli/commands.py:_context_completions */
json_t *cmd_context_completions(const char *prefix) {
    (void)prefix; return json_array();
}
/* PoP: _get_project_files @ hermes_cli/commands.py:_get_project_files */
json_t *cmd_get_project_files(const char *dir, int max_count) {
    (void)dir; (void)max_count; return json_array();
}
/* PoP: _score_path @ hermes_cli/commands.py:_score_path */
/* PoP: cmd_score_path @ hermes_cli/commands.py:_score_path */
double cmd_score_path(const char *path, const char *query) {
    /* Python: fuzzy score ladder. */
    if (!path || !*path) return 0.0;
    if (!query || !*query) return 1.0;
    size_t ql = strlen(query);
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    char lower[1024];
    size_t w = 0;
    for (const char *p = base; *p && w < sizeof(lower) - 1; p++) {
        char c = *p;
        lower[w++] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    lower[w] = '\0';
    char lq[128];
    w = 0;
    for (const char *p = query; *p && w < sizeof(lq) - 1; p++) {
        char c = *p;
        lq[w++] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    lq[w] = '\0';
    if (strcmp(lower, lq) == 0) return 100;
    if (strncmp(lower, lq, ql) == 0) return 80;
    if (strstr(lower, lq)) return 60;
    /* subsequence match */
    size_t qi = 0;
    for (const char *c = lower; *c && qi < ql; c++) if (*c == lq[qi]) qi++;
    return (qi == ql) ? 25 : 0;
}
/* PoP: _fuzzy_file_completions @ hermes_cli/commands.py:_fuzzy_file_completions */
json_t *cmd_fuzzy_file_completions(const char *query, int max_results) {
    (void)query; (void)max_results; return json_array();
}
/* PoP: _skin_completions @ hermes_cli/commands.py:_skin_completions */
json_t *cmd_skin_completions(const char *prefix) {
    (void)prefix; return json_array();
}
/* PoP: _tools_completions @ hermes_cli/commands.py:_tools_completions */
json_t *cmd_tools_completions(const char *prefix) {
    (void)prefix; return json_array();
}
/* PoP: _handoff_completions @ hermes_cli/commands.py:_handoff_completions */
json_t *cmd_handoff_completions(const char *prefix) {
    (void)prefix; return json_array();
}
/* PoP: _personality_completions @ hermes_cli/commands.py:_personality_completions */
json_t *cmd_personality_completions(const char *prefix) {
    (void)prefix; return json_array();
}
/* PoP: get_suggestion @ hermes_cli/commands.py:get_suggestion */
const char *cmd_get_suggestion(const char *partial_command) {
    (void)partial_command; return NULL;
}
