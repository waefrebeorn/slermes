#!/usr/bin/env python3
"""Differential oracle for agent/chat_completion_wait_notice.py
(gate w23_chatcomple_c9e7a5).

The module is presentation only: no clock, no filesystem, no network.  So the
live functions run unmodified and the fixture supplies exactly what the caller
holds -- the phase and silence, the watchdog ``(label, seconds_until_it_fires)``
tuple, the timestamps the watchdogs already measured, and a ``*_finite`` flag
standing in for the ``math.inf`` a JSON number cannot carry.

``WaitNoticeState`` is stateful, so a case is a script of steps and ``reset``
steps are replayed against a live instance.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "agent" / "chat_completion_wait_notice.py"
GATE = "w23_chatcomple_c9e7a5"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("wait_notice_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["wait_notice_live"] = live
_spec.loader.exec_module(live)

WANTED = {"wait_text", "deadline", "should_emit", "near", "phases", "near_secs"}


def watchdog_of(value):
    if not isinstance(value, (list, tuple)) or len(value) != 2:
        return None
    return (value[0], value[1])


def wait_text(world):
    world = world if isinstance(world, dict) else {}
    return live.wait_notice_text(world.get("model"), world.get("silence_secs"),
                                 world.get("phase"), watchdog_of(world.get("watchdog")))


def timeout_of(world, key, flag):
    """A float, or math.inf when the fixture says the timeout is unbounded."""
    if not world.get(flag, True):
        return float("inf")
    return float(world.get(key, 0.0))


def deadline(world):
    world = world if isinstance(world, dict) else {}
    out = live.codex_watchdog_deadline(
        stale_timeout=timeout_of(world, "stale_timeout", "stale_finite"),
        ttfb_enabled=bool(world.get("ttfb_enabled", False)),
        ttfb_timeout=timeout_of(world, "ttfb_timeout", "ttfb_finite"),
        last_event_ts=world.get("last_event_ts"),
        last_progress_ts=world.get("last_progress_ts"),
        retry_started_ts=world.get("retry_started_ts"),
        call_start=float(world.get("call_start", 0.0)),
        idle_enabled=bool(world.get("idle_enabled", False)),
        idle_timeout=timeout_of(world, "idle_timeout", "idle_finite"),
        idle_requires_progress=bool(world.get("idle_requires_progress", False)),
        elapsed=float(world.get("elapsed", 0.0)),
        progress_timeout=float(world.get("progress_timeout", 0.0)),
    )
    return None if out is None else [out[0], out[1]]


def should_emit(world):
    world = world if isinstance(world, dict) else {}
    state = live.WaitNoticeState()
    out = []
    for step in world.get("steps") or []:
        if not isinstance(step, dict):
            continue
        if step.get("reset"):
            state.reset()
            continue
        out.append(bool(state.should_emit(step.get("phase"), watchdog_of(step.get("watchdog")))))
    return out


def near(world):
    world = world if isinstance(world, dict) else {}
    return bool(live._near_deadline(watchdog_of(world.get("watchdog"))))


def expected(r):
    f = r.get("func")
    if f == "wait_text":
        return wait_text(r.get("world"))
    if f == "deadline":
        return deadline(r.get("world"))
    if f == "should_emit":
        return should_emit(r.get("world"))
    if f == "near":
        return near(r.get("world"))
    if f == "phases":
        return list(live._PHASE_TEXT)
    if f == "near_secs":
        return live.NEAR_DEADLINE_SECS
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
