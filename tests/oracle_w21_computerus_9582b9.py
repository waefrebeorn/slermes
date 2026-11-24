#!/usr/bin/env python3
"""Differential oracle for hermes_cli/subcommands/computer_use_screen.py
(gate w21_computerus_9582b9).

The live module is executable stdlib code: the oracle imports the real file and
stubs ONLY the host seams it reaches through `from tools.bot_desktop import ...`
-- runtime (Bot Desktop process control), lease (control holder) and install
(the package manager runner, incl. sudo). stdout is captured verbatim so the C
port must reproduce every printed line, and argparse-free `Namespace` args are
built per case. In --json mode the printed payload is parsed back to JSON, so
only the payload shape is compared (separators are presentation, not
behaviour).
"""
import builtins
import importlib.util
import io
import json
import sys
import types
from contextlib import redirect_stdout
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "subcommands" / "computer_use_screen.py"
GATE = "w21_computerus_9582b9"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))


def _fake_packages():
    tools = types.ModuleType("tools")
    tools.__path__ = []
    bot = types.ModuleType("tools.bot_desktop")
    bot.__path__ = []
    runtime = types.ModuleType("tools.bot_desktop.runtime")
    lease = types.ModuleType("tools.bot_desktop.lease")
    install = types.ModuleType("tools.bot_desktop.install")
    bot.runtime, bot.lease, bot.install = runtime, lease, install
    tools.bot_desktop = bot
    sys.modules["tools"] = tools
    sys.modules["tools.bot_desktop"] = bot
    sys.modules["tools.bot_desktop.runtime"] = runtime
    sys.modules["tools.bot_desktop.lease"] = lease
    sys.modules["tools.bot_desktop.install"] = install
    return runtime, lease, install


RUNTIME, LEASE, INSTALL = _fake_packages()


def load():
    spec = importlib.util.spec_from_file_location("computer_use_screen_live", SRC)
    mod = importlib.util.module_from_spec(spec)
    sys.modules["computer_use_screen_live"] = mod
    spec.loader.exec_module(mod)
    return mod


LIVE = load()


class _St:
    pass


class _Holder:
    def __init__(self, holder):
        self.holder = holder


class _Args:
    def __init__(self, **kw):
        for k, v in kw.items():
            setattr(self, k, v)


class _InstallBusy(Exception):
    pass


def _state_status(state):
    st = _St()
    st.supported = bool(state.get("supported"))
    st.installed = bool(state.get("installed"))
    st.running = bool(state.get("running"))
    st.missing = list(state.get("missing") or [])
    st.install_command = state.get("install_command")
    st.profile = state.get("profile") or ""
    st.display = state.get("display") or ""
    st.geometry = state.get("geometry") or ""
    st.pid = state.get("pid") or 0
    st.socket = state.get("socket") or ""
    st.blocker = state.get("blocker")
    st.as_dict = lambda: dict(state.get("as_dict") or {})
    RUNTIME.status = lambda: st
    LEASE.HUMAN = "human"
    view = dict(state.get("lease_view") or {})
    LEASE.get = lambda: _Holder(view.get("holder"))
    LEASE.public_view = lambda holder: dict(view)
    return st


def run(status=None, start=None, stop=None, install=None, tty=False, answer=""):
    """Call one live handler with captured stdout; returns (rc, text)."""
    if status is not None:
        _state_status(status)
        if status.get("start_error") is not None:
            def _start():
                raise RuntimeError(status["start_error"])
            RUNTIME.start = _start
        else:
            st = _state_status(status)
            RUNTIME.start = lambda: st
        LEASE.release = lambda unless_human=True: _Holder(status.get("released_holder", "agent"))
        RUNTIME.stop = lambda: bool(status.get("stopped"))
        RUNTIME.is_supported_host = lambda: bool(status.get("supported"))
        handler = LIVE.SCREEN_ACTIONS["status"]
        args = _Args(json=bool(status.get("json")))
    elif start is not None:
        if start.get("error"):
            def _raise():
                raise RuntimeError(start["error"])
            RUNTIME.start = _raise
        else:
            st = _St()
            st.profile = start.get("profile") or ""
            st.display = start.get("display") or ""
            st.geometry = start.get("geometry") or ""
            RUNTIME.start = lambda: st
        handler = LIVE.SCREEN_ACTIONS["start"]
        args = _Args()
    elif stop is not None:
        RUNTIME.is_supported_host = lambda: bool(stop.get("supported"))
        LEASE.HUMAN = "human"
        LEASE.release = lambda unless_human=True: _Holder(stop.get("released_holder", "agent"))
        RUNTIME.stop = lambda: bool(stop.get("stopped"))
        handler = LIVE.SCREEN_ACTIONS["stop"]
        args = _Args(force=bool(stop.get("force")))
    else:
        RUNTIME.is_supported_host = lambda: bool(install.get("supported"))
        seq = [list(install.get("missing") or []), list(install.get("after_missing") or [])]

        def _missing():
            return seq.pop(0) if seq else []
        RUNTIME.missing_binaries = _missing
        RUNTIME.install_command = lambda: install.get("command")
        INSTALL.NO_SUDO = 111
        INSTALL.InstallBusy = _InstallBusy

        def _packages(ask_password, on_line):
            if install.get("busy"):
                raise _InstallBusy(install.get("busy_message") or "")
            return 111 if install.get("no_sudo") else int(install.get("rc") or 0)
        INSTALL.install_packages = _packages
        handler = LIVE.SCREEN_ACTIONS["install"]
        args = _Args(yes=bool(install.get("yes")))

    fake_sys = types.SimpleNamespace(
        stdin=types.SimpleNamespace(isatty=lambda: bool(tty)))
    saved_sys, saved_input = LIVE.sys, builtins.input

    def _input(prompt=""):
        sys.stdout.write(prompt)
        return answer
    LIVE.sys = fake_sys
    builtins.input = _input
    buf = io.StringIO()
    try:
        with redirect_stdout(buf):
            rc = handler(args)
    finally:
        LIVE.sys = saved_sys
        builtins.input = saved_input
    return rc, buf.getvalue()


def expected(r):
    f = r.get("func")
    if f == "actions":
        return dict(LIVE.SCREEN_ACTIONS_NAMES)
    if f == "status":
        rc, text = run(status=r)
        if r.get("json"):
            return {"rc": rc, "payload": json.loads(text)}
        return {"rc": rc, "text": text}
    if f == "start":
        rc, text = run(start=r)
        return {"rc": rc, "text": text}
    if f == "stop":
        rc, text = run(stop=r)
        return {"rc": rc, "text": text}
    if f == "install":
        rc, text = run(install=r, tty=bool(r.get("tty")), answer=r.get("answer") or "")
        return {"rc": rc, "text": text}
    raise AssertionError("unknown func %r" % (f,))


LIVE.SCREEN_ACTIONS_NAMES = {k: v.__name__ for k, v in LIVE.SCREEN_ACTIONS.items()}


def main():
    cases = mism = 0
    wanted = {"status", "start", "stop", "install", "actions"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001 - surfaced as a mismatch
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
