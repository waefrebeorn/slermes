/*
 * port_cli.c — C port of cli.py
 */

#include "hermes_logger.h"
#include "hermes_skill_commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: string @ cli.py:CanonicalUsage */
/* PoP: string @ cli.py:_strip_reasoning_tags */
/* PoP: string @ cli.py:_assistant_content_as_text */
/* PoP: string @ cli.py:_assistant_copy_text */
/* PoP: string @ cli.py:_load_prefill_messages */
/* PoP: string @ cli.py:_resolve_prefill_messages_file */
/* PoP: string @ cli.py:_parse_reasoning_config */
/* PoP: string @ cli.py:_parse_service_tier_config */
/* PoP: string @ cli.py:load_cli_config */
/* PoP: string @ cli.py:find_spec */
/* PoP: string @ cli.py:AIAgent */
/* PoP: string @ cli.py:get_tool_definitions */
/* PoP: string @ cli.py:get_toolset_for_tool */
/* PoP: string @ cli.py:get_all_toolsets */
/* PoP: string @ cli.py:get_toolset_info */
/* PoP: string @ cli.py:validate_toolset */
/* PoP: string @ cli.py:_sync_process_session_id */
/* PoP: string @ cli.py:get_job */
/* PoP: string @ cli.py:_cleanup_all_terminals */
/* PoP: string @ cli.py:set_sudo_password_callback */
/* PoP: string @ cli.py:set_approval_callback */
/* PoP: string @ cli.py:set_secret_capture_callback */
/* PoP: string @ cli.py:_cleanup_all_browsers */
/* PoP: string @ cli.py:_mark_tui_input_modes_active */
/* PoP: string @ cli.py:_prepare_deferred_agent_startup */
/* PoP: string @ cli.py:_run_cleanup */
/* PoP: string @ cli.py:_should_emit_cleanup_session_finalize */
/* PoP: string @ cli.py:_notify_session_finalize */
/* PoP: string @ cli.py:_emit_interrupted_session_end */
/* PoP: string @ cli.py:_notify_single_query_session_finalize */
/* PoP: string @ cli.py:_finalize_single_query */
/* PoP: string @ cli.py:_reset_terminal_input_modes_on_exit */
/* PoP: string @ cli.py:_normalize_git_bash_path */
/* PoP: string @ cli.py:_git_repo_root */
/* PoP: string @ cli.py:_path_is_within_root */
/* PoP: string @ cli.py:_setup_worktree */
/* PoP: string @ cli.py:_worktree_has_unpushed_commits */
/* PoP: string @ cli.py:_cleanup_worktree */
/* PoP: string @ cli.py:_run_state_db_auto_maintenance */
/* PoP: string @ cli.py:_run_checkpoint_auto_maintenance */
/* PoP: string @ cli.py:_prune_stale_worktrees */
/* PoP: string @ cli.py:_prune_orphaned_branches */
/* PoP: string @ cli.py:_hex_to_ansi */
/* PoP: string @ cli.py:_luminance_from_hex */
/* PoP: string @ cli.py:_query_osc11_background */
/* PoP: string @ cli.py:_detect_light_mode */
/* PoP: string @ cli.py:_maybe_remap_for_light_mode */
/* PoP: string @ cli.py:_install_skin_light_mode_hook */
/* PoP: string @ cli.py:__str__ */
/* PoP: string @ cli.py:__add__ */
/* PoP: string @ cli.py:__radd__ */
/* PoP: string @ cli.py:_rich_text_from_ansi */
/* PoP: string @ cli.py:_strip_markdown_syntax */
/* PoP: string @ cli.py:_preserve_windows_dot_segments_for_markdown */
/* PoP: string @ cli.py:_terminal_width_for_streaming */
/* PoP: string @ cli.py:_render_final_assistant_content */
/* PoP: string @ cli.py:_coerce_output_history_limit */
/* PoP: string @ cli.py:_configure_output_history */
/* PoP: string @ cli.py:_clear_output_history */
/* PoP: string @ cli.py:_suspend_output_history */
/* PoP: string @ cli.py:_record_output_history_entry */
/* PoP: string @ cli.py:_record_output_history */
/* PoP: string @ cli.py:_replay_output_history */
/* PoP: string @ cli.py:_cprint */
/* PoP: string @ cli.py:_prepend_note_to_message */
/* PoP: string @ cli.py:_termux_example_image_path */
/* PoP: string @ cli.py:_split_path_input */
/* PoP: string @ cli.py:_resolve_attachment_path */
/* PoP: string @ cli.py:_detect_file_drop */
/* PoP: string @ cli.py:_format_image_attachment_badges */
/* PoP: string @ cli.py:_should_auto_attach_clipboard_image_on_paste */
/* PoP: string @ cli.py:_strip_leaked_bracketed_paste_wrappers */
/* PoP: string @ cli.py:_apply_bracketed_paste_timeout_patch */
/* PoP: string @ cli.py:_preserve_ctrl_enter_newline */
/* PoP: string @ cli.py:_bind_prompt_submit_keys */
/* PoP: string @ cli.py:_disable_prompt_toolkit_cpr_warning */
/* PoP: string @ cli.py:_strip_leaked_terminal_responses_with_meta */
/* PoP: string @ cli.py:_strip_leaked_terminal_responses */
/* PoP: string @ cli.py:_estimate_tui_input_height */
/* PoP: string @ cli.py:_collect_query_images */
/* PoP: string @ cli.py:_build_compact_banner */
/* PoP: string @ cli.py:_looks_like_slash_command */
/* PoP: string @ cli.py:_ensure_skill_commands */
/* PoP: string @ cli.py:build_skill_invocation_message */
/* PoP: string @ cli.py:_get_plugin_cmd_handler_names */
/* PoP: string @ cli.py:_parse_skills_argument */
/* PoP: string @ cli.py:save_config_value */
/* PoP: string @ cli.py:_claim_active_session */
/* PoP: string @ cli.py:_release_active_session */
/* PoP: string @ cli.py:_invalidate */
/* PoP: string @ cli.py:_paint_now */
/* PoP: string @ cli.py:_force_full_redraw */
/* PoP: string @ cli.py:_clear_prompt_toolkit_screen */
/* PoP: string @ cli.py:_recover_after_resize */
/* PoP: string @ cli.py:_schedule_resize_recovery */
/* PoP: string @ cli.py:_status_bar_context_style */
/* PoP: string @ cli.py:_compression_count_style */
/* PoP: string @ cli.py:_build_context_bar */
/* PoP: string @ cli.py:_format_prompt_elapsed */
/* PoP: string @ cli.py:_format_idle_since */
/* PoP: string @ cli.py:_get_status_bar_snapshot */
/* PoP: string @ cli.py:_status_bar_display_width */
/* PoP: string @ cli.py:_trim_status_bar_text */
/* PoP: string @ cli.py:_get_tui_terminal_width */
/* PoP: string @ cli.py:_use_minimal_tui_chrome */
/* PoP: string @ cli.py:_scrollback_box_width */
/* PoP: string @ cli.py:_tui_input_rule_height */
/* PoP: string @ cli.py:_spinner_widget_height */
/* PoP: string @ cli.py:_render_spinner_text */
/* PoP: string @ cli.py:_voice_record_key_label */
/* PoP: string @ cli.py:set_voice_record_key_cache */
/* PoP: string @ cli.py:_get_voice_status_fragments */
/* PoP: string @ cli.py:_build_status_bar_text */
/* PoP: string @ cli.py:_get_status_bar_fragments */
/* PoP: string @ cli.py:_normalize_model_for_provider */
/* PoP: string @ cli.py:_on_thinking */
/* PoP: string @ cli.py:_on_notice */
/* PoP: string @ cli.py:_flush_credit_notices */
/* PoP: string @ cli.py:_on_notice_clear */
/* PoP: string @ cli.py:_current_reasoning_callback */
/* PoP: string @ cli.py:_emit_reasoning_preview */
/* PoP: string @ cli.py:_flush_reasoning_preview */
/* PoP: string @ cli.py:_format_submitted_user_message_preview */
/* PoP: string @ cli.py:_expand_paste_references */
/* PoP: string @ cli.py:_print_user_message_preview */
/* PoP: string @ cli.py:_stream_reasoning_delta */
/* PoP: string @ cli.py:_close_reasoning_box */
/* PoP: string @ cli.py:_stream_delta */
/* PoP: string @ cli.py:_emit_stream_text */
/* PoP: string @ cli.py:_flush_stream */
/* PoP: string @ cli.py:_reset_stream_state */
/* PoP: string @ cli.py:_slow_command_status */
/* PoP: string @ cli.py:_command_spinner_frame */
/* PoP: string @ cli.py:_busy_command */
/* PoP: string @ cli.py:_open_external_editor */
/* PoP: string @ cli.py:_install_tool_callbacks */
/* PoP: string @ cli.py:_ensure_tirith_security */
/* PoP: string @ cli.py:_show_security_advisories */
/* PoP: string @ cli.py:show_banner */
/* PoP: string @ cli.py:_restore_session_cwd */
/* PoP: string @ cli.py:_render_resume_history_panel_lines */
/* PoP: string @ cli.py:_try_attach_clipboard_image */
/* PoP: string @ cli.py:_resolve_checkpoint_ref */
/* PoP: string @ cli.py:_write_osc52_clipboard */
/* PoP: string @ cli.py:_recover_terminal_input_modes */
/* PoP: string @ cli.py:_preprocess_images_with_vision */
/* PoP: string @ cli.py:_show_tool_availability_warnings */
/* PoP: string @ cli.py:_show_status */
/* PoP: string @ cli.py:_show_session_status */
/* PoP: string @ cli.py:_fast_command_available */
/* PoP: string @ cli.py:_command_available */
/* PoP: string @ cli.py:show_help */
/* PoP: string @ cli.py:show_tools */
/* PoP: string @ cli.py:show_toolsets */
/* PoP: string @ cli.py:_list_recent_sessions */
/* PoP: string @ cli.py:_show_recent_sessions */
/* PoP: string @ cli.py:show_history */
/* PoP: string @ cli.py:_discard_session_if_empty */
/* PoP: string @ cli.py:new_session */
/* PoP: string @ cli.py:_consume_pending_resume_selection */
/* PoP: string @ cli.py:save_conversation */
/* PoP: string @ cli.py:retry_last */
/* PoP: string @ cli.py:undo_last */
/* PoP: string @ cli.py:_undo_content_to_text */
/* PoP: string @ cli.py:_prefill_input_buffer */
/* PoP: string @ cli.py:_run_curses_picker */
/* PoP: string @ cli.py:_prompt_text_input */
/* PoP: string @ cli.py:_prompt_text_input_modal */
/* PoP: string @ cli.py:_submit_slash_confirm_response */
/* PoP: string @ cli.py:_normalize_slash_confirm_choice */
/* PoP: string @ cli.py:_get_slash_confirm_display_fragments */
/* PoP: string @ cli.py:_open_model_picker */
/* PoP: string @ cli.py:_confirm_expensive_model_switch */
/* PoP: string @ cli.py:_confirm_and_apply_model_switch_result */
/* PoP: string @ cli.py:_close_model_picker */
/* PoP: string @ cli.py:_compute_model_picker_viewport */
/* PoP: string @ cli.py:_apply_model_switch_result */
/* PoP: string @ cli.py:_handle_model_picker_selection */
/* PoP: string @ cli.py:_handle_model_switch */
/* PoP: string @ cli.py:_handle_codex_runtime */
/* PoP: string @ cli.py:_should_handle_model_command_inline */
/* PoP: string @ cli.py:_should_handle_steer_command_inline */
/* PoP: string @ cli.py:_output_console */
/* PoP: string @ cli.py:_console_print */
/* PoP: string @ cli.py:_resolve_personality_prompt */
/* PoP: string @ cli.py:_show_gateway_status */
/* PoP: string @ cli.py:process_command */
/* PoP: string @ cli.py:_try_launch_chrome_debug */
/* PoP: string @ cli.py:_get_goal_manager */
/* PoP: string @ cli.py:_maybe_continue_goal_after_turn */
/* PoP: string @ cli.py:_toggle_verbose */
/* PoP: string @ cli.py:_transfer_session_yolo */
/* PoP: string @ cli.py:_is_session_yolo_active */
/* PoP: string @ cli.py:_toggle_yolo */
/* PoP: string @ cli.py:_on_reasoning */
/* PoP: string @ cli.py:_manual_compress */
/* PoP: string @ cli.py:_show_usage */
/* PoP: string @ cli.py:_print_nous_credits_block */
/* PoP: string @ cli.py:_show_credits */
/* PoP: string @ cli.py:_show_insights */
/* PoP: string @ cli.py:_check_config_mcp_changes */
/* PoP: string @ cli.py:_split_destructive_skip */
/* PoP: string @ cli.py:_confirm_destructive_slash */
/* PoP: string @ cli.py:_confirm_and_reload_mcp */
/* PoP: string @ cli.py:_on_tool_gen_start */
/* PoP: string @ cli.py:_on_tool_progress */
/* PoP: string @ cli.py:_on_tool_start */
/* PoP: string @ cli.py:_on_tool_complete */
/* PoP: string @ cli.py:_voice_start_recording */
/* PoP: string @ cli.py:_voice_stop_and_transcribe */
/* PoP: string @ cli.py:_voice_speak_response_async */
/* PoP: string @ cli.py:_voice_speak_response */
/* PoP: string @ cli.py:_voice_beeps_enabled */
/* PoP: string @ cli.py:_enable_voice_mode */
/* PoP: string @ cli.py:_disable_voice_mode */
/* PoP: string @ cli.py:_toggle_voice_tts */
/* PoP: string @ cli.py:_show_voice_status */
/* PoP: string @ cli.py:_persist_prompt_summary */
/* PoP: string @ cli.py:_clarify_callback */
/* PoP: string @ cli.py:_sudo_password_callback */
/* PoP: string @ cli.py:_approval_callback */
/* PoP: string @ cli.py:_approval_choices */
/* PoP: string @ cli.py:_computer_use_approval_callback */
/* PoP: string @ cli.py:_handle_approval_selection */
/* PoP: string @ cli.py:_get_approval_display_fragments */
/* PoP: string @ cli.py:_secret_capture_callback */
/* PoP: string @ cli.py:_capture_modal_input_snapshot */
/* PoP: string @ cli.py:_restore_modal_input_snapshot */
/* PoP: string @ cli.py:_submit_secret_response */
/* PoP: string @ cli.py:_cancel_secret_capture */
/* PoP: string @ cli.py:_clear_secret_input_buffer */
/* PoP: string @ cli.py:_clear_terminal_on_exit */
/* PoP: string @ cli.py:_print_exit_summary */
/* PoP: string @ cli.py:_get_tui_prompt_symbols */
/* PoP: string @ cli.py:_audio_level_bar */
/* PoP: string @ cli.py:_get_tui_prompt_fragments */
/* PoP: string @ cli.py:_get_tui_prompt_text */
/* PoP: string @ cli.py:_build_tui_style_dict */
/* PoP: string @ cli.py:_apply_tui_skin_style */
/* PoP: string @ cli.py:_get_extra_tui_widgets */
/* PoP: string @ cli.py:_register_extra_tui_keybindings */
/* PoP: string @ cli.py:_build_tui_layout_children */
/* PoP: string @ cli.py:run */
/* PoP: string @ cli.py:_run_kanban_goal_loop_q */

