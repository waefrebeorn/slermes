#ifndef TUI_FULLSCREEN_H
#define TUI_FULLSCREEN_H

/*
 * tui_fullscreen.h — Full ncurses TUI for Hermes C (P189-P200).
 * MIT License — WuBu Slermes Project
 *
 * 12 phases:
 *   P189: Layout — split panes (message history | input | tool feed | status bar)
 *   P190: Input — multi-line, emoji picker, slash autocomplete
 *   P191: Message display — role colors, syntax highlight, markdown
 *   P192: Streaming — token streaming, real-time token counter
 *   P193: Tool feed — live tool call status, progress bar, result preview
 *   P194: Status bar — model/provider, tokens, iterations, budget
 *   P195: Session browser — list, search, preview, load/delete/export
 *   P196: Config editor — interactive key browser, set/get/explain
 *   P197: Image viewer — sixel/kitty inline display, zoom/pan
 *   P198: Theme engine — skin files, color schemes, fonts
 *   P199: Gateway — JSON-RPC backend, split TUI and agent processes
 *   P200: Mobile mode — responsive layout, touch input, compact status
 *
 * Port of Python: tui_gateway.server + tui_gateway.ws — main TUI app with
 * layout/themes/streaming/session browser/config editor/image viewer.
 * C implementation is native ncurses (no TypeScript Ink dependency).
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* === Core API === */

/* Initialize and run the full-screen TUI. Returns 0 on success. */
int tui_fullscreen_run(agent_state_t *state);

/* === Display API (called from agent/gateway) === */

/* Print a message to the output pane (role-colored) */
void tui_fullscreen_print(const char *fmt, ...);

/* Print an error message (red) */
void tui_fullscreen_error(const char *fmt, ...);

/* Print a warning message (yellow) */
void tui_fullscreen_warn(const char *fmt, ...);

/* OSC52 — copy text to system clipboard via terminal escape sequence.
 * Writes ESC]52;c;<base64> to stdout if terminal supports it.
 * Falls back to /copy command display if no TUI. */
void tui_fullscreen_clipboard_copy(const char *text);

/* Display help hints — hotkeys and common commands overlay.
 * Shows a list of common hotkeys and commands in the tool feed pane. */
void tui_fullscreen_help_hints(void);

/* Stream a token to the output pane */
void tui_fullscreen_stream_token(const char *token);

/* Signal end of streaming */
void tui_fullscreen_stream_done(void);

/* Update tool feed with current tool call status */
void tui_fullscreen_tool_status(const char *tool_name, const char *status,
                                 int progress, int total);

/* Update status bar with agent runtime info */
void tui_fullscreen_status_update(const char *model, const char *provider,
                                    int iteration, int max_iterations,
                                    int tokens_in, int tokens_out,
                                    double budget_remaining);

/* === Session Browser === */
void tui_fullscreen_session_browse(void);

/* === Config Editor === */
void tui_fullscreen_config_edit(void);

/* === Theme === */
void tui_fullscreen_theme_reload(const char *skin_name);

/* === Session Lifecycle (Port of Python: tui_gateway.server) === */

/* Create a new TUI session with the given session key.
 * Starts deferred agent build in background. Emits session boundary events.
 * Python equivalent: session.create
 */
void tui_fullscreen_session_create(const char *session_key, const char *model);

/* Resume an existing session by key.
 * Python equivalent: session.resume
 */
void tui_fullscreen_session_resume(const char *session_key);

/* Close the current session: finalize, emit session_end event.
 * Python equivalent: session.close + _finalize_session
 */
void tui_fullscreen_session_close(const char *end_reason);

/* Finalize the current session: commit memory, end DB row.
 * Python equivalent: _finalize_session
 */
void tui_fullscreen_session_finalize(void);

/* === Deferred Agent Factory (Port of Python: _make_agent / _start_agent_build) === */

/* Start building the agent in background (called lazily on first prompt).
 * Python equivalent: _start_agent_build
 */
void tui_fullscreen_start_agent_build(void);

/* Wait for agent build to complete (blocking, with timeout).
 * Returns true if agent is ready.
 * Python equivalent: _wait_agent
 */
bool tui_fullscreen_wait_agent(int timeout_ms);

/* === Model Switch (Port of Python: _resolve_model / _persist_model_switch) === */

/* Resolve the model string from config/environment.
 * Python equivalent: _resolve_model
 */
const char *tui_fullscreen_resolve_model(void);

/* Switch the session's model at runtime.
 * Python equivalent: _apply_model_switch
 */
bool tui_fullscreen_switch_model(const char *model_spec, bool persist);

/* === Secret Prompting (Port of Python: _block / _clear_pending) === */

/* Block until the user responds to a secret/env prompt.
 * Returns the user's response string (must be freed by caller).
 * Python equivalent: _block(event, sid, payload, timeout)
 */
char *tui_fullscreen_prompt_secret(const char *prompt_text, int timeout_ms);

/* Clear all pending prompts (e.g. on session close).
 * Python equivalent: _clear_pending(sid)
 */
void tui_fullscreen_clear_prompts(void);

/* === Approval Flow (Port of Python: approval.request / clarify/sudo) === */

/* Request user approval for a potentially dangerous operation.
 * Returns true if approved.
 * Python equivalent: approval.respond
 */
bool tui_fullscreen_request_approval(const char *tool_name,
                                      const char *args_preview);

/* Request clarification from the user.
 * Returns the user's response (must be freed by caller).
 * Python equivalent: clarify prompt
 */
char *tui_fullscreen_request_clarify(const char *question);

/* === Session Boundary Hooks (Port of Python: _notify_session_boundary) === */

/* Callback type for session lifecycle hooks.
 * Python equivalent: hermes_cli.plugins.invoke_hook
 */
typedef void (*tui_session_hook_t)(const char *event_type,
                                    const char *session_id,
                                    void *userdata);

/* Register a session lifecycle hook callback.
 * event_type: "on_session_reset", "on_session_finalize", etc.
 */
bool tui_fullscreen_register_hook(const char *event_type,
                                   tui_session_hook_t cb,
                                   void *userdata);

/* Fire a session boundary event to all registered hooks.
 * Python equivalent: _notify_session_boundary
 */
void tui_fullscreen_notify_boundary(const char *event_type,
                                     const char *session_id);

#ifdef __cplusplus
}
#endif

#endif /* TUI_FULLSCREEN_H */
