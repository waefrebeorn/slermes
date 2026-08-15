/* Slermes C11 port — cli.py */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: CanonicalUsage @ cli.py:CanonicalUsage */
static json_t *CanonicalUsage(json_t * *args, json_t * **kwargs) {
    return json_object();
}

/* PoP: _reverse_alias_for_display @ cli.py:_reverse_alias_for_display */
static const char *_reverse_alias_for_display(const char * model_name) {
    return NULL;
}

/* PoP: _assistant_copy_text @ cli.py:_assistant_copy_text */
static const char *_assistant_copy_text(json_t * content) {
    return NULL;
}

/* PoP: _resolve_prefill_messages_file @ cli.py:_resolve_prefill_messages_file */
static const char *_resolve_prefill_messages_file(json_t * config) {
    return NULL;
}

/* PoP: find_spec @ cli.py:find_spec */
static json_t *find_spec(json_t * self, json_t * fullname, json_t * path, json_t * target) {
    return json_object();
}

/* PoP: AIAgent @ cli.py:AIAgent */
static json_t *AIAgent(json_t * *args, json_t * **kwargs) {
    return json_object();
}

/* PoP: get_tool_definitions @ cli.py:get_tool_definitions */
static json_t *get_tool_definitions(json_t * *args, json_t * **kwargs) {
    return json_object();
}

/* PoP: get_toolset_for_tool @ cli.py:get_toolset_for_tool */
static json_t *get_toolset_for_tool(json_t * *args, json_t * **kwargs) {
    return json_object();
}

/* PoP: get_all_toolsets @ cli.py:get_all_toolsets */
static json_t *get_all_toolsets(json_t * *args, json_t * **kwargs) {
    return json_object();
}

/* PoP: get_toolset_info @ cli.py:get_toolset_info */
static json_t *get_toolset_info(json_t * *args, json_t * **kwargs) {
    return json_object();
}

/* PoP: validate_toolset @ cli.py:validate_toolset */
static json_t *validate_toolset(json_t * *args, json_t * **kwargs) {
    return json_object();
}

/* PoP: _sync_process_session_id @ cli.py:_sync_process_session_id */
static void _sync_process_session_id(const char * session_id) {
}

/* PoP: get_job @ cli.py:get_job */
static json_t *get_job(json_t * *args, json_t * **kwargs) {
    return json_object();
}

/* PoP: _cleanup_all_terminals @ cli.py:_cleanup_all_terminals */
static json_t *_cleanup_all_terminals(json_t * *args, json_t * **kwargs) {
    return json_object();
}

/* PoP: set_sudo_password_callback @ cli.py:set_sudo_password_callback */
static json_t *set_sudo_password_callback(json_t * *args, json_t * **kwargs) {
    return json_object();
}

/* PoP: set_approval_callback @ cli.py:set_approval_callback */
static json_t *set_approval_callback(json_t * *args, json_t * **kwargs) {
    return json_object();
}

/* PoP: set_secret_capture_callback @ cli.py:set_secret_capture_callback */
static json_t *set_secret_capture_callback(json_t * *args, json_t * **kwargs) {
    return json_object();
}

/* PoP: _cleanup_all_browsers @ cli.py:_cleanup_all_browsers */
static json_t *_cleanup_all_browsers(json_t * *args, json_t * **kwargs) {
    return json_object();
}

/* PoP: _mark_tui_input_modes_active @ cli.py:_mark_tui_input_modes_active */
static void _mark_tui_input_modes_active(void) {
}

/* PoP: _prepare_deferred_agent_startup_xf @ cli.py:_prepare_deferred_agent_startup */
static void _prepare_deferred_agent_startup_xf(void) {
}

/* PoP: _arm_exit_watchdog_xf @ cli.py:_arm_exit_watchdog */
static void _arm_exit_watchdog_xf(json_t * timeout_s) {
}

/* PoP: _arm_exit_watchdog_on_shutdown_signal_xf @ cli.py:_arm_exit_watchdog_on_shutdown_signal */
static void _arm_exit_watchdog_on_shutdown_signal_xf(void) {
}

/* PoP: _run_cleanup @ cli.py:_run_cleanup */
static json_t *_run_cleanup(bool notify_session_finalize) {
    return json_object();
}

/* PoP: _should_emit_cleanup_session_finalize @ cli.py:_should_emit_cleanup_session_finalize */
static bool _should_emit_cleanup_session_finalize(json_t * session_id) {
    return false;
}

/* PoP: _emit_interrupted_session_end @ cli.py:_emit_interrupted_session_end */
static void _emit_interrupted_session_end(json_t * cli, const char * reason) {
}

/* PoP: _notify_single_query_session_finalize @ cli.py:_notify_single_query_session_finalize */
static void _notify_single_query_session_finalize(json_t * cli, const char * reason) {
}