/* Port of Python cli:CanonicalUsage */

/* Port of Python cli:_strip_reasoning_tags */

/* Port of Python cli:_assistant_content_as_text */

/* Port of Python cli:_assistant_copy_text */

/* Port of Python cli:_load_prefill_messages */

/* Port of Python cli:_resolve_prefill_messages_file */

/* Port of Python cli:_parse_reasoning_config */

/* Port of Python cli:_parse_service_tier_config */

/* Port of Python cli:load_cli_config */

/* Port of Python cli:find_spec */


/* Port of Python cli:AIAgent */

/* Port of Python cli:get_tool_definitions */

/* Port of Python cli:get_toolset_for_tool */

/* Port of Python cli:get_all_toolsets */

/* Port of Python cli:get_toolset_info */

/* Port of Python cli:validate_toolset */

/* Port of Python cli:_sync_process_session_id */

/* Port of Python cli:get_job */

/* Port of Python cli:_cleanup_all_terminals */

/* Port of Python cli:set_sudo_password_callback */

/* Port of Python cli:set_approval_callback */

/* Port of Python cli:set_secret_capture_callback */

/* Port of Python cli:_cleanup_all_browsers */

/* Port of Python cli:_mark_tui_input_modes_active */


/* Port of Python cli:_prepare_deferred_agent_startup */

