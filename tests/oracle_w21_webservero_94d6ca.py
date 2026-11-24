#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_server_owner_exit.py (gate w21_webservero_94d6ca).

The live module is imported and both entry points run for real:
`should_retire_superseded` is called directly, and `start_owner_watchdog` really
starts its daemon thread over the live `_run_retirement_watchdog` loop.

Seams, named:
* `read_lock` -> the fixture's scripted `lock` per poll (the filesystem read of
  backend.lock.json; `read_valid_backend_lock` is not modelled).
* `fence.prepare()/commit()` -> the fixture's scripted verdicts (the live
  retirement fence closes admission and tears the backend down).
* `time.monotonic` -> the scripted clock, so `now() - started` is the fixture's
  `age_s` (`started` is the first value).
* `time.sleep` -> counted, not slept (the watchdog's poll interval).
* `web_server_skew_exit._log` -> a recorder (the retire reason it logs).
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "web_server_owner_exit.py"
GATE = "w21_webservero_94d6ca"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("web_server_owner_exit_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["web_server_owner_exit_live"] = live
_spec.loader.exec_module(live)

import hermes_cli.web_server_skew_exit as skew  # noqa: E402

LOCK_PATH = Path(QUARRY / "desktop-ssh" / "0123456789abcdef0123456789abcdef" / "backend.lock.json")


class _Server:
    should_exit = False


class _Log:
    def __init__(self):
        self.lines = []

    def warning(self, fmt, *args, **kwargs):
        self.lines.append(fmt % args if args else str(fmt))


def run_watchdog(nonce, max_polls, polls, fences):
    """Drive the live watchdog once; return its observable trace."""
    polls = polls or []
    fences = fences or []
    ages = [p.get("age_s", 0) for p in polls if p.get("lock") is not None]
    clock = iter([0.0] + ages)
    state = {"reads": 0, "prepare": 0, "commit": 0, "sleeps": 0, "fence_i": 0, "cur": {}}

    def read_lock(_path):
        idx = state["reads"]
        state["reads"] += 1
        if idx < len(polls):
            return polls[idx].get("lock")
        return None

    class Fence:
        def prepare(self):
            state["prepare"] += 1
            attempt = fences[state["fence_i"]] if state["fence_i"] < len(fences) else {}
            state["fence_i"] += 1
            state["cur"] = attempt
            return attempt.get("prepare") or {}

        def commit(self, token):
            state["commit"] += 1
            return state["cur"].get("commit") or {}

    server = _Server()
    log = _Log()
    sleeps = []
    original_sleep, original_log = skew.time.sleep, skew._log
    skew.time.sleep = lambda seconds: sleeps.append(seconds)
    skew._log = log
    try:
        thread = live.start_owner_watchdog(
            server, lock_path=LOCK_PATH, nonce=nonce, read_lock=read_lock, fence=Fence(),
            poll_s=live.DEFAULT_OWNER_POLL_S, now=lambda: next(clock), max_polls=max_polls)
        thread.join(15)
    finally:
        skew.time.sleep = original_sleep
        skew._log = original_log
    return {
        "retired": bool(server.should_exit),
        "polls": state["reads"],
        "prepare_calls": state["prepare"],
        "commit_calls": state["commit"],
        "sleeps": len(sleeps),
        "message": log.lines[0] if log.lines else None,
    }


def do_should_retire(r):
    try:
        return live.should_retire_superseded(lock=r.get("lock"),
                                             my_nonce=r.get("my_nonce", ""),
                                             age_s=r.get("age_s", 0))
    except KeyError as exc:
        return {"raised": "KeyError", "message": str(exc)}


def do_observe_message(r):
    """The message is the live `_observe` closure's, captured through a retiring run."""
    trace = run_watchdog(r.get("nonce", ""), 2,
                         [{"lock": {"spawnNonce": r.get("spawn_nonce")}, "age_s": 200}] * 2,
                         [{"prepare": {"ok": True, "token": "t"}, "commit": {"ok": True}}])
    return trace["message"]


def expected(r):
    f = r.get("func")
    if f == "default_poll":
        return live.DEFAULT_OWNER_POLL_S
    if f == "should_retire":
        return do_should_retire(r)
    if f == "observe_message":
        return do_observe_message(r)
    if f == "simulate":
        return run_watchdog(r.get("nonce", ""), r.get("max_polls", 0),
                            r.get("polls"), r.get("fences"))
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"default_poll", "should_retire", "observe_message", "simulate"}
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