/* PoP: _finalize_single_query @ cli.py:_finalize_single_query */
static void _finalize_single_query(json_t * cli) {
}

/* PoP: _reset_terminal_input_modes_on_exit_xf @ cli.py:_reset_terminal_input_modes_on_exit */
static void _reset_terminal_input_modes_on_exit_xf(void) {
}

/* PoP: _git_repo_root @ cli.py:_git_repo_root */
static const char *_git_repo_root(void) {
    return NULL;
}

/* PoP: _worktree_has_unpushed_commits @ cli.py:_worktree_has_unpushed_commits */
static bool _worktree_has_unpushed_commits(const char * worktree_path, int timeout) {
    return false;
}

/* PoP: _worktree_merge_cache_path @ cli.py:_worktree_merge_cache_path */
static const char *_worktree_merge_cache_path(void) {
    return NULL;
}

/* PoP: _save_worktree_merge_cache_xf @ cli.py:_save_worktree_merge_cache */
static void _save_worktree_merge_cache_xf(json_t * verdicts) {
}

/* PoP: _cleanup_worktree @ cli.py:_cleanup_worktree */
static void _cleanup_worktree(json_t * info) {
}

/* PoP: _run_state_db_auto_maintenance @ cli.py:_run_state_db_auto_maintenance */
static void _run_state_db_auto_maintenance(json_t * session_db) {
}

/* PoP: _run_checkpoint_auto_maintenance @ cli.py:_run_checkpoint_auto_maintenance */
static void _run_checkpoint_auto_maintenance(void) {
}

/* PoP: _prune_stale_worktrees @ cli.py:_prune_stale_worktrees */
static void _prune_stale_worktrees(const char * repo_root, int max_age_hours) {
}

/* PoP: _prune_orphaned_branches @ cli.py:_prune_orphaned_branches */
static void _prune_orphaned_branches(const char * repo_root) {
}

/* PoP: _detect_light_mode @ cli.py:_detect_light_mode */
static bool _detect_light_mode(void) {
    return false;
}

/* PoP: _maybe_remap_for_light_mode @ cli.py:_maybe_remap_for_light_mode */
static const char *_maybe_remap_for_light_mode(const char * hex_color) {
    return NULL;
}

/* PoP: _install_skin_light_mode_hook @ cli.py:_install_skin_light_mode_hook */
static void _install_skin_light_mode_hook(void) {
}

/* PoP: __str__ @ cli.py:__str__ */
static const char *__str__(void) {
    return NULL;
}

/* PoP: __add__ @ cli.py:__add__ */
static const char *__add__(json_t * self, const char * other) {
    return NULL;
}

/* PoP: __radd__ @ cli.py:__radd__ */
static const char *__radd__(json_t * self, const char * other) {
    return NULL;
}

/* PoP: _rich_text_from_ansi @ cli.py:_rich_text_from_ansi */
static json_t *_rich_text_from_ansi(const char * text) {
    return json_object();
}

/* PoP: _strip_markdown_syntax @ cli.py:_strip_markdown_syntax */
static const char *_strip_markdown_syntax(const char * text) {
    return NULL;
}

/* PoP: _preserve_windows_dot_segments_for_markdown @ cli.py:_preserve_windows_dot_segments_for_markdown */
static const char *_preserve_windows_dot_segments_for_markdown(const char * text) {
    return NULL;
}

/* PoP: _terminal_width_for_streaming @ cli.py:_terminal_width_for_streaming */
static int _terminal_width_for_streaming(void) {
    return 0;
}

/* PoP: _render_final_assistant_content @ cli.py:_render_final_assistant_content */
static json_t *_render_final_assistant_content(const char * text, const char * mode) {
    return json_object();
}

/* PoP: _post_stream_transform_output @ cli.py:_post_stream_transform_output */
static const char *_post_stream_transform_output(const char * response, json_t * result) {
    return NULL;
}

/* PoP: _coerce_output_history_limit @ cli.py:_coerce_output_history_limit */
static int _coerce_output_history_limit(json_t * value) {
    return 0;
}

/* PoP: _configure_output_history @ cli.py:_configure_output_history */
static void _configure_output_history(bool enabled, json_t * max_lines) {
}

/* PoP: _clear_output_history @ cli.py:_clear_output_history */
static void _clear_output_history(void) {
}

/* PoP: _suspend_output_history @ cli.py:_suspend_output_history */
static json_t *_suspend_output_history(void) {
    return json_object();
}

/* PoP: _record_output_history_entry @ cli.py:_record_output_history_entry */
static void _record_output_history_entry(json_t * entry) {
}

/* PoP: _record_output_history @ cli.py:_record_output_history */
static void _record_output_history(const char * text) {
}

/* PoP: _replay_output_history @ cli.py:_replay_output_history */
static void _replay_output_history(void) {
}

/* PoP: _cprint @ cli.py:_cprint */
static json_t *_cprint(const char * text) {
    return json_object();
}