/* Port of Python cli:_run_cleanup */

/* Port of Python cli:_should_emit_cleanup_session_finalize */

/* Port of Python cli:_notify_session_finalize */

/* Port of Python cli:_emit_interrupted_session_end */

/* Port of Python cli:_notify_single_query_session_finalize */

/* Port of Python cli:_finalize_single_query */

/* Port of Python cli:_reset_terminal_input_modes_on_exit */

/* Port of Python cli:_normalize_git_bash_path */

/* Port of Python cli:_git_repo_root */

/* Port of Python cli:_path_is_within_root */

/* Port of Python cli:_setup_worktree */

/* Port of Python cli:_worktree_has_unpushed_commits */

/* Port of Python cli:_cleanup_worktree */

/* Port of Python cli:_run_state_db_auto_maintenance */

/* Port of Python cli:_run_checkpoint_auto_maintenance */

/* Port of Python cli:_prune_stale_worktrees */

/* Port of Python cli:_prune_orphaned_branches */

/* Port of Python cli:_hex_to_ansi */

/* Port of Python cli:_luminance_from_hex */

/* Port of Python cli:_query_osc11_background */

/* Port of Python cli:_detect_light_mode */

/* Port of Python cli:_maybe_remap_for_light_mode */

/* Port of Python cli:_install_skin_light_mode_hook */

