/*
 * cli_hermes_cli_skeleton.h — C signatures for the remaining HermesCLI class
 * methods (cli.py). Generated skeleton: each wave agent implements its batch
 * against these declarations so all batches share one consistent API.
 * Return types are heuristic; refine per-function as you implement.
 */
#ifndef CLI_HERMES_CLI_SKELETON_H
#define CLI_HERMES_CLI_SKELETON_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

/* opaque HermesCLI state (206 distinct self.* fields in Python) */
typedef struct hcli hcli_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: check_config_mcp_changes @ cli.py:_check_config_mcp_changes */
json_t * hcli_check_config_mcp_changes(hcli_t *self);

/* PoP: split_destructive_skip @ cli.py:_split_destructive_skip */
char * hcli_split_destructive_skip(hcli_t *self);

/* PoP: on_tool_start @ cli.py:_on_tool_start */
json_t * hcli_on_tool_start(hcli_t *self);

/* PoP: on_tool_complete @ cli.py:_on_tool_complete */
json_t * hcli_on_tool_complete(hcli_t *self);

/* PoP: voice_stt_model @ cli.py:_voice_stt_model */
json_t * hcli_voice_stt_model(hcli_t *self);

/* PoP: voice_stt_provider @ cli.py:_voice_stt_provider */
char * hcli_voice_stt_provider(hcli_t *self);

/* PoP: voice_speak_response_async @ cli.py:_voice_speak_response_async */
json_t * hcli_voice_speak_response_async(hcli_t *self);

/* PoP: voice_speak_response @ cli.py:_voice_speak_response */
json_t * hcli_voice_speak_response(hcli_t *self);

/* PoP: voice_beeps_enabled @ cli.py:_voice_beeps_enabled */
bool hcli_voice_beeps_enabled(hcli_t *self);

/* PoP: disable_voice_mode @ cli.py:_disable_voice_mode */
json_t * hcli_disable_voice_mode(hcli_t *self);

/* PoP: stop_wake_word_listener @ cli.py:_stop_wake_word_listener */
json_t * hcli_stop_wake_word_listener(hcli_t *self);

/* PoP: start_wake_watchdog @ cli.py:_start_wake_watchdog */
json_t * hcli_start_wake_watchdog(hcli_t *self);

/* PoP: show_wake_word_status @ cli.py:_show_wake_word_status */
json_t * hcli_show_wake_word_status(hcli_t *self);

/* PoP: toggle_voice_tts @ cli.py:_toggle_voice_tts */
json_t * hcli_toggle_voice_tts(hcli_t *self);

/* PoP: persist_prompt_summary @ cli.py:_persist_prompt_summary */
json_t * hcli_persist_prompt_summary(hcli_t *self);

/* PoP: clarify_batch_set_active @ cli.py:_clarify_batch_set_active */
json_t * hcli_clarify_batch_set_active(hcli_t *self);

/* PoP: approval_choices @ cli.py:_approval_choices */
json_t * hcli_approval_choices(hcli_t *self);

/* PoP: get_approval_display_fragments @ cli.py:_get_approval_display_fragments */
json_t * hcli_get_approval_display_fragments(hcli_t *self);

/* PoP: secret_capture_callback @ cli.py:_secret_capture_callback */
char * hcli_secret_capture_callback(hcli_t *self);

/* PoP: capture_modal_input_snapshot @ cli.py:_capture_modal_input_snapshot */
json_t * hcli_capture_modal_input_snapshot(hcli_t *self);

/* PoP: restore_modal_input_snapshot @ cli.py:_restore_modal_input_snapshot */
json_t * hcli_restore_modal_input_snapshot(hcli_t *self);

/* PoP: clear_secret_input_buffer @ cli.py:_clear_secret_input_buffer */
json_t * hcli_clear_secret_input_buffer(hcli_t *self);

/* PoP: clear_terminal_on_exit @ cli.py:_clear_terminal_on_exit */
json_t * hcli_clear_terminal_on_exit(hcli_t *self);

/* PoP: get_tui_prompt_symbols @ cli.py:_get_tui_prompt_symbols */
char * hcli_get_tui_prompt_symbols(hcli_t *self);

