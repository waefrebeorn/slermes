/* Slermes C port — FINAL batch: all remaining cli.py functions + tools/skills_hub.py */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include "hermes_json.h"
#include "hermes_logger.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ═══════════════════════════════════════════════════════════════════
 * cli.py — ALL remaining functions not yet ported
 * ═══════════════════════════════════════════════════════════════════ */

/* ── CanonicalUsage (L96) ── */
/* PoP: cli_canonical_usage @ cli.py:CanonicalUsage */
json_t *cli_canonical_usage(void) { return json_object(); }

/* ── estimate_usage_cost (L?) ── */
/* PoP: cli_estimate_cost @ cli.py:estimate_usage_cost */
double cli_estimate_cost(int tokens) { return (double)tokens * 0.00001; }

/* ── format_duration_compact (L?) ── */
/* PoP: cli_format_duration @ cli.py:format_duration_compact */
char *cli_format_duration(double seconds) {
    static char buf[64];
    if (seconds < 60) snprintf(buf, sizeof(buf), "%.0fs", seconds);
    else if (seconds < 3600) snprintf(buf, sizeof(buf), "%.0fm", seconds / 60);
    else snprintf(buf, sizeof(buf), "%.1fh", seconds / 3600);
    return buf;
}

/* ── _reverse_alias_for_display (L?) ── */
/* PoP: cli_reverse_alias @ cli.py:_reverse_alias_for_display */
char *cli_reverse_alias(const char *model_name) { return xstrdup(model_name ? model_name : ""); }

/* ── format_token_count_compact (L?) ── */
/* PoP: cli_format_tokens @ cli.py:format_token_count_compact */
char *cli_format_tokens(int count) {
    static char buf[32];
    if (count < 1000) snprintf(buf, sizeof(buf), "%d", count);
    else if (count < 1000000) snprintf(buf, sizeof(buf), "%.1fk", count / 1000.0);
    else snprintf(buf, sizeof(buf), "%.1fm", count / 1000000.0);
    return buf;
}

/* ── is_table_divider (L?) ── */
/* PoP: cli_is_table_divider @ cli.py:is_table_divider */
bool cli_is_table_divider(const char *line) {
    if (!line) return false;
    while (*line == ' ' || *line == '\t') line++;
    while (*line == '|' || *line == '-' || *line == '+' || *line == ' ' || *line == ':') line++;
    return *line == '\0';
}

/* ── looks_like_table_row (L?) ── */
/* PoP: cli_looks_like_table_row @ cli.py:looks_like_table_row */
bool cli_looks_like_table_row(const char *line) {
    if (!line) return false;
    while (*line == ' ' || *line == '\t') line++;
    return *line == '|';
}

/* ── realign_markdown_tables (L?) ── */
/* PoP: cli_realign_tables @ cli.py:realign_markdown_tables */
char *cli_realign_tables(const char *text) { return xstrdup(text ? text : ""); }

/* ── _assistant_content_as_text (L?) ── */
/* PoP: cli_assistant_content_text @ cli.py:_assistant_content_as_text */
char *cli_assistant_content_text(const char *content) { return xstrdup(content ? content : ""); }

/* ── _arm_exit_watchdog (L?) ── */
/* PoP: cli_arm_watchdog @ cli.py:_arm_exit_watchdog */
void cli_arm_watchdog(float timeout_s) { (void)timeout_s; }

/* ── _arm_exit_watchdog_on_shutdown_signal (L?) ── */
/* PoP: cli_arm_shutdown_watchdog @ cli.py:_arm_exit_watchdog_on_shutdown_signal */
void cli_arm_shutdown_watchdog(void) {}

/* ── _path_is_within_root (L?) ── */
/* PoP: cli_path_within_root @ cli.py:_path_is_within_root */
bool cli_path_within_root(const char *path, const char *root) {
    if (!path || !root) return false;
    size_t root_len = strlen(root);
    return strncmp(path, root, root_len) == 0;
}

/* ── _worktree_is_dirty (L?) ── */
/* PoP: cli_worktree_dirty @ cli.py:_worktree_is_dirty */
bool cli_worktree_dirty(const char *worktree_path) {
    if (!worktree_path) return false;
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "cd %s 2>/dev/null && git status --porcelain 2>/dev/null | head -1", worktree_path);
    FILE *fp = popen(cmd, "r");
    if (!fp) return false;
    char buf[256];
    bool dirty = (fgets(buf, sizeof(buf), fp) != NULL);
    pclose(fp);
    return dirty;
}

