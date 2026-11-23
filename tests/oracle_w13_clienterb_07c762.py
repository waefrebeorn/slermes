#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cli_enter_burst.py (gate w13_clienterb_07c762).

monotonic() and the cli/event/buf object state are injected from each case via
plain fakes; the LIVE note_text_change / handle_burst_enter functions run.
"""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
SRC = "C:/Users/eman5/hermes-agent/hermes_cli/cli_enter_burst.py"
spec = importlib.util.spec_from_file_location("w13burst_live", SRC)
live = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live)


class Loop:
    def __init__(self):
        self.calls = []

    def call_later(self, delay, *args):
        self.calls.append((delay, args))

    def call_soon(self, fn, *a):
        pass


class KP:
    def __init__(self, q):
        self.input_queue = q


class App:
    def __init__(self, kp, loop, running):
        self.key_processor = kp
        self.loop = loop
        self.is_running = running

    def invalidate(self):
        pass


class Ev:
    def __init__(self, app):
        self.app = app


class Buf:
    def __init__(self, doc):
        self.document = doc

    def insert_text(self, text):
        self.document = self.document + text


class Cli:
    def __init__(self):
        self._submitted = 0
        self._tui_last_text_change = 0.0

    def _tui_submit_buffer(self, event, buf):
        self._submitted += 1


def mk_cli(r):
    cli = Cli()
    cli._tui_last_text_change = r.get("last", 0.0)
    cli._tui_paste_burst_at = r.get("burst", -1e9)
    if r.get("held", False):
        cli._tui_held_enter = object()
    live.monotonic = lambda: r.get("now", 0.0)
    return cli


def note(r):
    cli = mk_cli(r)
    ret = live.note_text_change(cli)
    return {"ret": ret, "burst_at": cli._tui_paste_burst_at,
            "held": getattr(cli, "_tui_held_enter", None) is not None}


def handle(r):
    cli = mk_cli(r)
    loop = Loop() if r.get("has_loop", True) else None
    kp = KP(r.get("queue", False)) if r.get("has_kp", True) else None
    app = App(kp, loop, r.get("running", True))
    ev = Ev(app)
    buf = Buf(r.get("doc", "hello"))
    ret = live.handle_burst_enter(cli, ev, buf)
    out = {"ret": ret, "doc": buf.document,
           "held": getattr(cli, "_tui_held_enter", None) is not None,
           "burst_at": cli._tui_paste_burst_at, "submit": cli._submitted,
           "scheduled": bool(loop and loop.calls)}
    out["post_doc"] = buf.document
    out["post_held"] = getattr(cli, "_tui_held_enter", None) is not None
    out["post_submit"] = cli._submitted
    if r.get("settle", False) and loop and loop.calls:
        settle = loop.calls[0][1][-1]
        if r.get("clear_held", False):
            cli._tui_held_enter = None
        if r.get("edit_doc", False):
            buf.document = buf.document + "X"
        if r.get("change_last", False):
            cli._tui_last_text_change = r.get("last", 0.0) + 1.0
        settle()
        out["post_doc"] = buf.document
        out["post_held"] = getattr(cli, "_tui_held_enter", None) is not None
        out["post_submit"] = cli._submitted
    return out


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    if func == "note_text_change":
        want = note(r)
    elif func == "handle_burst_enter":
        want = handle(r)
    else:
        continue
    got = r.get("out")
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH {func} in={r} py={want!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w13_clienterb_07c762: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
