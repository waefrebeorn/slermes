/*
 * noc_w23_terminalto_ce074e.h - C surface for upstream
 * tools/terminal_tool_background.py (gate w23_terminalto_ce074e).
 *
 * The ``terminal(background=true)`` launch path: resolve the cwd, spawn
 * through the process registry, stamp gateway routing, decide whether this
 * session can receive an async completion at all, register the completion
 * watcher and assemble the JSON result the model sees.
 *
 * Every seam is a fixture fact: the registry (spawn/adopt/pending_watchers/
 * arm_heartbeat), _resolve_command_cwd, _resolve_notification_flag_conflict,
 * _redact_terminal_error_text, _msys_to_windows_path, _cwd_fallback_note,
 * async_delivery_supported, hosted_scope, is_delegated_child_context and the
 * gateway runner behind _gateway_runner_ref.  What lives here is the ladder
 * those facts decide and the exact texts it emits.
 */
#ifndef NOC_W23_TERMINALTO_CE074E_H
#define NOC_W23_TERMINALTO_CE074E_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _looks_like_homebrew_ci_poller(command) -> true/false. */
json_t *w23_terminalto_ce074e_ci_poller(const char *command);

/* _stamp_gateway_routing: req{platform, chat_id, user_id, user_name,
 * thread_id, message_id, session_id} -> the stamped attributes, or null when
 * the session has no platform (nothing is stamped then). */
json_t *w23_terminalto_ce074e_routing(const json_t *req);

/* _apply_async_support: req{notify_on_complete, watch_patterns,
 * world{async_supported, hosted, watcher_platform, watcher_chat_id, ...}} ->
 * {"notify","watch","result","routing"}. */
json_t *w23_terminalto_ce074e_async_support(const json_t *req);

/* _register_completion_watcher: req{session_key, armed, world{proc_id,
 * watcher_platform, watcher_* , parent_session_id}} ->
 * {"watcher","armed","pending","interval"}. */
json_t *w23_terminalto_ce074e_watcher(const json_t *req);

/* spawn_background_process: req carries the call's keyword arguments plus
 * "world" for the seam facts -> the parsed result object. */
json_t *w23_terminalto_ce074e_spawn(const json_t *req);

/* yield_to_background_handler: non-local backends answer null (no adoptable
 * host process); local ones answer what the handler hands back plus the
 * routing it stamped. */
json_t *w23_terminalto_ce074e_yield(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