/* PoP: audio_level_bar @ cli.py:_audio_level_bar */
char * hcli_audio_level_bar(hcli_t *self);

/* PoP: build_tui_style_dict @ cli.py:_build_tui_style_dict */
char * hcli_build_tui_style_dict(hcli_t *self);

/* PoP: get_extra_tui_widgets @ cli.py:_get_extra_tui_widgets */
json_t * hcli_get_extra_tui_widgets(hcli_t *self);

/* PoP: register_extra_tui_keybindings @ cli.py:_register_extra_tui_keybindings */
json_t * hcli_register_extra_tui_keybindings(hcli_t *self);

/* PoP: claim_active_session @ cli.py:_claim_active_session */
bool hcli_claim_active_session(hcli_t *self);

/* PoP: mark_terminal_io_broken @ cli.py:_mark_terminal_io_broken */
json_t * hcli_mark_terminal_io_broken(hcli_t *self);

/* PoP: invalidate @ cli.py:_invalidate */
json_t * hcli_invalidate(hcli_t *self);

/* PoP: paint_now @ cli.py:_paint_now */
json_t * hcli_paint_now(hcli_t *self);

/* PoP: schedule_focus_regain_redraw @ cli.py:_schedule_focus_regain_redraw */
json_t * hcli_schedule_focus_regain_redraw(hcli_t *self);

/* PoP: clear_prompt_toolkit_screen @ cli.py:_clear_prompt_toolkit_screen */
json_t * hcli_clear_prompt_toolkit_screen(hcli_t *self);

/* PoP: schedule_resize_recovery @ cli.py:_schedule_resize_recovery */
json_t * hcli_schedule_resize_recovery(hcli_t *self);

/* PoP: install_resize_recovery @ cli.py:_install_resize_recovery */
json_t * hcli_install_resize_recovery(hcli_t *self);

/* PoP: use_minimal_tui_chrome @ cli.py:_use_minimal_tui_chrome */
bool hcli_use_minimal_tui_chrome(hcli_t *self);

/* PoP: tui_input_rule_height @ cli.py:_tui_input_rule_height */
json_t * hcli_tui_input_rule_height(hcli_t *self);

/* PoP: render_spinner_text @ cli.py:_render_spinner_text */
char * hcli_render_spinner_text(hcli_t *self);

/* PoP: on_reaction @ cli.py:_on_reaction */
json_t * hcli_on_reaction(hcli_t *self);

/* PoP: normalize_model_for_provider @ cli.py:_normalize_model_for_provider */
bool hcli_normalize_model_for_provider(hcli_t *self, const char *resolved_provider);

/* PoP: flush_reasoning_preview @ cli.py:_flush_reasoning_preview */
void hcli_flush_reasoning_preview(hcli_t *self, bool force);

/* PoP: print_user_message_preview @ cli.py:_print_user_message_preview */
void hcli_print_user_message_preview(hcli_t *self, const char *user_input);

/* PoP: stream_reasoning_delta @ cli.py:_stream_reasoning_delta */
void hcli_stream_reasoning_delta(hcli_t *self, const char *text);

/* PoP: busy_command @ cli.py:_busy_command
 * Enter the busy state (set flags + print "⏳ <status>"). Python is a
 * generator context manager; pair with hcli_busy_command_end() in the
 * caller's cleanup path (the `finally` branch). */
void hcli_busy_command(hcli_t *self, const char *status, bool blocks_input);
void hcli_busy_command_end(hcli_t *self);

/* PoP: inline_pastes @ cli.py:_inline_pastes
 * buffer_text: in/out pointer to the input-buffer text (freed/replaced when
 * expansion changes it). cursor_position: in/out cursor index, set to the new
 * length on expansion (may be NULL). Sets _skip_paste_collapse on change. */
void hcli_inline_pastes(hcli_t *self, char **buffer_text, size_t *cursor_position);

/* PoP: show_browser_backend_notice @ cli.py:_show_browser_backend_notice */
void hcli_show_browser_backend_notice(hcli_t *self);

/* PoP: restore_session_cwd @ cli.py:_restore_session_cwd
 * session_meta_json: JSON-encoded session row (as returned by SessionDB.get_session). */
void hcli_restore_session_cwd(hcli_t *self, const char *session_meta_json, bool quiet);

