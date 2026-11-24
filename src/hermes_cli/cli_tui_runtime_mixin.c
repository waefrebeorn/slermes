/*
 * cli_tui_runtime_mixin.c - C11 port of upstream
 * hermes_cli/cli_tui_runtime_mixin.py (gate w32_clituirunt_1dc8f7).
 *
 * PoP: w32_clituirunt_1dc8f7_closing_session @ hermes_cli/cli_tui_runtime_mixin.py:CLITuiRuntimeMixin._closing_session_id
 * PoP: w32_clituirunt_1dc8f7_hold_step @ hermes_cli/cli_tui_runtime_mixin.py:CLITuiRuntimeMixin._tui_hold_step
 * PoP: w32_clituirunt_1dc8f7_build_application @ hermes_cli/cli_tui_runtime_mixin.py:CLITuiRuntimeMixin._tui_build_application
 * PoP: w32_clituirunt_1dc8f7_signal_plan @ hermes_cli/cli_tui_runtime_mixin.py:CLITuiRuntimeMixin._tui_install_signal_handlers
 * PoP: w32_clituirunt_1dc8f7_slash_input @ hermes_cli/cli_tui_runtime_mixin.py:CLITuiRuntimeMixin._tui_run_slash_input
 * PoP: w32_clituirunt_1dc8f7_after_turn @ hermes_cli/cli_tui_runtime_mixin.py:CLITuiRuntimeMixin._tui_after_turn
 * PoP: w32_clituirunt_1dc8f7_shutdown_plan @ hermes_cli/cli_tui_runtime_mixin.py:CLITuiRuntimeMixin._tui_shutdown
 *
 * The REPL loop itself, the prompt_toolkit Application, the signal module, the
 * console printers, the gateway ownership probe, the exit-choice prompt, the
 * voice thread and the session DB are seams: none of them are re-implemented
 * here. What is ported is the part that is a pure function of values -- which
 * session a quit closes, what one held input step returns, which kwargs the
 * Application is built with, which signals get a handler, what seed a slash
 * command hands back, what state a finished turn leaves behind, and which side
 * effects a shutdown would run and in what order.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w32_clituirunt_1dc8f7.h"

/* Python truthiness of a JSON value. */
static int tu_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* A missing key reads as None. */
static json_t *tu_copy(const json_t *v) {
    return v ? json_copy(v) : json_null();
}

/* float(value): Python's numeric view, plus the string form float() accepts. */
static double tu_float(const json_t *v) {
    if (!v) return 0.0;
    switch (v->type) {
    case JSON_NUMBER: return v->num_val;
    case JSON_BOOL:   return v->bool_val ? 1.0 : 0.0;
    case JSON_STRING: return v->str_val ? strtod(v->str_val, NULL) : 0.0;
    default:          return 0.0;
    }
}

static int tu_truthy_key(const json_t *obj, const char *key) {
    return tu_truthy(json_obj_get(obj, key));
}

/* A session id that is present and not None. */
static const json_t *tu_present(const json_t *v) {
    return (v && v->type != JSON_NULL) ? v : NULL;
}

static void tu_call(json_t *calls, const char *name) {
    json_append(calls, json_string(name));
}

/* ------------------------------------------------------------ closing_session */

json_t *w32_clituirunt_1dc8f7_closing_session(const json_t *req) {
    const json_t *agent = json_obj_get(req, "agent");

    /* if self.agent: return self.agent.session_id */
    if (agent && agent->type == JSON_OBJECT)
        return tu_copy(json_obj_get(agent, "session_id"));
    /* return self.session_id if getattr(self, "_gateway_session_open", False) else None */
    if (!tu_truthy_key(req, "gateway_session_open"))
        return json_null();
    return tu_copy(json_obj_get(req, "session_id"));
}

/* ------------------------------------------------------------------ hold_step */

