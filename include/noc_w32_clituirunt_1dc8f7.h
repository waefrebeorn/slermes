/*
 * noc_w32_clituirunt_1dc8f7.h - C surface for upstream
 * hermes_cli/cli_tui_runtime_mixin.py (gate w32_clituirunt_1dc8f7).
 *
 * The pure decision core of the classic-TUI run loop: which session a quit
 * closes, the held-input tri-state, the prompt_toolkit Application kwargs,
 * the signal-installation plan, the pending seed a slash command hands back,
 * the post-turn bookkeeping and the teardown plan.  Every seam -- the
 * application object, the signal module, the console printers, the gateway
 * ownership probe, the exit-choice prompt, the voice thread, the session DB --
 * stays in the caller; this file owns only what the values decide.
 */
#ifndef NOC_W32_CLITUIRUNT_1DC8F7_H
#define NOC_W32_CLITUIRUNT_1DC8F7_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {agent: {session_id} | null, session_id, gateway_session_open} ->
 * the session a quit closes, or null. */
json_t *w32_clituirunt_1dc8f7_closing_session(const json_t *req);

/* req: {turn, done, item} -> {hold, run, slept, calls}: one step of holding a
 * local input behind the viewer's own /queue admission. */
json_t *w32_clituirunt_1dc8f7_hold_step(const json_t *req);

/* req: {kitty, cpr_output, steady_cursor, vim_mode, refresh_interval} ->
 * the prompt_toolkit Application kwargs, as the values that built them. */
json_t *w32_clituirunt_1dc8f7_build_application(const json_t *req);

/* req: {platform, has_sighup} -> {installed, sigint_absorbed, sigterm, count}. */
json_t *w32_clituirunt_1dc8f7_signal_plan(const json_t *req);

/* req: {command_ok, app_running, pending_seed, should_exit} ->
 * {seed, should_exit, app_exit, pending_after}. */
json_t *w32_clituirunt_1dc8f7_slash_input(const json_t *req);

/* req: post-turn flags -> {state, calls, hook_errors}: the attribute resets,
 * the ordered hook/voice-restart calls they trigger. */
json_t *w32_clituirunt_1dc8f7_after_turn(const json_t *req);

/* req: teardown flags -> {calls, hard_interrupt, ...}: the shutdown decision
 * plan, one entry per side effect the body would take. */
json_t *w32_clituirunt_1dc8f7_shutdown_plan(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
