/*
 * port_cli.c — C port of cli.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include "hermes_skill_commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_cli_CanonicalUsage @ cli.py:CanonicalUsage */
/* PoP: cli_cli__strip_reasoning_tags @ cli.py:_strip_reasoning_tags */
/* PoP: cli_cli__assistant_content_as_text @ cli.py:_assistant_content_as_text */
/* PoP: cli_cli__assistant_copy_text @ cli.py:_assistant_copy_text */
/* PoP: cli_cli__load_prefill_messages @ cli.py:_load_prefill_messages */
/* PoP: cli_cli__resolve_prefill_messages_file @ cli.py:_resolve_prefill_messages_file */
/* PoP: cli_cli__parse_reasoning_config @ cli.py:_parse_reasoning_config */
/* PoP: cli_cli__parse_service_tier_config @ cli.py:_parse_service_tier_config */
/* PoP: cli_cli_load_cli_config @ cli.py:load_cli_config */
/* PoP: cli_cli_find_spec @ cli.py:find_spec */
/* PoP: cli_cli_AIAgent @ cli.py:AIAgent */
/* PoP: cli_cli_get_tool_definitions @ cli.py:get_tool_definitions */
/* PoP: cli_cli_get_toolset_for_tool @ cli.py:get_toolset_for_tool */
/* PoP: cli_cli_get_all_toolsets @ cli.py:get_all_toolsets */
/* PoP: cli_cli_get_toolset_info @ cli.py:get_toolset_info */
/* PoP: cli_cli_validate_toolset @ cli.py:validate_toolset */
/* PoP: cli_cli__sync_process_session_id @ cli.py:_sync_process_session_id */
/* PoP: cli_cli_get_job @ cli.py:get_job */
/* PoP: cli_cli__cleanup_all_terminals @ cli.py:_cleanup_all_terminals */
/* PoP: cli_cli_set_sudo_password_callback @ cli.py:set_sudo_password_callback */
/* PoP: cli_cli_set_approval_callback @ cli.py:set_approval_callback */
/* PoP: cli_cli_set_secret_capture_callback @ cli.py:set_secret_capture_callback */
/* PoP: cli_cli__cleanup_all_browsers @ cli.py:_cleanup_all_browsers */
/* PoP: cli_cli__mark_tui_input_modes_active @ cli.py:_mark_tui_input_modes_active */
/* PoP: cli_cli__prepare_deferred_agent_startup @ cli.py:_prepare_deferred_agent_startup */
/* PoP: cli_cli__run_cleanup @ cli.py:_run_cleanup */
/* PoP: cli_cli__should_emit_cleanup_session_finalize @ cli.py:_should_emit_cleanup_session_finalize */
/* PoP: cli_cli__notify_session_finalize @ cli.py:_notify_session_finalize */
/* PoP: cli_cli__emit_interrupted_session_end @ cli.py:_emit_interrupted_session_end */
/* PoP: cli_cli__notify_single_query_session_finalize @ cli.py:_notify_single_query_session_finalize */
/* PoP: cli_cli__finalize_single_query @ cli.py:_finalize_single_query */
/* PoP: cli_cli__reset_terminal_input_modes_on_exit @ cli.py:_reset_terminal_input_modes_on_exit */
/* PoP: cli_cli__normalize_git_bash_path @ cli.py:_normalize_git_bash_path */
/* PoP: cli_cli__git_repo_root @ cli.py:_git_repo_root */
/* PoP: cli_cli__path_is_within_root @ cli.py:_path_is_within_root */
/* PoP: cli_cli__setup_worktree @ cli.py:_setup_worktree */
/* PoP: cli_cli__worktree_has_unpushed_commits @ cli.py:_worktree_has_unpushed_commits */
/* PoP: cli_cli__cleanup_worktree @ cli.py:_cleanup_worktree */
/* PoP: cli_cli__run_state_db_auto_maintenance @ cli.py:_run_state_db_auto_maintenance */
/* PoP: cli_cli__run_checkpoint_auto_maintenance @ cli.py:_run_checkpoint_auto_maintenance */
/* PoP: cli_cli__prune_stale_worktrees @ cli.py:_prune_stale_worktrees */
/* PoP: cli_cli__prune_orphaned_branches @ cli.py:_prune_orphaned_branches */
/* PoP: cli_cli__hex_to_ansi @ cli.py:_hex_to_ansi */
/* PoP: cli_cli__luminance_from_hex @ cli.py:_luminance_from_hex */
/* PoP: cli_cli__query_osc11_background @ cli.py:_query_osc11_background */
/* PoP: cli_cli__detect_light_mode @ cli.py:_detect_light_mode */
/* PoP: cli_cli__maybe_remap_for_light_mode @ cli.py:_maybe_remap_for_light_mode */
/* PoP: cli_cli__install_skin_light_mode_hook @ cli.py:_install_skin_light_mode_hook */
/* PoP: cli_cli___str__ @ cli.py:__str__ */
/* PoP: cli_cli___add__ @ cli.py:__add__ */
/* PoP: cli_cli___radd__ @ cli.py:__radd__ */
/* PoP: cli_cli__rich_text_from_ansi @ cli.py:_rich_text_from_ansi */
/* PoP: cli_cli__strip_markdown_syntax @ cli.py:_strip_markdown_syntax */
/* PoP: cli_cli__preserve_windows_dot_segments_for_markdown @ cli.py:_preserve_windows_dot_segments_for_markdown */
/* PoP: cli_cli__terminal_width_for_streaming @ cli.py:_terminal_width_for_streaming */
/* PoP: cli_cli__render_final_assistant_content @ cli.py:_render_final_assistant_content */
/* PoP: cli_cli__coerce_output_history_limit @ cli.py:_coerce_output_history_limit */
/* PoP: cli_cli__configure_output_history @ cli.py:_configure_output_history */
/* PoP: cli_cli__clear_output_history @ cli.py:_clear_output_history */
/* PoP: cli_cli__suspend_output_history @ cli.py:_suspend_output_history */
/* PoP: cli_cli__record_output_history_entry @ cli.py:_record_output_history_entry */
/* PoP: cli_cli__record_output_history @ cli.py:_record_output_history */
/* PoP: cli_cli__replay_output_history @ cli.py:_replay_output_history */
/* PoP: cli_cli__cprint @ cli.py:_cprint */
/* PoP: cli_cli__prepend_note_to_message @ cli.py:_prepend_note_to_message */
/* PoP: cli_cli__termux_example_image_path @ cli.py:_termux_example_image_path */
/* PoP: cli_cli__split_path_input @ cli.py:_split_path_input */
/* PoP: cli_cli__resolve_attachment_path @ cli.py:_resolve_attachment_path */
/* PoP: cli_cli__detect_file_drop @ cli.py:_detect_file_drop */
/* PoP: cli_cli__format_image_attachment_badges @ cli.py:_format_image_attachment_badges */
/* PoP: cli_cli__should_auto_attach_clipboard_image_on_paste @ cli.py:_should_auto_attach_clipboard_image_on_paste */
/* PoP: cli_cli__strip_leaked_bracketed_paste_wrappers @ cli.py:_strip_leaked_bracketed_paste_wrappers */
/* PoP: cli_cli__apply_bracketed_paste_timeout_patch @ cli.py:_apply_bracketed_paste_timeout_patch */
/* PoP: cli_cli__preserve_ctrl_enter_newline @ cli.py:_preserve_ctrl_enter_newline */
/* PoP: cli_cli__bind_prompt_submit_keys @ cli.py:_bind_prompt_submit_keys */
/* PoP: cli_cli__disable_prompt_toolkit_cpr_warning @ cli.py:_disable_prompt_toolkit_cpr_warning */
/* PoP: cli_cli__strip_leaked_terminal_responses_with_meta @ cli.py:_strip_leaked_terminal_responses_with_meta */
/* PoP: cli_cli__strip_leaked_terminal_responses @ cli.py:_strip_leaked_terminal_responses */
/* PoP: cli_cli__estimate_tui_input_height @ cli.py:_estimate_tui_input_height */
/* PoP: cli_cli__collect_query_images @ cli.py:_collect_query_images */
/* PoP: cli_cli__build_compact_banner @ cli.py:_build_compact_banner */
/* PoP: cli_cli__looks_like_slash_command @ cli.py:_looks_like_slash_command */
/* PoP: cli_cli__ensure_skill_commands @ cli.py:_ensure_skill_commands */
/* PoP: cli_cli_build_skill_invocation_message @ cli.py:build_skill_invocation_message */
/* PoP: cli_cli__get_plugin_cmd_handler_names @ cli.py:_get_plugin_cmd_handler_names */
/* PoP: cli_cli__parse_skills_argument @ cli.py:_parse_skills_argument */
/* PoP: cli_cli_save_config_value @ cli.py:save_config_value */
/* PoP: cli_cli__claim_active_session @ cli.py:_claim_active_session */
/* PoP: cli_cli__release_active_session @ cli.py:_release_active_session */
/* PoP: cli_cli__invalidate @ cli.py:_invalidate */
/* PoP: cli_cli__paint_now @ cli.py:_paint_now */
/* PoP: cli_cli__force_full_redraw @ cli.py:_force_full_redraw */
/* PoP: cli_cli__clear_prompt_toolkit_screen @ cli.py:_clear_prompt_toolkit_screen */
/* PoP: cli_cli__recover_after_resize @ cli.py:_recover_after_resize */
/* PoP: cli_cli__schedule_resize_recovery @ cli.py:_schedule_resize_recovery */
/* PoP: cli_cli__status_bar_context_style @ cli.py:_status_bar_context_style */
/* PoP: cli_cli__compression_count_style @ cli.py:_compression_count_style */
/* PoP: cli_cli__build_context_bar @ cli.py:_build_context_bar */
/* PoP: cli_cli__format_prompt_elapsed @ cli.py:_format_prompt_elapsed */
/* PoP: cli_cli__format_idle_since @ cli.py:_format_idle_since */
/* PoP: cli_cli__get_status_bar_snapshot @ cli.py:_get_status_bar_snapshot */
/* PoP: cli_cli__status_bar_display_width @ cli.py:_status_bar_display_width */
/* PoP: cli_cli__trim_status_bar_text @ cli.py:_trim_status_bar_text */
/* PoP: cli_cli__get_tui_terminal_width @ cli.py:_get_tui_terminal_width */
/* PoP: cli_cli__use_minimal_tui_chrome @ cli.py:_use_minimal_tui_chrome */
/* PoP: cli_cli__scrollback_box_width @ cli.py:_scrollback_box_width */
/* PoP: cli_cli__tui_input_rule_height @ cli.py:_tui_input_rule_height */
/* PoP: cli_cli__spinner_widget_height @ cli.py:_spinner_widget_height */
/* PoP: cli_cli__render_spinner_text @ cli.py:_render_spinner_text */
/* PoP: cli_cli__voice_record_key_label @ cli.py:_voice_record_key_label */
/* PoP: cli_cli_set_voice_record_key_cache @ cli.py:set_voice_record_key_cache */
/* PoP: cli_cli__get_voice_status_fragments @ cli.py:_get_voice_status_fragments */
/* PoP: cli_cli__build_status_bar_text @ cli.py:_build_status_bar_text */
/* PoP: cli_cli__get_status_bar_fragments @ cli.py:_get_status_bar_fragments */
/* PoP: cli_cli__normalize_model_for_provider @ cli.py:_normalize_model_for_provider */
/* PoP: cli_cli__on_thinking @ cli.py:_on_thinking */
/* PoP: cli_cli__on_notice @ cli.py:_on_notice */
/* PoP: cli_cli__flush_credit_notices @ cli.py:_flush_credit_notices */
/* PoP: cli_cli__on_notice_clear @ cli.py:_on_notice_clear */
/* PoP: cli_cli__current_reasoning_callback @ cli.py:_current_reasoning_callback */
/* PoP: cli_cli__emit_reasoning_preview @ cli.py:_emit_reasoning_preview */
/* PoP: cli_cli__flush_reasoning_preview @ cli.py:_flush_reasoning_preview */
/* PoP: cli_cli__format_submitted_user_message_preview @ cli.py:_format_submitted_user_message_preview */
/* PoP: cli_cli__expand_paste_references @ cli.py:_expand_paste_references */
/* PoP: cli_cli__print_user_message_preview @ cli.py:_print_user_message_preview */
/* PoP: cli_cli__stream_reasoning_delta @ cli.py:_stream_reasoning_delta */
/* PoP: cli_cli__close_reasoning_box @ cli.py:_close_reasoning_box */
/* PoP: cli_cli__stream_delta @ cli.py:_stream_delta */
/* PoP: cli_cli__emit_stream_text @ cli.py:_emit_stream_text */
/* PoP: cli_cli__flush_stream @ cli.py:_flush_stream */
/* PoP: cli_cli__reset_stream_state @ cli.py:_reset_stream_state */
/* PoP: cli_cli__slow_command_status @ cli.py:_slow_command_status */
/* PoP: cli_cli__command_spinner_frame @ cli.py:_command_spinner_frame */
/* PoP: cli_cli__busy_command @ cli.py:_busy_command */
/* PoP: cli_cli__open_external_editor @ cli.py:_open_external_editor */
/* PoP: cli_cli__install_tool_callbacks @ cli.py:_install_tool_callbacks */
/* PoP: cli_cli__ensure_tirith_security @ cli.py:_ensure_tirith_security */
/* PoP: cli_cli__show_security_advisories @ cli.py:_show_security_advisories */
/* PoP: cli_cli_show_banner @ cli.py:show_banner */
/* PoP: cli_cli__restore_session_cwd @ cli.py:_restore_session_cwd */
/* PoP: cli_cli__render_resume_history_panel_lines @ cli.py:_render_resume_history_panel_lines */
/* PoP: cli_cli__try_attach_clipboard_image @ cli.py:_try_attach_clipboard_image */
/* PoP: cli_cli__resolve_checkpoint_ref @ cli.py:_resolve_checkpoint_ref */
/* PoP: cli_cli__write_osc52_clipboard @ cli.py:_write_osc52_clipboard */
/* PoP: cli_cli__recover_terminal_input_modes @ cli.py:_recover_terminal_input_modes */
/* PoP: cli_cli__preprocess_images_with_vision @ cli.py:_preprocess_images_with_vision */
/* PoP: cli_cli__show_tool_availability_warnings @ cli.py:_show_tool_availability_warnings */
/* PoP: cli_cli__show_status @ cli.py:_show_status */
/* PoP: cli_cli__show_session_status @ cli.py:_show_session_status */
/* PoP: cli_cli__fast_command_available @ cli.py:_fast_command_available */
/* PoP: cli_cli__command_available @ cli.py:_command_available */
/* PoP: cli_cli_show_help @ cli.py:show_help */
/* PoP: cli_cli_show_tools @ cli.py:show_tools */
/* PoP: cli_cli_show_toolsets @ cli.py:show_toolsets */
/* PoP: cli_cli__list_recent_sessions @ cli.py:_list_recent_sessions */
/* PoP: cli_cli__show_recent_sessions @ cli.py:_show_recent_sessions */
/* PoP: cli_cli_show_history @ cli.py:show_history */
/* PoP: cli_cli__discard_session_if_empty @ cli.py:_discard_session_if_empty */
/* PoP: cli_cli_new_session @ cli.py:new_session */
/* PoP: cli_cli__consume_pending_resume_selection @ cli.py:_consume_pending_resume_selection */
/* PoP: cli_cli_save_conversation @ cli.py:save_conversation */
/* PoP: cli_cli_retry_last @ cli.py:retry_last */
/* PoP: cli_cli_undo_last @ cli.py:undo_last */
/* PoP: cli_cli__undo_content_to_text @ cli.py:_undo_content_to_text */
/* PoP: cli_cli__prefill_input_buffer @ cli.py:_prefill_input_buffer */
/* PoP: cli_cli__run_curses_picker @ cli.py:_run_curses_picker */
/* PoP: cli_cli__prompt_text_input @ cli.py:_prompt_text_input */
/* PoP: cli_cli__prompt_text_input_modal @ cli.py:_prompt_text_input_modal */
/* PoP: cli_cli__submit_slash_confirm_response @ cli.py:_submit_slash_confirm_response */
/* PoP: cli_cli__normalize_slash_confirm_choice @ cli.py:_normalize_slash_confirm_choice */
/* PoP: cli_cli__get_slash_confirm_display_fragments @ cli.py:_get_slash_confirm_display_fragments */
/* PoP: cli_cli__open_model_picker @ cli.py:_open_model_picker */
/* PoP: cli_cli__confirm_expensive_model_switch @ cli.py:_confirm_expensive_model_switch */
/* PoP: cli_cli__confirm_and_apply_model_switch_result @ cli.py:_confirm_and_apply_model_switch_result */
/* PoP: cli_cli__close_model_picker @ cli.py:_close_model_picker */
/* PoP: cli_cli__compute_model_picker_viewport @ cli.py:_compute_model_picker_viewport */
/* PoP: cli_cli__apply_model_switch_result @ cli.py:_apply_model_switch_result */
/* PoP: cli_cli__handle_model_picker_selection @ cli.py:_handle_model_picker_selection */
/* PoP: cli_cli__handle_model_switch @ cli.py:_handle_model_switch */
/* PoP: cli_cli__handle_codex_runtime @ cli.py:_handle_codex_runtime */
/* PoP: cli_cli__should_handle_model_command_inline @ cli.py:_should_handle_model_command_inline */
/* PoP: cli_cli__should_handle_steer_command_inline @ cli.py:_should_handle_steer_command_inline */
/* PoP: cli_cli__output_console @ cli.py:_output_console */
/* PoP: cli_cli__console_print @ cli.py:_console_print */
/* PoP: cli_cli__resolve_personality_prompt @ cli.py:_resolve_personality_prompt */
/* PoP: cli_cli__show_gateway_status @ cli.py:_show_gateway_status */
/* PoP: cli_cli_process_command @ cli.py:process_command */
/* PoP: cli_cli__try_launch_chrome_debug @ cli.py:_try_launch_chrome_debug */
/* PoP: cli_cli__get_goal_manager @ cli.py:_get_goal_manager */
/* PoP: cli_cli__maybe_continue_goal_after_turn @ cli.py:_maybe_continue_goal_after_turn */
/* PoP: cli_cli__toggle_verbose @ cli.py:_toggle_verbose */
/* PoP: cli_cli__transfer_session_yolo @ cli.py:_transfer_session_yolo */
/* PoP: cli_cli__is_session_yolo_active @ cli.py:_is_session_yolo_active */
/* PoP: cli_cli__toggle_yolo @ cli.py:_toggle_yolo */
/* PoP: cli_cli__on_reasoning @ cli.py:_on_reasoning */
/* PoP: cli_cli__manual_compress @ cli.py:_manual_compress */
/* PoP: cli_cli__show_usage @ cli.py:_show_usage */
/* PoP: cli_cli__print_nous_credits_block @ cli.py:_print_nous_credits_block */
/* PoP: cli_cli__show_credits @ cli.py:_show_credits */
/* PoP: cli_cli__show_insights @ cli.py:_show_insights */
/* PoP: cli_cli__check_config_mcp_changes @ cli.py:_check_config_mcp_changes */
/* PoP: cli_cli__split_destructive_skip @ cli.py:_split_destructive_skip */
/* PoP: cli_cli__confirm_destructive_slash @ cli.py:_confirm_destructive_slash */
/* PoP: cli_cli__confirm_and_reload_mcp @ cli.py:_confirm_and_reload_mcp */
/* PoP: cli_cli__on_tool_gen_start @ cli.py:_on_tool_gen_start */
/* PoP: cli_cli__on_tool_progress @ cli.py:_on_tool_progress */
/* PoP: cli_cli__on_tool_start @ cli.py:_on_tool_start */
/* PoP: cli_cli__on_tool_complete @ cli.py:_on_tool_complete */
/* PoP: cli_cli__voice_start_recording @ cli.py:_voice_start_recording */
/* PoP: cli_cli__voice_stop_and_transcribe @ cli.py:_voice_stop_and_transcribe */
/* PoP: cli_cli__voice_speak_response_async @ cli.py:_voice_speak_response_async */
/* PoP: cli_cli__voice_speak_response @ cli.py:_voice_speak_response */
/* PoP: cli_cli__voice_beeps_enabled @ cli.py:_voice_beeps_enabled */
/* PoP: cli_cli__enable_voice_mode @ cli.py:_enable_voice_mode */
/* PoP: cli_cli__disable_voice_mode @ cli.py:_disable_voice_mode */
/* PoP: cli_cli__toggle_voice_tts @ cli.py:_toggle_voice_tts */
/* PoP: cli_cli__show_voice_status @ cli.py:_show_voice_status */
/* PoP: cli_cli__persist_prompt_summary @ cli.py:_persist_prompt_summary */
/* PoP: cli_cli__clarify_callback @ cli.py:_clarify_callback */
/* PoP: cli_cli__sudo_password_callback @ cli.py:_sudo_password_callback */
/* PoP: cli_cli__approval_callback @ cli.py:_approval_callback */
/* PoP: cli_cli__approval_choices @ cli.py:_approval_choices */
/* PoP: cli_cli__computer_use_approval_callback @ cli.py:_computer_use_approval_callback */
/* PoP: cli_cli__handle_approval_selection @ cli.py:_handle_approval_selection */
/* PoP: cli_cli__get_approval_display_fragments @ cli.py:_get_approval_display_fragments */
/* PoP: cli_cli__secret_capture_callback @ cli.py:_secret_capture_callback */
/* PoP: cli_cli__capture_modal_input_snapshot @ cli.py:_capture_modal_input_snapshot */
/* PoP: cli_cli__restore_modal_input_snapshot @ cli.py:_restore_modal_input_snapshot */
/* PoP: cli_cli__submit_secret_response @ cli.py:_submit_secret_response */
/* PoP: cli_cli__cancel_secret_capture @ cli.py:_cancel_secret_capture */
/* PoP: cli_cli__clear_secret_input_buffer @ cli.py:_clear_secret_input_buffer */
/* PoP: cli_cli__clear_terminal_on_exit @ cli.py:_clear_terminal_on_exit */
/* PoP: cli_cli__print_exit_summary @ cli.py:_print_exit_summary */
/* PoP: cli_cli__get_tui_prompt_symbols @ cli.py:_get_tui_prompt_symbols */
/* PoP: cli_cli__audio_level_bar @ cli.py:_audio_level_bar */
/* PoP: cli_cli__get_tui_prompt_fragments @ cli.py:_get_tui_prompt_fragments */
/* PoP: cli_cli__get_tui_prompt_text @ cli.py:_get_tui_prompt_text */
/* PoP: cli_cli__build_tui_style_dict @ cli.py:_build_tui_style_dict */
/* PoP: cli_cli__apply_tui_skin_style @ cli.py:_apply_tui_skin_style */
/* PoP: cli_cli__get_extra_tui_widgets @ cli.py:_get_extra_tui_widgets */
/* PoP: cli_cli__register_extra_tui_keybindings @ cli.py:_register_extra_tui_keybindings */
/* PoP: cli_cli__build_tui_layout_children @ cli.py:_build_tui_layout_children */
/* PoP: cli_cli_run @ cli.py:run */
/* PoP: cli_cli__run_kanban_goal_loop_q @ cli.py:_run_kanban_goal_loop_q */

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