json_t *w32_clituirunt_1dc8f7_hold_step(const json_t *req) {
    const json_t *turn = json_obj_get(req, "turn");
    const json_t *item = json_obj_get(req, "item");
    int done = tu_truthy_key(req, "done");
    int has_turn = turn && turn->type != JSON_NULL;
    json_t *out = json_object();
    json_t *calls = json_array();

    /* self._drain_process_notifications("cli-idle") -- suppressed on error, so a
     * raising drain still reports the call and leaves the decision untouched. */
    tu_call(calls, "drain:cli-idle");
    tu_call(calls, "take_gateway_turn");

    if (has_turn) {
        /* item = hold.take_gateway_turn(...); if item is not None: return hold, item */
        json_set(out, "hold", json_bool(1));
        json_set(out, "run", tu_copy(turn));
        json_set(out, "slept", json_number(0));
    } else {
        tu_call(calls, "done");
        if (done) {
            /* return None, hold.item */
            json_set(out, "hold", json_bool(0));
            json_set(out, "run", tu_copy(item));
            json_set(out, "slept", json_number(0));
        } else {
            /* time.sleep(0.05); return hold, None */
            json_set(out, "hold", json_bool(1));
            json_set(out, "run", json_null());
            json_set(out, "slept", json_number(1));
        }
    }
    json_set(out, "calls", calls);
    return out;
}

/* --------------------------------------------------------- build_application */

json_t *w32_clituirunt_1dc8f7_build_application(const json_t *req) {
    int kitty = tu_truthy_key(req, "kitty");
    int cpr_output = tu_truthy_key(req, "cpr_output");
    int steady_cursor = tu_truthy_key(req, "steady_cursor");
    int vim_mode = tu_truthy_key(req, "vim_mode");
    /* float(CLI_CONFIG.get("display", {}).get("cli_refresh_interval", 0)) */
    double interval = json_has(req, "refresh_interval")
                          ? tu_float(json_obj_get(req, "refresh_interval"))
                          : 0.0;
    json_t *out = json_object();
    json_t *keys = json_array();

    json_append(keys, json_string("layout"));
    json_append(keys, json_string("key_bindings"));
    json_append(keys, json_string("style"));
    json_append(keys, json_string("full_screen"));
    json_append(keys, json_string("mouse_support"));
    json_append(keys, json_string("refresh_interval"));
    json_append(keys, json_string("erase_when_done"));
    /* extra_kw, in the order the body inserts it. */
    if (kitty) json_append(keys, json_string("color_depth"));
    if (cpr_output) json_append(keys, json_string("output"));
    if (steady_cursor) json_append(keys, json_string("cursor"));
    json_append(keys, json_string("editing_mode"));

    json_set(out, "keys", keys);
    json_set(out, "color_depth", kitty ? json_string("DEPTH_24_BIT") : json_null());
    json_set(out, "has_output", json_bool(cpr_output));
    json_set(out, "has_cursor", json_bool(steady_cursor));
    json_set(out, "editing_mode", json_string(vim_mode ? "VI" : "EMACS"));
    json_set(out, "refresh_interval", json_number(interval));
    json_set(out, "full_screen", json_bool(0));
    json_set(out, "mouse_support", json_bool(0));
    json_set(out, "erase_when_done", json_bool(1));
    json_set(out, "layout_arg", json_bool(1));
    json_set(out, "kb_arg", json_bool(1));
    json_set(out, "style_arg", json_bool(1));
    return out;
}

/* --------------------------------------------------------------- signal_plan */

json_t *w32_clituirunt_1dc8f7_signal_plan(const json_t *req) {
    const char *platform = json_get_str(req, "platform", "linux");
    int win32 = platform && strcmp(platform, "win32") == 0;
    int has_sighup = tu_truthy_key(req, "has_sighup");
    json_t *out = json_object();
    json_t *installed = json_array();
    int count = 0;

    /* _signal.signal(SIGTERM, handler); if hasattr(_signal, "SIGHUP"): SIGHUP. */
    json_append(installed, json_string("SIGTERM"));
    count++;
    if (has_sighup) {
        json_append(installed, json_string("SIGHUP"));
        count++;
    }
    /* Windows: absorb SIGINT with a lambda; real Ctrl+C is prompt_toolkit's. */
    if (win32) {
        json_append(installed, json_string("SIGINT"));
        count++;
    }
    json_set(out, "installed", installed);
    json_set(out, "sigint_absorbed", json_bool(win32));
    json_set(out, "sigterm", json_number(15));
    json_set(out, "count", json_number(count));
    return out;
}