/* PoP: _prepend_note_to_message @ cli.py:_prepend_note_to_message */
static json_t *_prepend_note_to_message(json_t * message, const char * note) {
    return json_object();
}

/* PoP: _termux_example_image_path @ cli.py:_termux_example_image_path */
static const char *_termux_example_image_path(const char * filename) {
    return NULL;
}

/* PoP: _format_image_attachment_badges @ cli.py:_format_image_attachment_badges */
static const char *_format_image_attachment_badges(json_t * attached_images, int image_counter, json_t * width) {
    return NULL;
}

/* PoP: _should_auto_attach_clipboard_image_on_paste @ cli.py:_should_auto_attach_clipboard_image_on_paste */
static bool _should_auto_attach_clipboard_image_on_paste(const char * pasted_text) {
    return false;
}

/* PoP: _strip_leaked_bracketed_paste_wrappers @ cli.py:_strip_leaked_bracketed_paste_wrappers */
static const char *_strip_leaked_bracketed_paste_wrappers(const char * text) {
    return NULL;
}

/* PoP: _apply_bracketed_paste_timeout_patch @ cli.py:_apply_bracketed_paste_timeout_patch */
static void _apply_bracketed_paste_timeout_patch(void) {
}

/* PoP: _preserve_ctrl_enter_newline_xf @ cli.py:_preserve_ctrl_enter_newline */
static bool _preserve_ctrl_enter_newline_xf(void) {
    return false;
}

/* PoP: _bind_prompt_submit_keys @ cli.py:_bind_prompt_submit_keys */
static void _bind_prompt_submit_keys(json_t * kb, json_t * handler) {
}

/* PoP: _disable_prompt_toolkit_cpr_warning @ cli.py:_disable_prompt_toolkit_cpr_warning */
static void _disable_prompt_toolkit_cpr_warning(json_t * app) {
}

/* PoP: _terminal_may_leak_cpr_xf @ cli.py:_terminal_may_leak_cpr */
static bool _terminal_may_leak_cpr_xf(void) {
    return false;
}

/* PoP: _select_classic_cli_pt_output @ cli.py:_select_classic_cli_pt_output */
static json_t *_select_classic_cli_pt_output(json_t * stdout) {
    return json_object();
}

/* PoP: _strip_leaked_terminal_responses @ cli.py:_strip_leaked_terminal_responses */
static const char *_strip_leaked_terminal_responses(const char * text) {
    return NULL;
}

/* PoP: _build_compact_banner @ cli.py:_build_compact_banner */
static const char *_build_compact_banner(void) {
    return NULL;
}

/* PoP: _looks_like_slash_command @ cli.py:_looks_like_slash_command */
static bool _looks_like_slash_command(const char * text) {
    return false;
}

/* PoP: _ensure_skill_commands @ cli.py:_ensure_skill_commands */
static json_t *_ensure_skill_commands(void) {
    return json_object();
}

/* PoP: _get_plugin_cmd_handler_names @ cli.py:_get_plugin_cmd_handler_names */
static json_t *_get_plugin_cmd_handler_names(void) {
    return json_object();
}

/* PoP: _parse_skills_argument @ cli.py:_parse_skills_argument */
static json_t *_parse_skills_argument(json_t * skills) {
    return json_object();
}

/* PoP: save_config_value_xf @ cli.py:save_config_value */
static bool save_config_value_xf(const char * key_path, json_t * value) {
    return false;
}

/* PoP: __str___2 @ cli.py:__str__ */
static const char *__str___2(void) {
    return NULL;
}

/* PoP: _claim_active_session @ cli.py:_claim_active_session */
static bool _claim_active_session(json_t * self, const char * surface, bool stderr) {
    return false;
}

/* PoP: _release_active_session @ cli.py:_release_active_session */
static void _release_active_session(void) {
}

/* PoP: _invalidate @ cli.py:_invalidate */
static void _invalidate(json_t * self, double min_interval) {
}

/* PoP: _paint_now @ cli.py:_paint_now */
static void _paint_now(void) {
}

/* PoP: _force_full_redraw @ cli.py:_force_full_redraw */
static void _force_full_redraw(void) {
}

/* PoP: _clear_prompt_toolkit_screen @ cli.py:_clear_prompt_toolkit_screen */
static void _clear_prompt_toolkit_screen(json_t * self, json_t * app, bool rebuild_scrollback) {
}

/* PoP: _recover_after_resize @ cli.py:_recover_after_resize */
static void _recover_after_resize(json_t * self, json_t * app, json_t * original_on_resize) {
}

/* PoP: _schedule_resize_recovery @ cli.py:_schedule_resize_recovery */
static void _schedule_resize_recovery(json_t * self, json_t * app, json_t * original_on_resize, double delay) {
}

/* PoP: _status_bar_context_style @ cli.py:_status_bar_context_style */
static const char *_status_bar_context_style(json_t * self, int percent_used) {
    return NULL;
}