/* PoP: restore_session_yolo @ cli.py:_restore_session_yolo
 * Returns the malloc'd restore message (caller frees) or NULL when nothing
 * was restored; printed via the console unless quiet. */
char * hcli_restore_session_yolo(hcli_t *self, const char *session_meta_json, bool quiet);

/* PoP: restore_session_model @ cli.py:_restore_session_model */
void hcli_restore_session_model(hcli_t *self, const char *session_meta_json, bool quiet);

/* PoP: check_termios_drift @ cli.py:_check_termios_drift */
void hcli_check_termios_drift(hcli_t *self);

/* PoP: show_tool_availability_warnings @ cli.py:_show_tool_availability_warnings */
void hcli_show_tool_availability_warnings(hcli_t *self);

/* PoP: show_status @ cli.py:_show_status */
json_t * hcli_show_status(hcli_t *self);

/* PoP: command_available @ cli.py:_command_available */
bool hcli_command_available(hcli_t *self);

/* PoP: show_help @ cli.py:show_help */
bool hcli_show_help(hcli_t *self);

/* PoP: show_recent_sessions @ cli.py:_show_recent_sessions */
bool hcli_show_recent_sessions(hcli_t *self);

/* PoP: show_history @ cli.py:show_history */
char * hcli_show_history(hcli_t *self);

/* PoP: retry_last @ cli.py:retry_last */
json_t * hcli_retry_last(hcli_t *self);

/* PoP: prompt_text_input @ cli.py:_prompt_text_input */
json_t * hcli_prompt_text_input(hcli_t *self);

/* PoP: submit_slash_confirm_response @ cli.py:_submit_slash_confirm_response */
json_t * hcli_submit_slash_confirm_response(hcli_t *self);

/* PoP: build_command_palette_entries @ cli.py:_build_command_palette_entries */
json_t * hcli_build_command_palette_entries(hcli_t *self);

/* PoP: should_handle_steer_command_inline @ cli.py:_should_handle_steer_command_inline */
bool hcli_should_handle_steer_command_inline(hcli_t *self);

/* PoP: should_handle_background_command_inline @ cli.py:_should_handle_background_command_inline */
bool hcli_should_handle_background_command_inline(hcli_t *self);

/* PoP: console_print @ cli.py:_console_print */
json_t * hcli_console_print(hcli_t *self);

/* PoP: handle_bang_shell @ cli.py:handle_bang_shell */
bool hcli_handle_bang_shell(hcli_t *self);

/* PoP: start_heartbeat_watchdog @ cli.py:_start_heartbeat_watchdog */
json_t * hcli_start_heartbeat_watchdog(hcli_t *self);

/* PoP: maybe_fire_loop_tick @ cli.py:_maybe_fire_loop_tick */
json_t * hcli_maybe_fire_loop_tick(hcli_t *self);

/* PoP: maybe_complete_loop_tick_after_turn @ cli.py:_maybe_complete_loop_tick_after_turn */
json_t * hcli_maybe_complete_loop_tick_after_turn(hcli_t *self);

/* PoP: maybe_continue_goal_after_turn @ cli.py:_maybe_continue_goal_after_turn */
char * hcli_maybe_continue_goal_after_turn(hcli_t *self);

/* PoP: toggle_verbose @ cli.py:_toggle_verbose */
json_t * hcli_toggle_verbose(hcli_t *self);

/* PoP: on_reasoning @ cli.py:_on_reasoning */
json_t * hcli_on_reasoning(hcli_t *self);

/* PoP: manual_compress @ cli.py:_manual_compress */
json_t * hcli_manual_compress(hcli_t *self);

/* PoP: voice_restart_recording_async @ cli.py:_voice_restart_recording_async */
json_t * hcli_voice_restart_recording_async(hcli_t *self);

/* PoP: voice_full_duplex_listener @ cli.py:_voice_full_duplex_listener */
bool hcli_voice_full_duplex_listener(hcli_t *self);

/* PoP: enable_voice_mode @ cli.py:_enable_voice_mode */
json_t * hcli_enable_voice_mode(hcli_t *self);

/* PoP: typed_voice_stop @ cli.py:_typed_voice_stop */
bool hcli_typed_voice_stop(hcli_t *self);