/* Port of Python cli:__str__ */

/* Port of Python cli:__add__ */

/* Port of Python cli:__radd__ */

/* Port of Python cli:_rich_text_from_ansi */

/* Port of Python cli:_strip_markdown_syntax */

/* Port of Python cli:_preserve_windows_dot_segments_for_markdown */

/* Port of Python cli:_terminal_width_for_streaming */

/* Port of Python cli:_render_final_assistant_content */

/* Port of Python cli:_coerce_output_history_limit */

/* Port of Python cli:_configure_output_history */

/* Port of Python cli:_clear_output_history */

/* Port of Python cli:_suspend_output_history */

/* Port of Python cli:_record_output_history_entry */

/* Port of Python cli:_record_output_history */

/* Port of Python cli:_replay_output_history */

/* Port of Python cli:_cprint */

/* Port of Python cli:_prepend_note_to_message */

/* Port of Python cli:_termux_example_image_path */

/* Port of Python cli:_split_path_input */

/* Port of Python cli:_resolve_attachment_path */

/* Port of Python cli:_detect_file_drop */

/* Port of Python cli:_format_image_attachment_badges */

/* Port of Python cli:_should_auto_attach_clipboard_image_on_paste */

/* Port of Python cli:_strip_leaked_bracketed_paste_wrappers */