/* PoP: _battery_status_style @ cli.py:_battery_status_style */
static const char *_battery_status_style(const char * category) {
    return NULL;
}

/* PoP: _handle_battery_command @ cli.py:_handle_battery_command */
static void _handle_battery_command(json_t * self, const char * cmd_original) {
}

/* PoP: _compression_count_style @ cli.py:_compression_count_style */
static const char *_compression_count_style(int count) {
    return NULL;
}

/* PoP: _build_context_bar @ cli.py:_build_context_bar */
static const char *_build_context_bar(json_t * self, int percent_used, int width) {
    return NULL;
}

/* PoP: _format_prompt_elapsed @ cli.py:_format_prompt_elapsed */
static const char *_format_prompt_elapsed(double prompt_start_time, double prompt_duration, bool live) {
    return NULL;
}

/* PoP: _format_idle_since @ cli.py:_format_idle_since */
static const char *_format_idle_since(double last_finished_at, bool turn_live) {
    return NULL;
}

/* PoP: _get_status_bar_session_title @ cli.py:_get_status_bar_session_title */
static const char *_get_status_bar_session_title(void) {
    return NULL;
}

/* PoP: _status_bar_display_width @ cli.py:_status_bar_display_width */
static int _status_bar_display_width(const char * text) {
    return 0;
}

/* PoP: _trim_status_bar_text @ cli.py:_trim_status_bar_text */
static const char *_trim_status_bar_text(json_t * cls, const char * text, int max_width) {
    return NULL;
}

/* PoP: _right_align_status_title @ cli.py:_right_align_status_title */
static const char *_right_align_status_title(json_t * cls, const char * text, const char * title, int width) {
    return NULL;
}

/* PoP: _right_align_status_title_fragments @ cli.py:_right_align_status_title_fragments */
static json_t *_right_align_status_title_fragments(json_t * cls, json_t * frags, const char * title, int width) {
    return json_object();
}

/* PoP: _use_minimal_tui_chrome @ cli.py:_use_minimal_tui_chrome */
static bool _use_minimal_tui_chrome(json_t * self, int width) {
    return false;
}

/* PoP: _scrollback_box_width @ cli.py:_scrollback_box_width */
static int _scrollback_box_width(int width) {
    return 0;
}

/* PoP: _tui_input_rule_height @ cli.py:_tui_input_rule_height */
static int _tui_input_rule_height(json_t * self, const char * position, int width) {
    return 0;
}

/* PoP: _spinner_widget_height @ cli.py:_spinner_widget_height */
static int _spinner_widget_height(json_t * self, int width) {
    return 0;
}

/* PoP: _render_spinner_text @ cli.py:_render_spinner_text */
static const char *_render_spinner_text(void) {
    return NULL;
}

/* PoP: _on_reaction @ cli.py:_on_reaction */
static void _on_reaction(json_t * self, const char * kind) {
}

/* PoP: _voice_record_key_label @ cli.py:_voice_record_key_label */
static const char *_voice_record_key_label(void) {
    return NULL;
}

/* PoP: set_voice_record_key_cache @ cli.py:set_voice_record_key_cache */
static void set_voice_record_key_cache(json_t * self, json_t * raw_key) {
}

/* PoP: _get_voice_status_fragments @ cli.py:_get_voice_status_fragments */
static json_t *_get_voice_status_fragments(json_t * self, int width) {
    return json_object();
}

/* PoP: _build_status_bar_text @ cli.py:_build_status_bar_text */
static const char *_build_status_bar_text(json_t * self, int width) {
    return NULL;
}

/* PoP: _get_status_bar_fragments @ cli.py:_get_status_bar_fragments */
static json_t *_get_status_bar_fragments(void) {
    return json_object();
}

/* PoP: _normalize_model_for_provider @ cli.py:_normalize_model_for_provider */
static bool _normalize_model_for_provider(json_t * self, const char * resolved_provider) {
    return false;
}

/* PoP: _on_thinking @ cli.py:_on_thinking */
static void _on_thinking(json_t * self, const char * text) {
}

/* PoP: _on_notice @ cli.py:_on_notice */
static void _on_notice(json_t * self, json_t * notice) {
}

/* PoP: _flush_credit_notices @ cli.py:_flush_credit_notices */
static void _flush_credit_notices(void) {
}

/* PoP: _on_notice_clear @ cli.py:_on_notice_clear */
static void _on_notice_clear(json_t * self, const char * key) {
}

/* PoP: _current_reasoning_callback @ cli.py:_current_reasoning_callback */
static json_t *_current_reasoning_callback(void) {
    return json_object();
}

/* PoP: _emit_reasoning_preview @ cli.py:_emit_reasoning_preview */
static void _emit_reasoning_preview(json_t * self, const char * reasoning_text) {
}

