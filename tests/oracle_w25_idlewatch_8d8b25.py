#!/usr/bin/env python3
"""Differential oracle for tools/bot_desktop/idle_watch.py (gate w25_idlewatch_8d8b25).

The live module is imported and its two pure helpers are called on the values a
run would see.  The seams are given as fixed inputs: ``runtime._create_time``
is replaced with the fixture's ``born`` (the process table is not consulted) and
``runtime.DEFAULT_IDLE_STOP_MINUTES`` with the fixture's ``default_minutes``
when one is given (the config file is not read).  Nothing is stopped, spawned or
logged.
"""
import json
import os
import sys
import tempfile
from pathlib import Path

# Env seam: the live module resolves Path.home() at import time, and ntpath
# only knows USERPROFILE.  Give it one when the caller's shell dropped it.
if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = os.environ.get("HOME") or tempfile.gettempdir()

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_idlewatch_8d8b25"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tools.bot_desktop import idle_watch as live  # noqa: E402
from tools.bot_desktop import runtime  # noqa: E402

WANTED = {"next_check_in", "own_launcher", "constants"}


def next_check_in(r):
    minutes = r.get("default_minutes")
    saved = runtime.DEFAULT_IDLE_STOP_MINUTES
    if minutes is not None:
        runtime.DEFAULT_IDLE_STOP_MINUTES = minutes
    try:
        return live.next_check_in(r.get("limit"), r.get("idle"))
    finally:
        runtime.DEFAULT_IDLE_STOP_MINUTES = saved


def own_launcher(r):
    argv = r.get("argv")
    argv = argv if isinstance(argv, list) else []
    born = r.get("born")
    saved = runtime._create_time
    runtime._create_time = lambda pid: born
    try:
        got = live._own_launcher(list(argv))
    finally:
        runtime._create_time = saved
    return None if got is None else [got[0], got[1]]


def constants():
    return {
        "min_delay_s": float(live._MIN_DELAY_S),
        "default_idle_stop_minutes": float(runtime.DEFAULT_IDLE_STOP_MINUTES),
        "default_idle_stop_seconds": float(runtime.DEFAULT_IDLE_STOP_MINUTES) * 60.0,
    }


def expected(r):
    f = r.get("func")
    if f == "next_check_in":
        return next_check_in(r)
    if f == "own_launcher":
        return own_launcher(r)
    if f == "constants":
        return constants()
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
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