/* --------------------------------------------------------------- slash_input */

json_t *w32_clituirunt_1dc8f7_slash_input(const json_t *req) {
    const json_t *ok = json_obj_get(req, "command_ok");
    const json_t *pending = json_obj_get(req, "pending_seed");
    int interrupted = ok && ok->type == JSON_STRING && ok->str_val &&
                      strcmp(ok->str_val, "kbi") == 0;
    int should_exit = tu_truthy_key(req, "should_exit");
    int app_exit = 0;
    json_t *out = json_object();

    if (interrupted) {
        /* Ctrl+C during a slow command: return None, the pending seed is untouched. */
        json_set(out, "seed", json_null());
        json_set(out, "should_exit", json_bool(should_exit));
        json_set(out, "app_exit", json_bool(0));
        json_set(out, "pending_after", tu_copy(pending));
        return out;
    }
    if (!tu_truthy(ok)) {
        /* if not self.process_command(...): self._should_exit = True; app.exit() if running. */
        should_exit = 1;
        app_exit = tu_truthy_key(req, "app_running");
    }
    /* _seed, self._pending_agent_seed = self._pending_agent_seed, None; return _seed or None */
    json_set(out, "seed", tu_truthy(pending) ? tu_copy(pending) : json_null());
    json_set(out, "should_exit", json_bool(should_exit));
    json_set(out, "app_exit", json_bool(app_exit));
    json_set(out, "pending_after", json_null());
    return out;
}

/* ---------------------------------------------------------------- after_turn */

json_t *w32_clituirunt_1dc8f7_after_turn(const json_t *req) {
    int interrupted = tu_truthy_key(req, "last_turn_interrupted");
    int voice_mode = tu_truthy_key(req, "voice_mode");
    int voice_continuous = tu_truthy_key(req, "voice_continuous");
    int voice_recording = tu_truthy_key(req, "voice_recording");
    int voice_tts = tu_truthy_key(req, "voice_tts");
    int barge = tu_truthy_key(req, "barge_capture");
    json_t *out = json_object();
    json_t *state = json_object();
    json_t *calls = json_array();
    int hook_errors = (tu_truthy_key(req, "goal_raises") ? 1 : 0) +
                      (tu_truthy_key(req, "loop_raises") ? 1 : 0);

    /* The bookkeeping resets happen before any hook fires. */
    json_set(state, "agent_running", json_bool(0));
    json_set(state, "pet_reasoning", json_bool(0));
    json_set(state, "spinner_text", json_string(""));
    json_set(state, "last_scrollback_tool", json_string(""));
    json_set(state, "tool_start_time", json_number(0.0));
    json_set(state, "pending_tool_info", json_number(0));
    json_set(state, "labels_present", json_bool(0));
    json_set(state, "interactive_turn", json_bool(0));

    json_append(calls, json_string("pet_react_turn_end"));
    json_append(calls, json_string("turn_summary_emit"));
    json_append(calls, json_string("invalidate"));
    if (interrupted) json_append(calls, json_string("recover_after_interrupt"));
    json_append(calls, json_string("drain_interrupt_queue"));
    /* Both hooks run, each under its own try/except. */
    json_append(calls, json_string("goal_hook"));
    json_append(calls, json_string("loop_hook"));
    /* Continuous voice: restart recording off-thread. */
    if (voice_mode && voice_continuous && !voice_recording) {
        json_append(calls, json_string("thread_start:None"));
        if (voice_tts) {
            json_append(calls, json_string("tts_wait:60"));
            json_append(calls, json_string("sleep"));
        }
        /* A barge-in capture already owns the mic and submits the interruption. */
        if (!barge) {
            json_append(calls, json_string("voice_start_recording"));
            json_append(calls, json_string("invalidate"));
        }
    }
    json_append(calls, json_string("drain:cli-post-turn"));

    json_set(out, "state", state);
    json_set(out, "calls", calls);
    json_set(out, "hook_errors", json_number(hook_errors));
    return out;
}

/* ------------------------------------------------------------- shutdown_plan */

