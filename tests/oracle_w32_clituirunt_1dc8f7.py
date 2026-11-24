#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cli_tui_runtime_mixin.py (gate w32_clituirunt_1dc8f7).

The live ``CLITuiRuntimeMixin`` is bound to a ``Fake`` subclass that owns only
the recorder attributes: every ported method runs its real body, while the
seams around it -- the prompt_toolkit Application, the signal module, the
console printers, the gateway ownership probe, the exit-choice prompt, the
voice thread -- are patched recorders fed from the fixture.  What is compared
is exactly what those values decide:

  * ``_closing_session_id``: which session a quit closes;
  * ``_tui_hold_step``: the (hold, input) tri-state of one held input;
  * ``_tui_build_application``: the prompt_toolkit Application kwargs;
  * ``_tui_install_signal_handlers``: which signals get a handler;
  * ``_tui_run_slash_input``: the pending agent seed a slash command returns;
  * ``_tui_after_turn``: the post-turn state resets, hook order and voice restart;
  * ``_tui_shutdown``: the teardown decision plan.
"""
import io
import json
import os
import sys
import tempfile
import types
from contextlib import redirect_stdout
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w32_clituirunt_1dc8f7"

# The quarry resolves its home at import time.  A POSIX-typed HOME (MSYS bash)
# is not one for a native interpreter, so hand it a directory of our own: the
# oracle then imports without reading or writing the real profile.
if not os.environ.get("USERPROFILE"):
    _home = tempfile.mkdtemp(prefix=GATE + "_home_")
    os.environ["USERPROFILE"] = _home
    os.environ["HOME"] = _home

# str(QUARRY) at sys.path[0]: tests/hermes_cli would otherwise shadow the quarry.
if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
# Blocked native extensions live behind hermes_seam_stubs, which sits in tests/.
sys.path.append(str(Path(__file__).resolve().parent))


def _import_live():
    try:
        import hermes_cli.cli_tui_runtime_mixin as mod
    except ImportError:  # a blocked compiled extension; the seam stubs cover it
        from hermes_seam_stubs import install as _install_seams

        _install_seams()
        import hermes_cli.cli_tui_runtime_mixin as mod
    return mod


m = _import_live()
import cli as climod  # noqa: E402
import hermes_cli.cli_shutdown as shutmod  # noqa: E402
import hermes_cli.worktree_exit as wtmod  # noqa: E402
import agent.vault_backends.unlock as vaultmod  # noqa: E402
import tools.voice_mode as voicemod  # noqa: E402

MIXIN = m.CLITuiRuntimeMixin
WANTED = {
    "closing_session", "hold_step", "build_application", "signal_plan",
    "slash_input", "after_turn", "shutdown_plan",
}

_MISSING = object()
_DELETE = object()


def _setattr(owner, name, value):
    if value is _DELETE:
        try:
            delattr(owner, name)
        except AttributeError:
            pass
    else:
        setattr(owner, name, value)


class Patched:
    """Apply (owner, attr, value) for the duration of a call, then restore."""

    def __init__(self, pairs):
        self.pairs = pairs

    def __enter__(self):
        self.saved = []
        for owner, name, value in self.pairs:
            self.saved.append((owner, name, getattr(owner, name, _MISSING)))
            _setattr(owner, name, value)
        return self

    def __exit__(self, *exc):
        for owner, name, old in reversed(self.saved):
            _setattr(owner, name, _DELETE if old is _MISSING else old)
        return False


class Fake(MIXIN):
    """The mixin with a plain attribute bag: the bodies under test are real."""

    def __init__(self, **kw):
        self.__dict__.update(kw)


def _quiet():
    return redirect_stdout(io.StringIO())


# ---------------------------------------------------------------- closing_session


def closing_session(req):
    agent = req.get("agent")
    self_ = Fake(
        agent=(types.SimpleNamespace(session_id=agent.get("session_id"))
               if isinstance(agent, dict) else None),
        session_id=req.get("session_id"),
        _gateway_session_open=bool(req.get("gateway_session_open")),
    )
    with _quiet():
        return self_._closing_session_id()


# ---------------------------------------------------------------- hold_step


def hold_step(req):
    turn = req.get("turn")
    done = bool(req.get("done"))
    item = req.get("item")
    calls = []

    class Hold:
        def take_gateway_turn(self, pending):
            calls.append("take_gateway_turn")
            return turn

        def done(self):
            calls.append("done")
            return done

    Hold.item = item

    def drain(where):
        calls.append("drain:" + where)
        if req.get("drain_raises"):
            raise RuntimeError("drain failed")

    self_ = Fake(_drain_process_notifications=drain, _pending_input=object())
    slept = []
    fake_time = types.SimpleNamespace(sleep=lambda s: slept.append(s))

    with Patched([(m, "time", fake_time)]), _quiet():
        hold, run = self_._tui_hold_step(Hold())
    return {
        "hold": hold is not None,
        "run": run,
        "slept": len(slept),
        "calls": calls,
    }


# ---------------------------------------------------------------- build_application


def build_application(req):
    rec = {}

    class App:
        def __init__(self, **kw):
            rec.update(kw)
            rec["_order"] = list(kw)

    def pt_out(_stream):
        return object() if req.get("cpr_output") else None

    if "refresh_interval" in req:
        cfg = {"display": {"cli_refresh_interval": req["refresh_interval"]}}
    elif req.get("display_present"):
        cfg = {"display": {}}
    else:
        cfg = {}

    self_ = Fake(_vim_mode=bool(req.get("vim_mode")))
    pairs = [
        (m, "Application", App),
        (climod, "_select_classic_cli_pt_output", pt_out),
        (climod, "CLI_CONFIG", cfg),
        (climod, "_STEADY_CURSOR", object() if req.get("steady_cursor") else None),
        (m.terminal_image, "supports_kitty_placeholders", lambda: bool(req.get("kitty"))),
    ]
    with Patched(pairs), _quiet():
        self_._tui_build_application("layout-obj", "kb-obj", "style-obj")

    depth = rec.get("color_depth")
    mode = rec.get("editing_mode")
    return {
        "keys": rec.get("_order"),
        "color_depth": getattr(depth, "name", None) if depth is not None else None,
        "has_output": rec.get("output") is not None,
        "has_cursor": rec.get("cursor") is not None,
        "editing_mode": getattr(mode, "name", None) if mode is not None else None,
        "refresh_interval": float(rec.get("refresh_interval", -1)),
        "full_screen": rec.get("full_screen"),
        "mouse_support": rec.get("mouse_support"),
        "erase_when_done": rec.get("erase_when_done"),
        "layout_arg": rec.get("layout") == "layout-obj",
        "kb_arg": rec.get("key_bindings") == "kb-obj",
        "style_arg": rec.get("style") == "style-obj",
    }


# ---------------------------------------------------------------- signal_plan

_SIGNAL_NAMES = (("SIGTERM", 15), ("SIGHUP", 1), ("SIGINT", 2), ("SIGBREAK", 21))


def signal_plan(req):
    import signal as sigmod

    platform = req.get("platform") or "linux"
    want_sighup = bool(req.get("has_sighup"))
    have_sighup = hasattr(sigmod, "SIGHUP")
    installed = []

    def fake_signal(signum, handler):
        name = next((n for n, v in _SIGNAL_NAMES if v == signum), str(signum))
        kind = "lambda" if getattr(handler, "__name__", "") == "<lambda>" else "fn"
        installed.append({"signal": name, "value": signum, "handler": kind})

    pairs = [(sigmod, "signal", fake_signal), (sys, "platform", platform)]
    if want_sighup and not have_sighup:
        pairs.append((sigmod, "SIGHUP", 1))
    elif not want_sighup and have_sighup:
        pairs.append((sigmod, "SIGHUP", _DELETE))

    with Patched(pairs), _quiet():
        Fake(_tui_signal_handler=object())._tui_install_signal_handlers()

    return {
        "installed": [i["signal"] for i in installed],
        "sigint_absorbed": any(i["signal"] == "SIGINT" and i["handler"] == "lambda"
                               for i in installed),
        "sigterm": next((i["value"] for i in installed if i["signal"] == "SIGTERM"), None),
        "count": len(installed),
    }


# ---------------------------------------------------------------- slash_input


def slash_input(req):
    ok = req.get("command_ok")
    calls = []
    exits = []

    def process_command(cmd):
        calls.append(cmd)
        if ok == "kbi":
            raise KeyboardInterrupt
        return bool(ok)

    app = types.SimpleNamespace(is_running=bool(req.get("app_running")),
                                exit=lambda: exits.append(1))
    self_ = Fake(
        process_command=process_command,
        _pending_agent_seed=req.get("pending_seed"),
        _should_exit=bool(req.get("should_exit")),
        _app=app,
    )
    with Patched([(climod, "_cprint", lambda *a, **k: calls.append("cprint"))]), _quiet():
        seed = self_._tui_run_slash_input(req.get("input") or "")
    return {
        "seed": seed,
        "should_exit": bool(self_._should_exit),
        "app_exit": bool(exits),
        "pending_after": self_._pending_agent_seed,
    }


# ---------------------------------------------------------------- after_turn


def after_turn(req):
    calls = []
    hook_err = []

    def goal_hook():
        calls.append("goal_hook")
        if req.get("goal_raises"):
            raise RuntimeError("goal hook")

    def loop_hook():
        calls.append("loop_hook")
        if req.get("loop_raises"):
            raise RuntimeError("loop hook")

    class FakeThread:
        def __init__(self, target=None, name=None, daemon=None):
            self.target = target
            self.name = name
            self.daemon = daemon

        def start(self):
            calls.append("thread_start:" + str(self.name))
            self.target()

    self_ = Fake(
        _agent_running=True,
        _pet_reasoning=True,
        _spinner_text="spin",
        _last_scrollback_tool="tool",
        _tool_start_time=9.5,
        _pending_tool_info={"a": 1},
        _interactive_turn=True,
        _should_exit=False,
        _app=types.SimpleNamespace(invalidate=lambda: calls.append("invalidate")),
        _pet_react_turn_end=lambda: calls.append("pet_react_turn_end"),
        _turn_summary_emit=lambda: calls.append("turn_summary_emit"),
        _last_turn_interrupted=bool(req.get("last_turn_interrupted")),
        _recover_terminal_after_interrupt=lambda: calls.append("recover_after_interrupt"),
        _drain_interrupt_queue_to_pending_input=lambda: calls.append("drain_interrupt_queue"),
        _maybe_continue_goal_after_turn=goal_hook,
        _maybe_complete_loop_tick_after_turn=loop_hook,
        _voice_mode=bool(req.get("voice_mode")),
        _voice_continuous=bool(req.get("voice_continuous")),
        _voice_recording=bool(req.get("voice_recording")),
        _voice_tts=bool(req.get("voice_tts")),
        _voice_barge_capture=types.SimpleNamespace(
            is_set=lambda: bool(req.get("barge_capture"))),
        _voice_tts_done=types.SimpleNamespace(
            wait=lambda timeout=None: calls.append("tts_wait:%s" % timeout)),
        _voice_start_recording=lambda: calls.append("voice_start_recording"),
        _drain_process_notifications=lambda where: calls.append("drain:" + where),
    )
    if req.get("has_running_tool_labels"):
        self_._running_tool_labels = ["x"]

    def debug(fmt, *a):
        hook_err.append(fmt % a if a else fmt)

    pairs = [
        (m, "threading", types.SimpleNamespace(Thread=FakeThread)),
        (m, "time", types.SimpleNamespace(sleep=lambda s: calls.append("sleep"))),
        (m, "logging", types.SimpleNamespace(debug=debug)),
    ]
    with Patched(pairs), _quiet():
        self_._tui_after_turn()

    return {
        "state": {
            "agent_running": self_._agent_running,
            "pet_reasoning": self_._pet_reasoning,
            "spinner_text": self_._spinner_text,
            "last_scrollback_tool": self_._last_scrollback_tool,
            "tool_start_time": self_._tool_start_time,
            "pending_tool_info": len(self_._pending_tool_info),
            "labels_present": "_running_tool_labels" in self_.__dict__,
            "interactive_turn": self_._interactive_turn,
        },
        "calls": calls,
        "hook_errors": len(hook_err),
    }


# ---------------------------------------------------------------- shutdown_plan


def shutdown_plan(req):
    calls = []
    agent = req.get("agent")
    sid = agent.get("session_id") if isinstance(agent, dict) else None
    gateway_owns = bool(req.get("gateway_owns"))
    delete_ok = bool(req.get("delete_returns"))

    ends = []
    deletes = []
    discards = []
    interrupts = []

    class Db:
        def end_session(self, session_id, reason):
            calls.append("end_session")
            ends.append((session_id, reason))

        def delete_session(self, session_id, sessions_dir=None):
            calls.append("delete_session")
            deletes.append(session_id)
            return delete_ok

    db = Db() if req.get("session_db") else None
    recordings = {}

    def rec(name):
        def _r(*a, **k):
            calls.append(name)
            recordings[name] = a
            return True
        return _r

    shell = Fake(
        _should_exit=False,
        _pet_stop_anim=lambda: calls.append("pet_stop_anim"),
        agent=(types.SimpleNamespace(session_id=sid) if isinstance(agent, dict) else None),
        _agent_running=bool(req.get("agent_running")),
        _detach_on_exit=bool(req.get("detach_on_exit")),
        _voice_recorder=(types.SimpleNamespace(shutdown=lambda: calls.append("voice_shutdown"))
                         if req.get("voice_recorder") else None),
        _persist_active_session_before_close=lambda: calls.append("persist_session"),
        _viewer_disconnected=bool(req.get("viewer_disconnected")),
        _session_db=db,
        _delete_session_on_exit=bool(req.get("delete_on_exit")),
        _discard_session_if_empty=lambda session_id: (
            calls.append("discard"), discards.append(session_id)),
        session_id=req.get("session_id"),
        _gateway_session_open=bool(req.get("gateway_session_open")),
        _print_exit_summary=lambda: calls.append("exit_summary"),
        _release_active_session=lambda: calls.append("release_session"),
    )

    def ask(worktree):
        calls.append("ask_exit_choice")
        if req.get("ask_raises"):
            raise KeyboardInterrupt

    pairs = [
        (climod, "_cprint", lambda *a, **k: calls.append("cprint")),
        (climod, "_invoke_interrupted_session_end",
         lambda ag, s, why: (calls.append("interrupted_end"), interrupts.append((s, why)))),
        (climod, "_run_cleanup", lambda: calls.append("run_cleanup")),
        (climod, "set_approval_callback", rec("unset")),
        (climod, "set_secret_capture_callback", rec("unset")),
        (climod, "set_sudo_password_callback", rec("unset")),
        (climod, "_signal_watchdog_armed", bool(req.get("watchdog_armed"))),
        (climod, "_active_worktree", req.get("active_worktree")),
        (shutmod, "_gateway_owns_session_end", lambda ag: gateway_owns),
        (vaultmod, "lock", lambda: calls.append("vault_lock")),
        (vaultmod, "set_code_prompt_callback", rec("unset")),
        (vaultmod, "set_save_login_prompt_callback", rec("unset")),
        (vaultmod, "set_unlock_prompt_callback", rec("unset")),
        (voicemod, "cleanup_temp_recordings", lambda: calls.append("cleanup_recordings")),
        (wtmod, "ask_exit_choice", ask),
        (wtmod, "apply_exit_choice", lambda wt, cp: calls.append("apply_exit_choice")),
        (m, "request_hard_interrupt", lambda ag: calls.append("hard_interrupt")),
    ]
    with Patched(pairs), _quiet():
        shell._tui_shutdown()

    closing = ends[0][0] if ends else None
    return {
        "calls": calls,
        "hard_interrupt": "hard_interrupt" in calls,
        "voice_shutdown": "voice_shutdown" in calls,
        "should_exit": bool(shell._should_exit),
        "closing_sid": closing,
        "end_reason": ends[0][1] if ends else None,
        "delete_called": bool(deletes),
        "discard": discards[0] if discards else None,
        "interrupted_end": interrupts[0][1] if interrupts else None,
        "cprint": calls.count("cprint"),
        "unset_count": sum(1 for c in calls if c == "unset"),
        "cleanup": "run_cleanup" in calls,
        "exit_summary": "exit_summary" in calls,
        "release": "release_session" in calls,
        "vault_lock": "vault_lock" in calls,
    }


# ---------------------------------------------------------------- driver

FUNCS = {
    "closing_session": closing_session,
    "hold_step": hold_step,
    "build_application": build_application,
    "signal_plan": signal_plan,
    "slash_input": slash_input,
    "after_turn": after_turn,
    "shutdown_plan": shutdown_plan,
}


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = FUNCS[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
