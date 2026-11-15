/* cli_gaps2_2026_08_26.h — REAL_GAP closure batch 2 for Python cli.py
 * (HermesCLI class methods, ported 2026-08-26). Streaming emitter, external
 * editor, tool-callback installers, security startup notices, banner
 * orchestration, resume restore trio, OSC52 clipboard, termios watchdogs,
 * vision pre-processing and tool availability warnings.
 *
 * Convention: C names match the Python method names 1:1 and every function
 * takes the opaque `hcli_t *self` handle as its first argument (mirroring
 * `self` on class HermesCLI). Subsystems without a linked C port are modeled
 * as nullable function-pointer seams on hcli_t — NULL reproduces the Python
 * `except ImportError: pass` branch (a real behavior, not a stub).
 */
#ifndef SLERMES_CLI_GAPS2_2026_08_26_H
#define SLERMES_CLI_GAPS2_2026_08_26_H

#include <stdbool.h>
#include <stddef.h>
#include "cli_hermes_cli.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Callback shape shared by the reasoning display callbacks. */
typedef void (*hcli_reasoning_delta_cb)(hcli_t *self, const char *text);

/* PoP: on_notice_clear @ cli.py:HermesCLI.on_notice_clear */
void on_notice_clear(hcli_t *self, const char *key);

/* PoP: current_reasoning_callback @ cli.py:HermesCLI.current_reasoning_callback
 * Returns the active reasoning display callback for the current mode
 * (NULL = none). */
hcli_reasoning_delta_cb current_reasoning_callback(hcli_t *self);

/* PoP: format_submitted_user_message_preview @ cli.py:HermesCLI.format_submitted_user_message_preview
 * Malloc'd rich-markup preview string; caller frees. */
char *format_submitted_user_message_preview(hcli_t *self,
                                             const char *user_input);

/* PoP: expand_paste_references @ cli.py:HermesCLI.expand_paste_references
 * Expand "[Pasted text #N: M lines → path]" placeholders into file contents.
 * Malloc'd text; caller frees. */
char *expand_paste_references(const char *text);

/* PoP: _close_reasoning_box @ cli.py:HermesCLI._close_reasoning_box */
void _close_reasoning_box(hcli_t *self);

/* PoP: _emit_stream_text @ cli.py:HermesCLI._emit_stream_text */
void _emit_stream_text(hcli_t *self, const char *text);

/* PoP: _flush_stream @ cli.py:HermesCLI._flush_stream */
void _flush_stream(hcli_t *self);

/* PoP: reset_stream_state @ cli.py:HermesCLI.reset_stream_state */
void reset_stream_state(hcli_t *self);

/* PoP: slow_command_status @ cli.py:HermesCLI.slow_command_status
 * Static lifetime message — do not free. */
const char *slow_command_status(const char *command);

/* PoP: command_spinner_frame @ cli.py:HermesCLI.command_spinner_frame
 * Static braille frame — do not free. */
const char *command_spinner_frame(void);

/* PoP: _open_external_editor @ cli.py:HermesCLI._open_external_editor */
bool _open_external_editor(hcli_t *self);

/* PoP: install_tool_callbacks @ cli.py:HermesCLI.install_tool_callbacks */
void install_tool_callbacks(hcli_t *self);

/* PoP: ensure_tirith_security @ cli.py:HermesCLI.ensure_tirith_security */
void ensure_tirith_security(hcli_t *self);

/* PoP: show_security_advisories @ cli.py:HermesCLI.show_security_advisories */
void show_security_advisories(void);

/* PoP: _show_browser_backend_notice @ cli.py:HermesCLI._show_browser_backend_notice */
void _show_browser_backend_notice(hcli_t *self);

/* PoP: finalize_preloaded_skills @ cli.py:HermesCLI.finalize_preloaded_skills */
void finalize_preloaded_skills(hcli_t *self);

/* PoP: show_banner @ cli.py:HermesCLI.show_banner */
void show_banner(hcli_t *self);

/* PoP: _restore_session_cwd @ cli.py:HermesCLI._restore_session_cwd
 * session_meta_json is the session row dict (JSON-encoded). */
void _restore_session_cwd(hcli_t *self, const char *session_meta_json,
                          bool quiet);

/* PoP: _restore_session_yolo @ cli.py:HermesCLI._restore_session_yolo */
void _restore_session_yolo(hcli_t *self, const char *session_meta_json,
                           bool quiet);

/* PoP: persist_model_switch_to_session @ cli.py:HermesCLI.persist_model_switch_to_session
 * result_json: {target_provider, base_url, api_mode, new_model}. */
void persist_model_switch_to_session(hcli_t *self, const char *result_json);

/* PoP: _restore_session_model @ cli.py:HermesCLI._restore_session_model */
void _restore_session_model(hcli_t *self, const char *session_meta_json,
                            bool quiet);

/* PoP: render_resume_history_panel_lines @ cli.py:HermesCLI.render_resume_history_panel_lines
 * Render the resume panel at the current terminal width. Returns a malloc'd
 * array of malloc'd lines (caller frees via gaps2_strings_free); count in
 * *out_len. */
char **render_resume_history_panel_lines(hcli_t *self, void *panel,
                                          size_t *out_len);
void gaps2_strings_free(char **arr, size_t len);

/* PoP: try_attach_clipboard_image @ cli.py:HermesCLI.try_attach_clipboard_image */
bool try_attach_clipboard_image(hcli_t *self);

/* PoP: resolve_checkpoint_ref @ cli.py:HermesCLI.resolve_checkpoint_ref
 * checkpoints_json: JSON array of {hash,...} rows. Malloc'd full hash or
 * NULL (message printed for an out-of-range number). */
char *resolve_checkpoint_ref(const char *ref, const char *checkpoints_json);

/* PoP: write_osc52_clipboard @ cli.py:HermesCLI.write_osc52_clipboard */
void write_osc52_clipboard(hcli_t *self, const char *text);

/* PoP: recover_terminal_input_modes @ cli.py:HermesCLI.recover_terminal_input_modes */
void recover_terminal_input_modes(hcli_t *self, const char *reason);

/* PoP: _check_termios_drift @ cli.py:HermesCLI._check_termios_drift */
void _check_termios_drift(hcli_t *self);

/* PoP: preprocess_images_with_vision @ cli.py:HermesCLI.preprocess_images_with_vision
 * Analyze attached images through the vision seam and prepend descriptions
 * to the user's text. Malloc'd enriched text; caller frees. */
char *preprocess_images_with_vision(hcli_t *self, const char *text,
                                     char **images, size_t images_len,
                                     bool announce);

/* PoP: _show_tool_availability_warnings @ cli.py:HermesCLI._show_tool_availability_warnings */
void _show_tool_availability_warnings(hcli_t *self);

#ifdef __cplusplus
}
#endif
#endif /* SLERMES_CLI_GAPS2_2026_08_26_H */