json_t *w32_clituirunt_1dc8f7_shutdown_plan(const json_t *req) {
    const json_t *agent = json_obj_get(req, "agent");
    const json_t *agent_sid = (agent && agent->type == JSON_OBJECT)
                                  ? json_obj_get(agent, "session_id")
                                  : NULL;
    int has_agent = agent && agent->type != JSON_NULL;
    int agent_running = tu_truthy_key(req, "agent_running");
    int detach_on_exit = tu_truthy_key(req, "detach_on_exit");
    int gateway_owns = tu_truthy_key(req, "gateway_owns");
    int viewer_disconnected = tu_truthy_key(req, "viewer_disconnected");
    int session_db = tu_truthy_key(req, "session_db");
    int delete_on_exit = tu_truthy_key(req, "delete_on_exit");
    const json_t *closing;
    json_t *out = json_object();
    json_t *calls = json_array();
    int cprint = 0, unsets = 0;

    json_append(calls, json_string("pet_stop_anim"));
    /* if self.agent and self._agent_running and not (detach and gateway owns) */
    if (has_agent && agent_running && !(detach_on_exit && gateway_owns))
        json_append(calls, json_string("hard_interrupt"));
    if (tu_truthy_key(req, "voice_recorder")) json_append(calls, json_string("voice_shutdown"));
    json_append(calls, json_string("cleanup_recordings"));
    /* set_sudo_password / approval / secret_capture / unlock / save_login / code */
    while (unsets < 6) {
        json_append(calls, json_string("unset"));
        unsets++;
    }
    json_append(calls, json_string("vault_lock"));
    json_append(calls, json_string("persist_session"));

    /* _closing_sid = self._closing_session_id(); a viewer disconnect is not a close. */
    if (has_agent)
        closing = agent_sid;
    else if (tu_truthy_key(req, "gateway_session_open"))
        closing = json_obj_get(req, "session_id");
    else
        closing = NULL;
    if (viewer_disconnected) closing = NULL;
    closing = tu_present(closing);

    if (session_db && closing) {
        json_append(calls, json_string("end_session"));
        if (!delete_on_exit) {
            /* Drop the empty row of a start-and-quit session. */
            json_append(calls, json_string("discard"));
        } else {
            /* /exit --delete: remove transcripts + SQLite history; either way a line prints. */
            json_append(calls, json_string("delete_session"));
            /* either delete outcome prints one dim line */
            json_append(calls, json_string("cprint"));
            cprint++;
        }
    }
    if (has_agent && agent_running && !gateway_owns)
        json_append(calls, json_string("interrupted_end"));
    /* A signal-driven exit never asks; an armed watchdog means it already did. */
    if (!tu_truthy_key(req, "watchdog_armed") && tu_truthy_key(req, "active_worktree")) {
        json_append(calls, json_string("ask_exit_choice"));
        if (!tu_truthy_key(req, "ask_raises"))
            json_append(calls, json_string("apply_exit_choice"));
    }
    json_append(calls, json_string("run_cleanup"));
    json_append(calls, json_string("exit_summary"));
    json_append(calls, json_string("release_session"));

    json_set(out, "calls", calls);
    json_set(out, "hard_interrupt",
             json_bool(has_agent && agent_running && !(detach_on_exit && gateway_owns)));
    json_set(out, "voice_shutdown", json_bool(tu_truthy_key(req, "voice_recorder")));
    json_set(out, "should_exit", json_bool(1));
    json_set(out, "closing_sid", (session_db && closing) ? tu_copy(closing) : json_null());
    json_set(out, "end_reason", (session_db && closing) ? json_string("cli_close") : json_null());
    json_set(out, "delete_called", json_bool(session_db && delete_on_exit && closing != NULL));
    json_set(out, "discard", (session_db && !delete_on_exit && closing) ? tu_copy(closing)
                                                                        : json_null());
    json_set(out, "interrupted_end",
             (has_agent && agent_running && !gateway_owns) ? json_string("shutdown")
                                                           : json_null());
    json_set(out, "cprint", json_number(cprint));
    json_set(out, "unset_count", json_number(unsets));
    json_set(out, "cleanup", json_bool(1));
    json_set(out, "exit_summary", json_bool(1));
    json_set(out, "release", json_bool(1));
    json_set(out, "vault_lock", json_bool(1));
    return out;
}