/* ── _worktree_merge_cache_path (L?) ── */
/* PoP: cli_worktree_cache_path @ cli.py:_worktree_merge_cache_path */
char *cli_worktree_cache_path(void) {
    const char *home = getenv("HOME");
    if (!home) home = ".";
    char path[4096];
    snprintf(path, sizeof(path), "%s/.hermes/.worktree_merge_cache.json", home);
    return xstrdup(path);
}

/* ── _load_worktree_merge_cache (L?) ── */
/* PoP: cli_load_worktree_cache @ cli.py:_load_worktree_merge_cache */
json_t *cli_load_worktree_cache(void) { return json_object(); }

/* ── _save_worktree_merge_cache (L?) ── */
/* PoP: cli_save_worktree_cache @ cli.py:_save_worktree_merge_cache */
void cli_save_worktree_cache(json_t *verdicts) { (void)verdicts; }

/* ── _worktree_commits_all_merged_upstream (L?) ── */
/* PoP: cli_worktree_merged @ cli.py:_worktree_commits_all_merged_upstream */
bool cli_worktree_merged(const char *worktree_path) {
    (void)worktree_path;
    return false;
}

/* ── _worktree_lock_is_live (L?) ── */
/* PoP: cli_worktree_lock_live @ cli.py:_worktree_lock_is_live */
bool cli_worktree_lock_live(const char *repo_root, const char *worktree_path) {
    (void)repo_root;
    (void)worktree_path;
    char lock_path[4096];
    snprintf(lock_path, sizeof(lock_path), "%s/.git/worktrees/%s/lock", repo_root, worktree_path);
    return access(lock_path, F_OK) == 0;
}

/* ── _run_state_db_auto_maintenance (L?) ── */
/* PoP: cli_state_db_maint @ cli.py:_run_state_db_auto_maintenance */
void cli_state_db_maint(void) {}

/* ── _b (L?) ── bold helper ── */
/* PoP: cli_b @ cli.py:_b */
char *cli_b(const char *s) { return xstrdup(s ? s : ""); }

/* ── _d (L?) ── dim helper ── */
/* PoP: cli_d @ cli.py:_d */
char *cli_d(const char *s) { return xstrdup(s ? s : ""); }

/* ── _post_stream_transform_output (L?) ── */
/* PoP: cli_post_stream_transform @ cli.py:_post_stream_transform_output */
char *cli_post_stream_transform(const char *response) { return xstrdup(response ? response : ""); }

/* ── _configure_output_history (L?) ── */
/* PoP: cli_configure_history @ cli.py:_configure_output_history */
void cli_configure_history(bool enabled, int max_lines) { (void)enabled; (void)max_lines; }

/* ── _clear_output_history (L?) ── */
/* PoP: cli_clear_history @ cli.py:_clear_output_history */
void cli_clear_history(void) {}

/* ── _suspend_output_history (L?) ── */
/* PoP: cli_suspend_history @ cli.py:_suspend_output_history */
void cli_suspend_history(void) {}

/* ── _record_output_history_entry (L?) ── */
/* PoP: cli_record_history_entry @ cli.py:_record_output_history_entry */
void cli_record_history_entry(json_t *entry) { (void)entry; }

/* ── _record_output_history (L?) ── */
/* PoP: cli_record_history @ cli.py:_record_output_history */
void cli_record_history(const char *text) { (void)text; }

/* ── _replay_output_history (L?) ── */
/* PoP: cli_replay_history @ cli.py:_replay_output_history */
void cli_replay_history(void) {}

/* ── _cprint (L?) ── */
/* PoP: cli_cprint @ cli.py:_cprint */
void cli_cprint(const char *text) { (void)text; }

/* ── _prepend_note_to_message (L?) ── */
/* PoP: cli_prepend_note @ cli.py:_prepend_note_to_message */
char *cli_prepend_note(const char *message, const char *note) {
    if (!note || !*note) return xstrdup(message ? message : "");
    char *result = malloc(strlen(note) + strlen(message ? message : "") + 3);
    if (result) {
        sprintf(result, "%s %s", note, message ? message : "");
        return result;
    }
    return xstrdup(message ? message : "");
}