/* Port of Python cli:_apply_bracketed_paste_timeout_patch */

/* Port of Python cli:_preserve_ctrl_enter_newline */

/* Port of Python cli:_bind_prompt_submit_keys */

/* Port of Python cli:_disable_prompt_toolkit_cpr_warning */

/* Port of Python cli:_strip_leaked_terminal_responses_with_meta */

/* Port of Python cli:_strip_leaked_terminal_responses */

/* Port of Python cli:_estimate_tui_input_height */

/* Port of Python cli:_collect_query_images */

/* Port of Python cli:_build_compact_banner */

/* Port of Python cli:_looks_like_slash_command */

/* Port of Python cli:_ensure_skill_commands */

/* Port of Python cli.py:build_skill_invocation_message */
/* Build the user message content for a skill slash command invocation.
 * Returns malloc'd string (caller must free) or NULL if skill not found. */
char *cli_cli_build_skill_invocation_message(const char *cmd_key,
                                             const char *user_instruction,
                                             const char *task_id,
                                             const char *runtime_note)
{
    (void)task_id;
    (void)runtime_note;
    if (!cmd_key || !cmd_key[0]) return NULL;

    /* Ensure skills are scanned */
    skill_cmd_scan();

    /* Look up the skill by slug */
    const skill_cmd_entry_t *sk = skill_cmd_get(cmd_key);
    if (!sk) return NULL;

    /* Load the skill payload */
    skill_cmd_payload_t *payload = load_skill_payload(cmd_key);
    if (!payload) return NULL;

    /* Build the activation note */
    char activation[2048];
    snprintf(activation, sizeof(activation),
        "[IMPORTANT: The user has invoked the \"%s\" skill, indicating they want "
        "you to follow its instructions. The full skill content is loaded below.]",
        payload->skill_name);

    /* Build the full message */
    size_t total = strlen(activation) + strlen(payload->body) + strlen(user_instruction) + 4096;
    char *msg = (char *)malloc(total);
    if (!msg) { skill_payload_free(payload); return NULL; }

    if (user_instruction && user_instruction[0]) {
        snprintf(msg, total,
            "%s\n\n%s\n\n[User instruction: %s]\n\n[Skill directory: %s]\n"
            "Resolve any relative paths in this skill (e.g. scripts/foo.js, "
            "templates/config.yaml) against that directory, then run them "
            "with the terminal tool using the absolute path.\n",
            activation, payload->body, user_instruction, payload->skill_dir);
    } else {
        snprintf(msg, total,
            "%s\n\n%s\n\n[Skill directory: %s]\n"
            "Resolve any relative paths in this skill (e.g. scripts/foo.js, "
            "templates/config.yaml) against that directory, then run them "
            "with the terminal tool using the absolute path.\n",
            activation, payload->body, payload->skill_dir);
    }

    skill_payload_free(payload);
    return msg;
}