/* PoP: _flush_reasoning_preview @ cli.py:_flush_reasoning_preview */
static void _flush_reasoning_preview(json_t * self, bool force) {
}

/* PoP: _format_submitted_user_message_preview @ cli.py:_format_submitted_user_message_preview */
static const char *_format_submitted_user_message_preview(json_t * self, const char * user_input) {
    return NULL;
}

/* PoP: _expand_paste_references @ cli.py:_expand_paste_references */
static const char *_expand_paste_references(json_t * self, json_t * text) {
    return NULL;
}

/* PoP: _print_user_message_preview @ cli.py:_print_user_message_preview */
static void _print_user_message_preview(json_t * self, const char * user_input) {
}

/* PoP: _stream_reasoning_delta @ cli.py:_stream_reasoning_delta */
static void _stream_reasoning_delta(json_t * self, const char * text) {
}

/* PoP: _close_reasoning_box @ cli.py:_close_reasoning_box */
static void _close_reasoning_box(void) {
}

/* PoP: _stream_delta @ cli.py:_stream_delta */
static void _stream_delta(json_t * self, json_t * text) {
}

/* PoP: _emit_stream_text @ cli.py:_emit_stream_text */
static void _emit_stream_text(json_t * self, const char * text) {
}

/* PoP: _flush_stream @ cli.py:_flush_stream */
static void _flush_stream(void) {
}

/* PoP: _reset_stream_state @ cli.py:_reset_stream_state */
static void _reset_stream_state(void) {
}

/* PoP: _slow_command_status @ cli.py:_slow_command_status */
static const char *_slow_command_status(json_t * self, const char * command) {
    return NULL;
}

/* PoP: _command_spinner_frame @ cli.py:_command_spinner_frame */
static const char *_command_spinner_frame(void) {
    return NULL;
}

/* PoP: _busy_command @ cli.py:_busy_command */
static json_t *_busy_command(json_t * self, const char * status, bool blocks_input) {
    return json_object();
}

/* PoP: _open_external_editor @ cli.py:_open_external_editor */
static bool _open_external_editor(json_t * self, json_t * buffer) {
    return false;
}

/* PoP: _inline_pastes @ cli.py:_inline_pastes */
static void _inline_pastes(json_t * self, json_t * buffer) {
}

/* PoP: _install_tool_callbacks @ cli.py:_install_tool_callbacks */
static void _install_tool_callbacks(void) {
}

/* PoP: _ensure_tirith_security @ cli.py:_ensure_tirith_security */
static void _ensure_tirith_security(void) {
}

/* PoP: _show_security_advisories @ cli.py:_show_security_advisories */
static json_t *_show_security_advisories(void) {
    return json_object();
}

/* PoP: _show_browser_backend_notice @ cli.py:_show_browser_backend_notice */
static json_t *_show_browser_backend_notice(void) {
    return json_object();
}

/* PoP: finalize_preloaded_skills @ cli.py:finalize_preloaded_skills */
static void finalize_preloaded_skills(void) {
}

/* PoP: show_banner @ cli.py:show_banner */
static json_t *show_banner(void) {
    return json_object();
}

/* PoP: _restore_session_cwd @ cli.py:_restore_session_cwd */
static void _restore_session_cwd(json_t * self, json_t * session_meta, bool quiet) {
}

/* PoP: _render_resume_history_panel_lines @ cli.py:_render_resume_history_panel_lines */
static json_t *_render_resume_history_panel_lines(json_t * self, json_t * panel) {
    return json_object();
}

/* PoP: _try_attach_clipboard_image @ cli.py:_try_attach_clipboard_image */
static bool _try_attach_clipboard_image(void) {
    return false;
}

/* PoP: _write_osc52_clipboard @ cli.py:_write_osc52_clipboard */
static void _write_osc52_clipboard(json_t * self, const char * text) {
}

/* PoP: _recover_terminal_input_modes @ cli.py:_recover_terminal_input_modes */
static void _recover_terminal_input_modes(json_t * self, const char * reason) {
}

/* PoP: _preprocess_images_with_vision @ cli.py:_preprocess_images_with_vision */
static const char *_preprocess_images_with_vision(json_t * self, const char * text, json_t * images, bool announce) {
    return NULL;
}

/* PoP: _show_tool_availability_warnings @ cli.py:_show_tool_availability_warnings */
static json_t *_show_tool_availability_warnings(void) {
    return json_object();
}

/* PoP: _show_status @ cli.py:_show_status */
static json_t *_show_status(void) {
    return json_object();
}

/* PoP: _show_session_status @ cli.py:_show_session_status */
static json_t *_show_session_status(void) {
    return json_object();
}

/* PoP: _fast_command_available @ cli.py:_fast_command_available */
static bool _fast_command_available(void) {
    return false;
}

/* PoP: _command_available @ cli.py:_command_available */
static bool _command_available(json_t * self, const char * slash_command) {
    return false;
}