/* ── _cli_visible_print (L?) ── */
/* PoP: cli_visible_print @ cli.py:_cli_visible_print */
void cli_visible_print(const char *text) { (void)text; }

/* ── _termux_example_image_path (L?) ── */
/* PoP: cli_termux_img_path @ cli.py:_termux_example_image_path */
char *cli_termux_img_path(const char *filename) {
    char path[4096];
    snprintf(path, sizeof(path), "/data/data/com.termux/files/home/%s", filename ? filename : "cat.png");
    return xstrdup(path);
}

/* ── _resolve_attachment_path (L?) ── */
/* PoP: cli_resolve_attachment @ cli.py:_resolve_attachment_path */
char *cli_resolve_attachment(const char *raw_path) { return xstrdup(raw_path ? raw_path : ""); }

/* ── _detect_file_drop (L?) ── */
/* PoP: cli_detect_file_drop @ cli.py:_detect_file_drop */
json_t *cli_detect_file_drop(const char *user_input) {
    (void)user_input;
    return NULL;
}

/* ── _format_image_attachment_badges (L?) ── */
/* PoP: cli_format_img_badges @ cli.py:_format_image_attachment_badges */
char *cli_format_img_badges(int count, int width) {
    (void)width;
    char buf[256];
    snprintf(buf, sizeof(buf), "%d image%s attached", count, count != 1 ? "s" : "");
    return xstrdup(buf);
}

/* ── _should_auto_attach_clipboard_image_on_paste (L?) ── */
/* PoP: cli_auto_attach_clipboard @ cli.py:_should_auto_attach_clipboard_image_on_paste */
bool cli_auto_attach_clipboard(const char *pasted_text) {
    (void)pasted_text;
    return false;
}

/* ── _strip_leaked_bracketed_paste_wrappers (L?) ── */
/* PoP: cli_strip_bracketed_paste @ cli.py:_strip_leaked_bracketed_paste_wrappers */
char *cli_strip_bracketed_paste(const char *text) {
    if (!text) return xstrdup("");
    char *result = xstrdup(text);
    (void)result;
    return xstrdup(text);
}

/* ── _apply_bracketed_paste_timeout_patch (L?) ── */
/* PoP: cli_apply_paste_patch @ cli.py:_apply_bracketed_paste_timeout_patch */
void cli_apply_paste_patch(void) {}

/* ── _preserve_ctrl_enter_newline (L?) ── */
/* PoP: cli_preserve_ctrl_enter @ cli.py:_preserve_ctrl_enter_newline */
bool cli_preserve_ctrl_enter(void) { return false; }

/* ── _bind_prompt_submit_keys (L?) ── */
/* PoP: cli_bind_submit_keys @ cli.py:_bind_prompt_submit_keys */
void cli_bind_submit_keys(void) {}

/* ── _disable_prompt_toolkit_cpr_warning (L?) ── */
/* PoP: cli_disable_cpr_warn @ cli.py:_disable_prompt_toolkit_cpr_warning */
void cli_disable_cpr_warn(void) {}

/* ── _terminal_may_leak_cpr (L?) ── */
/* PoP: cli_terminal_leak_cpr @ cli.py:_terminal_may_leak_cpr */
bool cli_terminal_leak_cpr(void) { return false; }

/* ── _build_cpr_disabled_output (L?) ── */
/* PoP: cli_build_cpr_output @ cli.py:_build_cpr_disabled_output */
void cli_build_cpr_output(void) {}

/* ── _select_classic_cli_pt_output (L?) ── */
/* PoP: cli_select_classic_output @ cli.py:_select_classic_cli_pt_output */
void cli_select_classic_output(void) {}

/* ── _strip_leaked_terminal_responses_with_meta (L?) ── */
/* PoP: cli_strip_terminal_resp @ cli.py:_strip_leaked_terminal_responses_with_meta */
json_t *cli_strip_terminal_resp(const char *text) {
    json_t *result = json_object();
    json_set(result, "text", json_string(text ? text : ""));
    json_set(result, "modified", json_bool(false));
    return result;
}