/* Port of Python cli:_get_plugin_cmd_handler_names */

/* Port of Python cli:_parse_skills_argument */

/* Port of Python cli:save_config_value */

/* Port of Python cli:_claim_active_session */

/* Port of Python cli:_release_active_session */

/* Port of Python cli:_invalidate */

/* Port of Python cli:_paint_now */

/* Port of Python cli:_force_full_redraw */

/* Port of Python cli:_clear_prompt_toolkit_screen */

/* Port of Python cli:_recover_after_resize */

/* Port of Python cli:_schedule_resize_recovery */

/* Port of Python cli:_status_bar_context_style */

/* Port of Python cli:_compression_count_style */

/* Port of Python cli:_build_context_bar */

/* Port of Python cli:_format_prompt_elapsed */

/* Port of Python cli:_format_idle_since */

/* Port of Python cli:_get_status_bar_snapshot */

/* Port of Python cli:_status_bar_display_width */

/* Port of Python cli:_trim_status_bar_text */

/* Port of Python cli:_get_tui_terminal_width */

/* Port of Python cli:_use_minimal_tui_chrome */

/* Port of Python cli:_scrollback_box_width */

/* Port of Python cli:_tui_input_rule_height */

/* Port of Python cli:_spinner_widget_height */

/* Port of Python cli:_render_spinner_text */