/* PoP: show_help @ cli.py:show_help */
static json_t *show_help(void) {
    return json_object();
}

/* PoP: show_tools @ cli.py:show_tools */
static json_t *show_tools(void) {
    return json_object();
}

/* PoP: show_toolsets @ cli.py:show_toolsets */
static json_t *show_toolsets(void) {
    return json_object();
}

/* PoP: _show_recent_sessions @ cli.py:_show_recent_sessions */
static bool _show_recent_sessions(json_t * self, const char * reason, int limit) {
    return false;
}

/* PoP: show_history @ cli.py:show_history */
static json_t *show_history(void) {
    return json_object();
}

/* PoP: _discard_session_if_empty @ cli.py:_discard_session_if_empty */
static bool _discard_session_if_empty(json_t * self, const char * session_id) {
    return false;
}

/* PoP: new_session_xf @ cli.py:new_session */
static json_t *new_session_xf(json_t * self, json_t * silent, json_t * title) {
    return json_object();
}

/* PoP: _consume_pending_resume_selection @ cli.py:_consume_pending_resume_selection */
static bool _consume_pending_resume_selection(json_t * self, const char * text) {
    return false;
}

/* PoP: save_conversation @ cli.py:save_conversation */
static json_t *save_conversation(void) {
    return json_object();
}

/* PoP: retry_last @ cli.py:retry_last */
static json_t *retry_last(void) {
    return json_object();
}

/* PoP: undo_last @ cli.py:undo_last */
static json_t *undo_last(json_t * self, int n, bool prefill) {
    return json_object();
}

/* PoP: _undo_content_to_text @ cli.py:_undo_content_to_text */
static const char *_undo_content_to_text(json_t * content) {
    return NULL;
}

/* PoP: _prefill_input_buffer @ cli.py:_prefill_input_buffer */
static void _prefill_input_buffer(json_t * self, const char * text) {
}

/* PoP: _submit_slash_confirm_response @ cli.py:_submit_slash_confirm_response */
static void _submit_slash_confirm_response(json_t * self, json_t * value) {
}

/* PoP: _get_slash_confirm_display_fragments @ cli.py:_get_slash_confirm_display_fragments */
static json_t *_get_slash_confirm_display_fragments(void) {
    return json_object();
}

/* PoP: _open_model_picker @ cli.py:_open_model_picker */
static void _open_model_picker(json_t * self, json_t * providers, const char * current_model, const char * current_provider, json_t * user_provs, json_t * custom_provs) {
}

/* PoP: _confirm_expensive_model_switch @ cli.py:_confirm_expensive_model_switch */
static bool _confirm_expensive_model_switch(json_t * self, json_t * result) {
    return false;
}

/* PoP: _close_model_picker @ cli.py:_close_model_picker */
static void _close_model_picker(void) {
}

/* PoP: _snapshot_model_runtime @ cli.py:_snapshot_model_runtime */
static json_t *_snapshot_model_runtime(void) {
    return json_object();
}

/* PoP: _restore_model_runtime_snapshot @ cli.py:_restore_model_runtime_snapshot */
static void _restore_model_runtime_snapshot(json_t * self, json_t * snapshot) {
}

/* PoP: _clear_persisted_context_for_model_switch @ cli.py:_clear_persisted_context_for_model_switch */
static void _clear_persisted_context_for_model_switch(json_t * self, json_t * result) {
}

/* PoP: _handle_model_picker_selection @ cli.py:_handle_model_picker_selection */
static void _handle_model_picker_selection(json_t * self, bool persist_global) {
}

/* PoP: _handle_model_switch @ cli.py:_handle_model_switch */
static json_t *_handle_model_switch(json_t * self, const char * cmd_original) {
    return json_object();
}

/* PoP: _handle_codex_runtime @ cli.py:_handle_codex_runtime */
static void _handle_codex_runtime(json_t * self, const char * cmd_original) {
}

/* PoP: _should_handle_model_command_inline @ cli.py:_should_handle_model_command_inline */
static bool _should_handle_model_command_inline(json_t * self, const char * text, bool has_images) {
    return false;
}

/* PoP: _should_handle_steer_command_inline @ cli.py:_should_handle_steer_command_inline */
static bool _should_handle_steer_command_inline(json_t * self, const char * text, bool has_images) {
    return false;
}

/* PoP: _output_console @ cli.py:_output_console */
static json_t *_output_console(void) {
    return json_object();
}

/* PoP: _console_print @ cli.py:_console_print */
static json_t *_console_print(json_t * self, json_t * *args, json_t * **kwargs) {
    return json_object();
}

/* PoP: _resolve_personality_prompt @ cli.py:_resolve_personality_prompt */
static const char *_resolve_personality_prompt(json_t * value) {
    return NULL;
}

/* PoP: _show_gateway_status @ cli.py:_show_gateway_status */
static json_t *_show_gateway_status(void) {
    return json_object();
}