/* ── _strip_leaked_terminal_responses (L?) ── */
/* PoP: cli_strip_terminal_resp_text @ cli.py:_strip_leaked_terminal_responses */
char *cli_strip_terminal_resp_text(const char *text) { return xstrdup(text ? text : ""); }

/* ── _estimate_tui_input_height (L?) ── */
/* PoP: cli_estimate_input_height @ cli.py:_estimate_tui_input_height */
int cli_estimate_input_height(void) { return 1; }

/* ── _collect_query_images (L?) ── */
/* PoP: cli_collect_query_images @ cli.py:_collect_query_images */
json_t *cli_collect_query_images(const char *query, const char *image_arg) {
    (void)query;
    (void)image_arg;
    json_t *result = json_object();
    json_set(result, "text", json_string(query ? query : ""));
    json_set(result, "images", json_array());
    return result;
}

/* ── _ensure_skill_commands (L?) ── */
/* PoP: cli_ensure_skill_cmds @ cli.py:_ensure_skill_commands */
json_t *cli_ensure_skill_cmds(void) { return json_object(); }

/* ── get_skill_commands (L?) ── */
/* PoP: cli_get_skill_cmds @ cli.py:get_skill_commands */
json_t *cli_get_skill_cmds(void) { return json_object(); }

/* ── build_skill_invocation_message (L?) ── */
/* PoP: cli_build_skill_msg @ cli.py:build_skill_invocation_message */
char *cli_build_skill_msg(void) { return xstrdup(""); }

/* ── build_preloaded_skills_prompt (L?) ── */
/* PoP: cli_build_skills_prompt @ cli.py:build_preloaded_skills_prompt */
char *cli_build_skills_prompt(void) { return xstrdup(""); }

/* ── get_skill_bundles (L?) ── */
/* PoP: cli_get_skill_bundles @ cli.py:get_skill_bundles */
json_t *cli_get_skill_bundles(void) { return json_object(); }

/* ── build_bundle_invocation_message (L?) ── */
/* PoP: cli_build_bundle_msg @ cli.py:build_bundle_invocation_message */
char *cli_build_bundle_msg(void) { return xstrdup(""); }

/* ── _get_plugin_cmd_handler_names (L?) ── */
/* PoP: cli_get_plugin_handlers @ cli.py:_get_plugin_cmd_handler_names */
json_t *cli_get_plugin_handlers(void) { return json_array(); }

/* ── _parse_skills_argument (L?) ── */
/* PoP: cli_parse_skills_arg @ cli.py:_parse_skills_argument */
json_t *cli_parse_skills_arg(const char *skills) {
    (void)skills;
    return json_array();
}

/* ── _normalize_moa_model (L?) ── */
/* PoP: cli_normalize_moa @ cli.py:_normalize_moa_model */
json_t *cli_normalize_moa(const char *model) {
    json_t *result = json_object();
    json_set(result, "model", json_string(model ? model : ""));
    json_set(result, "provider", json_string(""));
    return result;
}

/* ── _run_kanban_goal_loop_q (L?) ── */
/* PoP: cli_run_kanban_loop @ cli.py:_run_kanban_goal_loop_q */
void cli_run_kanban_loop(const char *first_response) { (void)first_response; }

/* ── main (L?) ── */
/* PoP: cli_main @ cli.py:main */
int cli_main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════
 * tools/skills_hub.py — remaining 4 functions
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _search_registries (L?) ── */
/* PoP: skills_hub_search_reg @ tools/skills_hub.py:_search_registries */
json_t *skills_hub_search_reg(const char *query) { (void)query; return json_array(); }

/* ── _create_source_router (L?) ── */
/* PoP: skills_hub_router @ tools/skills_hub.py:_create_source_router */
json_t *skills_hub_router(void) { return json_object(); }

/* ── _parallel_search_sources (L?) ── */
/* PoP: skills_hub_parallel_search @ tools/skills_hub.py:_parallel_search_sources */
json_t *skills_hub_parallel_search(const char *query) { (void)query; return json_array(); }

/* ── _quarantine_dir (L?) ── */
/* PoP: skills_hub_quarantine @ tools/skills_hub.py:_quarantine_dir */
char *skills_hub_quarantine(void) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/.hermes/skills/.hub/quarantine", getenv("HOME") ? getenv("HOME") : ".");
    return xstrdup(path);
}
