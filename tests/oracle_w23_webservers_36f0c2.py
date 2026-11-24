#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_server_skew_exit.py (gate
w23_webservers_36f0c2).

The live watchdog is a daemon thread, so the oracle runs it for real and
scripts its two seams:

  * ``skew_fn`` answers the fixture's per-poll reading (the boot/disk revision
    pair, or None for "no drift");
  * ``hermes_cli.update_lock.update_in_progress`` is a stub module whose answer
    -- or whose raise -- comes from the same reading, so the live
    ``_update_in_progress`` (and its "cannot prove the swap is over: wait"
    fallback) is what decides;
  * the fence records ``prepare()``/``commit(token)`` calls and answers the
    fixture's verdicts.

``poll_s`` is pinned to 0 so the thread finishes promptly, ``max_polls`` bounds
it exactly as the fixture says, and the thread is joined before the trace is
read.  ``should_retire_for_skew`` runs live and directly.
"""
import importlib.util
import json
import sys
import threading
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "web_server_skew_exit.py"
GATE = "w23_webservers_36f0c2"
JOIN_TIMEOUT = 30.0

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("web_server_skew_exit_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["web_server_skew_exit_live"] = live
_spec.loader.exec_module(live)

WANTED = {"poll_s", "confirmations", "should_retire", "reason", "run"}


def as_skew(value):
    """The fixture's pair as the tuple the module expects."""
    if isinstance(value, list):
        return tuple(value)
    return value


class RecordingFence:
    """prepare()/commit() answering the fixture, clamped to the last entry."""

    def __init__(self, spec):
        self.spec = spec if isinstance(spec, dict) else {}
        self.prepares = 0
        self.commits = 0
        self.tokens = []

    def _flag(self, key, index):
        vals = self.spec.get(key) or [False]
        return bool(vals[min(index, len(vals) - 1)])

    def prepare(self):
        index = self.prepares
        self.prepares += 1
        tokens = self.spec.get("tokens") or [""]
        return {"ok": self._flag("prepare", index),
                "token": tokens[min(index, len(tokens) - 1)]}

    def commit(self, token):
        self.commits += 1
        self.tokens.append(token)
        return {"ok": self._flag("commit", self.commits - 1)}


class RecordingLog:
    def __init__(self):
        self.lines = []

    def warning(self, fmt, *args):
        self.lines.append(fmt % args if args else fmt)

    def debug(self, *args, **kwargs):
        pass


def run(world):
    world = world if isinstance(world, dict) else {}
    readings = world.get("readings") or []
    max_polls = world.get("max_polls")
    if max_polls is None:
        # The script bounds an unbounded loop: past the last reading nothing
        # can warrant retirement, so the trace is the same either way.
        max_polls = len(readings)

    server = types.SimpleNamespace(should_exit=bool(world.get("should_exit", False)))
    holder = {"cur": {}}
    state = {"i": 0}

    def skew_fn():
        index = state["i"]
        state["i"] += 1
        cur = readings[index] if index < len(readings) else {}
        holder["cur"] = cur
        return as_skew(cur.get("skew"))

    def update_in_progress():
        cur = holder["cur"]
        if cur.get("update_raises"):
            raise RuntimeError("update lock unavailable")
        return bool(cur.get("update", False))

    import hermes_cli  # real package; only its attribute is swapped
    saved_attr = getattr(hermes_cli, "update_lock", None)
    saved_mod = sys.modules.get("hermes_cli.update_lock", "ABSENT")
    stub = types.ModuleType("hermes_cli.update_lock")
    stub.update_in_progress = update_in_progress
    sys.modules["hermes_cli.update_lock"] = stub
    hermes_cli.update_lock = stub

    fence = RecordingFence(world.get("fence") or {})
    log = RecordingLog()
    saved_log = live._log
    live._log = log
    try:
        thread = live.start_code_skew_watchdog(server, skew_fn=skew_fn,
                                               fence=fence, poll_s=0.0,
                                               max_polls=max_polls)
        thread.join(JOIN_TIMEOUT)
        if thread.is_alive():
            raise RuntimeError("watchdog thread did not stop")
    finally:
        live._log = saved_log
        if saved_mod == "ABSENT":
            sys.modules.pop("hermes_cli.update_lock", None)
        else:
            sys.modules["hermes_cli.update_lock"] = saved_mod
        if saved_attr is None:
            try:
                del hermes_cli.update_lock
            except AttributeError:
                pass
        else:
            hermes_cli.update_lock = saved_attr

    return {"should_exit": bool(server.should_exit),
            "polls": state["i"],
            "prepare_calls": fence.prepares,
            "commit_calls": fence.commits,
            "tokens": fence.tokens,
            "log": log.lines}


def reason(world):
    """The exact live log line for one reading, or None."""
    world = world if isinstance(world, dict) else {}
    reading = {"skew": world.get("skew"),
               "update": world.get("update", False),
               "update_raises": world.get("update_raises", False)}
    trace = run({"readings": [reading, reading], "max_polls": 2,
                 "fence": {"prepare": [True], "commit": [True], "tokens": ["probe"]}})
    return trace["log"][0] if trace["log"] else None


def expected(r):
    f = r.get("func")
    if f == "poll_s":
        return live.DEFAULT_SKEW_POLL_S
    if f == "confirmations":
        return live._CONFIRMATIONS
    if f == "should_retire":
        return live.should_retire_for_skew(
            skew=as_skew(r.get("skew")),
            update_in_progress=bool(r.get("update", False)))
    if f == "reason":
        return reason(r)
    if f == "run":
        return run(r.get("world"))
    raise AssertionError("unknown func %r" % (f,))


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
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
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