/* PoP: process_command @ cli.py:process_command */
static bool process_command(json_t * self, const char * command) {
    return false;
}

/* PoP: _try_launch_chrome_debug @ cli.py:_try_launch_chrome_debug */
static bool _try_launch_chrome_debug(int port, const char * system) {
    return false;
}

/* PoP: _get_goal_manager @ cli.py:_get_goal_manager */
static json_t *_get_goal_manager(void) {
    return json_object();
}

/* PoP: _get_heartbeat_manager @ cli.py:_get_heartbeat_manager */
static json_t *_get_heartbeat_manager(void) {
    return json_object();
}

/* PoP: _start_heartbeat_watchdog @ cli.py:_start_heartbeat_watchdog */
static json_t *_start_heartbeat_watchdog(void) {
    return json_object();
}

/* PoP: _owns_process_notification @ cli.py:_owns_process_notification */
static bool _owns_process_notification(json_t * self, json_t * event) {
    return false;
}

/* PoP: _drain_process_notifications @ cli.py:_drain_process_notifications */
static void _drain_process_notifications(json_t * self, const char * consumer) {
}

/* PoP: _maybe_continue_goal_after_turn @ cli.py:_maybe_continue_goal_after_turn */
static void _maybe_continue_goal_after_turn(void) {
}

/* PoP: _toggle_verbose @ cli.py:_toggle_verbose */
static json_t *_toggle_verbose(void) {
    return json_object();
}

/* PoP: _transfer_session_yolo @ cli.py:_transfer_session_yolo */
static void _transfer_session_yolo(json_t * self, const char * old_session_id, const char * new_session_id) {
}

/* PoP: _is_session_yolo_active @ cli.py:_is_session_yolo_active */
static bool _is_session_yolo_active(void) {
    return false;
}

/* PoP: _toggle_yolo @ cli.py:_toggle_yolo */
static json_t *_toggle_yolo(void) {
    return json_object();
}

/* PoP: _persist_session_yolo @ cli.py:_persist_session_yolo */
static void _persist_session_yolo(json_t * self, const char * session_key, bool enabled) {
}

/* PoP: _on_reasoning @ cli.py:_on_reasoning */
static json_t *_on_reasoning(json_t * self, const char * reasoning_text) {
    return json_object();
}

/* PoP: _manual_compress @ cli.py:_manual_compress */
static json_t *_manual_compress(json_t * self, const char * cmd_original) {
    return json_object();
}

/* PoP: _handle_usage_command @ cli.py:_handle_usage_command */
static json_t *_handle_usage_command(json_t * self, const char * cmd_original) {
    return json_object();
}

/* PoP: _usage_reset @ cli.py:_usage_reset */
static json_t *_usage_reset(json_t * self, bool force) {
    return json_object();
}

/* PoP: _show_usage @ cli.py:_show_usage */
static json_t *_show_usage(void) {
    return json_object();
}

/* PoP: _show_insights @ cli.py:_show_insights */
static json_t *_show_insights(json_t * self, const char * command) {
    return json_object();
}

/* PoP: _check_config_mcp_changes @ cli.py:_check_config_mcp_changes */
static void _check_config_mcp_changes(void) {
}

/* PoP: _confirm_and_reload_mcp @ cli.py:_confirm_and_reload_mcp */
static void _confirm_and_reload_mcp(json_t * self, const char * cmd_original) {
}

/* PoP: _on_tool_gen_start @ cli.py:_on_tool_gen_start */
static void _on_tool_gen_start(json_t * self, const char * tool_name) {
}

/* PoP: _on_tool_progress @ cli.py:_on_tool_progress */
static json_t *_on_tool_progress(json_t * self, const char * event_type, const char * function_name, const char * preview, json_t * function_args, json_t * **kwargs) {
    return json_object();
}

/* PoP: _on_tool_start @ cli.py:_on_tool_start */
static json_t *_on_tool_start(json_t * self, const char * tool_call_id, const char * function_name, json_t * function_args) {
    return json_object();
}

/* PoP: _on_tool_complete @ cli.py:_on_tool_complete */
static json_t *_on_tool_complete(json_t * self, const char * tool_call_id, const char * function_name, json_t * function_args, const char * function_result) {
    return json_object();
}

/* PoP: _voice_start_recording @ cli.py:_voice_start_recording */
static json_t *_voice_start_recording(void) {
    return json_object();
}

/* PoP: _voice_stt_model @ cli.py:_voice_stt_model */
static const char *_voice_stt_model(void) {
    return NULL;
}

/* PoP: _voice_restart_recording_async @ cli.py:_voice_restart_recording_async */
static void _voice_restart_recording_async(void) {
}

/* PoP: _voice_stop_and_transcribe @ cli.py:_voice_stop_and_transcribe */
static json_t *_voice_stop_and_transcribe(void) {
    return json_object();
}