/* Port of Python cli:_voice_record_key_label */

/* Port of Python cli:set_voice_record_key_cache */

/* Port of Python cli:_get_voice_status_fragments */

/* Port of Python cli:_build_status_bar_text */

/* Port of Python cli:_get_status_bar_fragments */

/* Port of Python cli:_normalize_model_for_provider */

/* Port of Python cli:_on_thinking */

/* Port of Python cli:_on_notice */

/* Port of Python cli:_flush_credit_notices */

/* Port of Python cli:_on_notice_clear */


/* Port of Python cli:_current_reasoning_callback */

/* Port of Python cli:_emit_reasoning_preview */

/* Port of Python cli:_flush_reasoning_preview */

/* Port of Python cli:_format_submitted_user_message_preview */

/* Port of Python cli:_expand_paste_references */

/* Port of Python cli:_print_user_message_preview */

/* Port of Python cli:_stream_reasoning_delta */

/* Port of Python cli:_close_reasoning_box */

/* Port of Python cli:_stream_delta */

/* Port of Python cli:_emit_stream_text */

/* Port of Python cli:_flush_stream */

/* Port of Python cli:_reset_stream_state */

/* Port of Python cli:_slow_command_status */

/* Port of Python cli:_command_spinner_frame */

/* Port of Python cli:_busy_command */

/* Port of Python cli:_open_external_editor */

/* Port of Python cli:_install_tool_callbacks */

/* Port of Python cli:_ensure_tirith_security */

/* Port of Python cli:_show_security_advisories */

/* Port of Python cli:show_banner */

/* Port of Python cli:_restore_session_cwd */

/* Port of Python cli:_render_resume_history_panel_lines */

/* Port of Python cli:_try_attach_clipboard_image */

/* Port of Python cli:_resolve_checkpoint_ref */

/* Port of Python cli:_write_osc52_clipboard */

/* Port of Python cli:_recover_terminal_input_modes */

/* Port of Python cli:_preprocess_images_with_vision */

/* Port of Python cli:_show_tool_availability_warnings */

/* Port of Python cli:_show_status */

/* Port of Python cli:_show_session_status */

/* Port of Python cli:_fast_command_available */

/* Port of Python cli:_command_available */

/* Port of Python cli:show_help */

/* Port of Python cli:show_tools */

/* Port of Python cli:show_toolsets */

/* Port of Python cli:_list_recent_sessions */

/* Port of Python cli:_show_recent_sessions */

/* Port of Python cli:show_history */

/* Port of Python cli:_discard_session_if_empty */

/* Port of Python cli:new_session */

/* Port of Python cli:_consume_pending_resume_selection */

/* Port of Python cli:save_conversation */

/* Port of Python cli:retry_last */

/* Port of Python cli:undo_last */

/* Port of Python cli:_undo_content_to_text */

/* Port of Python cli:_prefill_input_buffer */

/* Port of Python cli:_run_curses_picker */

/* Port of Python cli:_prompt_text_input */

/* Port of Python cli:_prompt_text_input_modal */

/* Port of Python cli:_submit_slash_confirm_response */

/* Port of Python cli:_normalize_slash_confirm_choice */

/* Port of Python cli:_get_slash_confirm_display_fragments */

/* Port of Python cli:_open_model_picker */