/* PoP: maybe_start_wake_word @ cli.py:_maybe_start_wake_word */
json_t * hcli_maybe_start_wake_word(hcli_t *self);

/* PoP: start_wake_word_listener @ cli.py:_start_wake_word_listener */
bool hcli_start_wake_word_listener(hcli_t *self);

/* PoP: show_voice_status @ cli.py:_show_voice_status */
json_t * hcli_show_voice_status(hcli_t *self);

/* PoP: clarify_batch_enter @ cli.py:_clarify_batch_enter */
json_t * hcli_clarify_batch_enter(hcli_t *self);

/* PoP: computer_use_approval_callback @ cli.py:_computer_use_approval_callback */
json_t * hcli_computer_use_approval_callback(hcli_t *self);

/* PoP: handle_approval_selection @ cli.py:_handle_approval_selection */
json_t * hcli_handle_approval_selection(hcli_t *self);

/* PoP: submit_secret_response @ cli.py:_submit_secret_response */
json_t * hcli_submit_secret_response(hcli_t *self);

/* PoP: cancel_secret_capture @ cli.py:_cancel_secret_capture */
json_t * hcli_cancel_secret_capture(hcli_t *self);

/* PoP: print_exit_summary @ cli.py:_print_exit_summary */
json_t * hcli_print_exit_summary(hcli_t *self);

/* PoP: get_tui_prompt_text @ cli.py:_get_tui_prompt_text */
char * hcli_get_tui_prompt_text(hcli_t *self);

/* PoP: build_tui_layout_children @ cli.py:_build_tui_layout_children */
json_t * hcli_build_tui_layout_children(hcli_t *self);

/* PoP: render_stash_panel @ cli.py:_render_stash_panel */
json_t * hcli_render_stash_panel(hcli_t *self);

/* PoP: on_thinking @ cli.py:_on_thinking */
json_t * hcli_on_thinking(hcli_t *self);

/* PoP: close_reasoning_box @ cli.py:_close_reasoning_box */
json_t * hcli_close_reasoning_box(hcli_t *self);

/* PoP: open_external_editor @ cli.py:_open_external_editor */
bool hcli_open_external_editor(hcli_t *self);

/* PoP: show_session_status @ cli.py:_show_session_status */
json_t * hcli_show_session_status(hcli_t *self);

/* PoP: close_command_palette @ cli.py:_close_command_palette */
json_t * hcli_close_command_palette(hcli_t *self);

/* PoP: open_model_picker @ cli.py:_open_model_picker */
json_t * hcli_open_model_picker(hcli_t *self);

/* PoP: confirm_expensive_model_switch @ cli.py:_confirm_expensive_model_switch */
bool hcli_confirm_expensive_model_switch(hcli_t *self);

/* PoP: confirm_and_apply_model_switch_result @ cli.py:_confirm_and_apply_model_switch_result */
json_t * hcli_confirm_and_apply_model_switch_result(hcli_t *self);

/* PoP: close_model_picker @ cli.py:_close_model_picker */
json_t * hcli_close_model_picker(hcli_t *self);

/* PoP: handle_model_switch @ cli.py:_handle_model_switch */
json_t * hcli_handle_model_switch(hcli_t *self);

/* PoP: confirm_and_apply_cli_model_switch @ cli.py:_confirm_and_apply_cli_model_switch */
json_t * hcli_confirm_and_apply_cli_model_switch(hcli_t *self);

/* PoP: handle_usage_command @ cli.py:_handle_usage_command */
json_t * hcli_handle_usage_command(hcli_t *self);

/* PoP: on_tool_gen_start @ cli.py:_on_tool_gen_start */
json_t * hcli_on_tool_gen_start(hcli_t *self);

/* PoP: on_wake_word @ cli.py:_on_wake_word */
char * hcli_on_wake_word(hcli_t *self);

/* PoP: clarify_batch_lock @ cli.py:_clarify_batch_lock */
json_t * hcli_clarify_batch_lock(hcli_t *self);

/* PoP: clarify_callback_batch @ cli.py:_clarify_callback_batch */
json_t * hcli_clarify_callback_batch(hcli_t *self);