/* PoP: _voice_speak_response_async @ cli.py:_voice_speak_response_async */
static void _voice_speak_response_async(json_t * self, const char * text) {
}

/* PoP: _voice_speak_response @ cli.py:_voice_speak_response */
static json_t *_voice_speak_response(json_t * self, const char * text) {
    return json_object();
}

/* PoP: _voice_submit_barge_utterance @ cli.py:_voice_submit_barge_utterance */
static void _voice_submit_barge_utterance(json_t * self, const char * wav_path) {
}

/* PoP: _voice_beeps_enabled @ cli.py:_voice_beeps_enabled */
static bool _voice_beeps_enabled(void) {
    return false;
}

/* PoP: _enable_voice_mode @ cli.py:_enable_voice_mode */
static json_t *_enable_voice_mode(void) {
    return json_object();
}

/* PoP: _disable_voice_mode @ cli.py:_disable_voice_mode */
static json_t *_disable_voice_mode(void) {
    return json_object();
}

/* PoP: _toggle_voice_tts @ cli.py:_toggle_voice_tts */
static json_t *_toggle_voice_tts(void) {
    return json_object();
}

/* PoP: _show_voice_status @ cli.py:_show_voice_status */
static json_t *_show_voice_status(void) {
    return json_object();
}

/* PoP: _persist_prompt_summary @ cli.py:_persist_prompt_summary */
static void _persist_prompt_summary(json_t * self, const char * icon, const char * label, const char * detail, const char * outcome) {
}

/* PoP: _clarify_callback @ cli.py:_clarify_callback */
static json_t *_clarify_callback(json_t * self, json_t * question, json_t * choices, json_t * multi_select) {
    return json_object();
}

/* PoP: _sudo_password_callback @ cli.py:_sudo_password_callback */
static const char *_sudo_password_callback(void) {
    return NULL;
}

/* PoP: _computer_use_approval_callback @ cli.py:_computer_use_approval_callback */
static const char *_computer_use_approval_callback(json_t * self, const char * action, json_t * args, const char * summary) {
    return NULL;
}

/* PoP: _handle_approval_selection @ cli.py:_handle_approval_selection */
static void _handle_approval_selection(void) {
}

/* PoP: _get_approval_display_fragments @ cli.py:_get_approval_display_fragments */
static json_t *_get_approval_display_fragments(void) {
    return json_object();
}

/* PoP: _secret_capture_callback @ cli.py:_secret_capture_callback */
static json_t *_secret_capture_callback(json_t * self, const char * var_name, const char * prompt, json_t * metadata) {
    return json_object();
}

/* PoP: _capture_modal_input_snapshot @ cli.py:_capture_modal_input_snapshot */
static void _capture_modal_input_snapshot(void) {
}

/* PoP: _restore_modal_input_snapshot @ cli.py:_restore_modal_input_snapshot */
static void _restore_modal_input_snapshot(void) {
}

/* PoP: _submit_secret_response @ cli.py:_submit_secret_response */
static void _submit_secret_response(json_t * self, const char * value) {
}

/* PoP: _cancel_secret_capture @ cli.py:_cancel_secret_capture */
static void _cancel_secret_capture(void) {
}

/* PoP: _clear_secret_input_buffer @ cli.py:_clear_secret_input_buffer */
static void _clear_secret_input_buffer(void) {
}

/* PoP: _clear_terminal_on_exit @ cli.py:_clear_terminal_on_exit */
static json_t *_clear_terminal_on_exit(void) {
    return json_object();
}

/* PoP: _print_exit_summary @ cli.py:_print_exit_summary */
static json_t *_print_exit_summary(json_t * self, bool clear_screen) {
    return json_object();
}

/* PoP: _audio_level_bar @ cli.py:_audio_level_bar */
static const char *_audio_level_bar(void) {
    return NULL;
}

/* PoP: _get_tui_prompt_fragments @ cli.py:_get_tui_prompt_fragments */
static json_t *_get_tui_prompt_fragments(void) {
    return json_object();
}

/* PoP: _get_tui_prompt_text @ cli.py:_get_tui_prompt_text */
static const char *_get_tui_prompt_text(void) {
    return NULL;
}

/* PoP: _apply_tui_skin_style @ cli.py:_apply_tui_skin_style */
static bool _apply_tui_skin_style(void) {
    return false;
}

/* PoP: _get_extra_tui_widgets @ cli.py:_get_extra_tui_widgets */
static json_t *_get_extra_tui_widgets(void) {
    return json_object();
}

/* PoP: _register_extra_tui_keybindings @ cli.py:_register_extra_tui_keybindings */
static void _register_extra_tui_keybindings(json_t * self, json_t * kb, json_t * input_area) {
}

/* PoP: _run_kanban_goal_loop_q @ cli.py:_run_kanban_goal_loop_q */
static void _run_kanban_goal_loop_q(json_t * cli, const char * first_response) {
}