/* Port of Python cli:_confirm_expensive_model_switch */

/* Port of Python cli:_confirm_and_apply_model_switch_result */

/* Port of Python cli:_close_model_picker */

/* Port of Python cli:_compute_model_picker_viewport */

/* Port of Python cli:_apply_model_switch_result */

/* Port of Python cli:_handle_model_picker_selection */

/* Port of Python cli:_handle_model_switch */

/* Port of Python cli:_handle_codex_runtime */

/* Port of Python cli:_should_handle_model_command_inline */

/* Port of Python cli:_should_handle_steer_command_inline */

/* Port of Python cli:_output_console */

/* Port of Python cli:_console_print */

/* Port of Python cli:_resolve_personality_prompt */

/* Port of Python cli:_show_gateway_status */

/* Port of Python cli:process_command */

/* Port of Python cli:_try_launch_chrome_debug */

/* Port of Python cli:_get_goal_manager */

/* Port of Python cli:_maybe_continue_goal_after_turn */

/* Port of Python cli:_toggle_verbose */

/* Port of Python cli:_transfer_session_yolo */

/* Port of Python cli:_is_session_yolo_active */

/* Port of Python cli:_toggle_yolo */

/* Port of Python cli:_on_reasoning */

/* Port of Python cli:_manual_compress */

/* Port of Python cli:_show_usage */

/* Port of Python cli:_print_nous_credits_block */

/* Port of Python cli:_show_credits */

/* Port of Python cli:_show_insights */

/* Port of Python cli:_check_config_mcp_changes */

/* Port of Python cli:_split_destructive_skip */

/* Port of Python cli:_confirm_destructive_slash */

/* Port of Python cli:_confirm_and_reload_mcp */

/* Port of Python cli:_on_tool_gen_start */

/* Port of Python cli:_on_tool_progress */

/* Port of Python cli:_on_tool_start */

/* Port of Python cli:_on_tool_complete */

/* Port of Python cli:_voice_start_recording */

/* Port of Python cli:_voice_stop_and_transcribe */

/* Port of Python cli:_voice_speak_response_async */

/* Port of Python cli:_voice_speak_response */

/* Port of Python cli:_voice_beeps_enabled */

/* Port of Python cli:_enable_voice_mode */

/* Port of Python cli:_disable_voice_mode */

/* Port of Python cli:_toggle_voice_tts */

/* Port of Python cli:_show_voice_status */

/* Port of Python cli:_persist_prompt_summary */

/* Port of Python cli:_clarify_callback */

/* Port of Python cli:_sudo_password_callback */

/* Port of Python cli:_approval_callback */

/* Port of Python cli:_approval_choices */

/* Port of Python cli:_computer_use_approval_callback */

/* Port of Python cli:_handle_approval_selection */

/* Port of Python cli:_get_approval_display_fragments */

/* Port of Python cli:_secret_capture_callback */

/* Port of Python cli:_capture_modal_input_snapshot */

/* Port of Python cli:_restore_modal_input_snapshot */

/* Port of Python cli:_submit_secret_response */

/* Port of Python cli:_cancel_secret_capture */

/* Port of Python cli:_clear_secret_input_buffer */

/* Port of Python cli:_clear_terminal_on_exit */

/* Port of Python cli:_print_exit_summary */

/* Port of Python cli:_get_tui_prompt_symbols */

/* Port of Python cli:_audio_level_bar */

/* Port of Python cli:_get_tui_prompt_fragments */

/* Port of Python cli:_get_tui_prompt_text */

/* Port of Python cli:_build_tui_style_dict */

/* Port of Python cli:_apply_tui_skin_style */

/* Port of Python cli:_get_extra_tui_widgets */

/* Port of Python cli:_register_extra_tui_keybindings */


/* Port of Python cli:_build_tui_layout_children */

/* Port of Python cli:run */

/* Port of Python cli:_run_kanban_goal_loop_q */