/* PoP: apply_tui_skin_style @ cli.py:_apply_tui_skin_style */
bool hcli_apply_tui_skin_style(hcli_t *self);

/* PoP: force_full_redraw @ cli.py:_force_full_redraw */
char * hcli_force_full_redraw(hcli_t *self);

/* PoP: get_voice_status_fragments @ cli.py:_get_voice_status_fragments */
json_t * hcli_get_voice_status_fragments(hcli_t *self);

/* PoP: emit_stream_text @ cli.py:_emit_stream_text */
json_t * hcli_emit_stream_text(hcli_t *self);

/* PoP: flush_stream @ cli.py:_flush_stream */
json_t * hcli_flush_stream(hcli_t *self);

/* PoP: show_banner @ cli.py:show_banner */
json_t * hcli_show_banner(hcli_t *self);

/* PoP: new_session @ cli.py:new_session */
json_t * hcli_new_session(hcli_t *self);

/* PoP: undo_last @ cli.py:undo_last */
json_t * hcli_undo_last(hcli_t *self);

/* PoP: open_command_palette @ cli.py:_open_command_palette */
json_t * hcli_open_command_palette(hcli_t *self);

/* PoP: handle_command_palette_selection @ cli.py:_handle_command_palette_selection */
json_t * hcli_handle_command_palette_selection(hcli_t *self);

/* PoP: handle_model_picker_selection @ cli.py:_handle_model_picker_selection */
json_t * hcli_handle_model_picker_selection(hcli_t *self);

/* PoP: confirm_destructive_slash @ cli.py:_confirm_destructive_slash */
char * hcli_confirm_destructive_slash(hcli_t *self);

/* PoP: voice_start_recording @ cli.py:_voice_start_recording */
char * hcli_voice_start_recording(hcli_t *self);

/* PoP: voice_submit_barge_utterance @ cli.py:_voice_submit_barge_utterance */
json_t * hcli_voice_submit_barge_utterance(hcli_t *self);

/* PoP: clarify_callback @ cli.py:_clarify_callback */
json_t * hcli_clarify_callback(hcli_t *self);

/* PoP: sudo_password_callback @ cli.py:_sudo_password_callback */
char * hcli_sudo_password_callback(hcli_t *self);

/* PoP: approval_callback @ cli.py:_approval_callback */
char * hcli_approval_callback(hcli_t *self);

/* PoP: recover_after_resize @ cli.py:_recover_after_resize */
json_t * hcli_recover_after_resize(hcli_t *self);

/* PoP: get_status_bar_snapshot @ cli.py:_get_status_bar_snapshot */
json_t * hcli_get_status_bar_snapshot(hcli_t *self);

/* PoP: spinner_widget_height @ cli.py:_spinner_widget_height */
char * hcli_spinner_widget_height(hcli_t *self);

/* PoP: stream_delta @ cli.py:_stream_delta */
json_t * hcli_stream_delta(hcli_t *self);

/* PoP: prompt_text_input_modal @ cli.py:_prompt_text_input_modal */
bool hcli_prompt_text_input_modal(hcli_t *self);

/* PoP: on_tool_progress @ cli.py:_on_tool_progress */
json_t * hcli_on_tool_progress(hcli_t *self);

/* PoP: build_status_bar_text @ cli.py:_build_status_bar_text */
char * hcli_build_status_bar_text(hcli_t *self);

/* PoP: confirm_and_reload_mcp @ cli.py:_confirm_and_reload_mcp */
json_t * hcli_confirm_and_reload_mcp(hcli_t *self);

/* PoP: voice_stop_and_transcribe @ cli.py:_voice_stop_and_transcribe */
json_t * hcli_voice_stop_and_transcribe(hcli_t *self);

/* PoP: get_tui_prompt_fragments @ cli.py:_get_tui_prompt_fragments */
char * hcli_get_tui_prompt_fragments(hcli_t *self);

/* PoP: get_status_bar_fragments @ cli.py:_get_status_bar_fragments */
json_t * hcli_get_status_bar_fragments(hcli_t *self);

/* PoP: process_command @ cli.py:process_command */
bool hcli_process_command(hcli_t *self);

#ifdef __cplusplus
}
#endif

#endif /* CLI_HERMES_CLI_SKELETON_H */