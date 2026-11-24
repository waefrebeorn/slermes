#!/usr/bin/env python3
"""Differential oracle for tools/process_registry_awaiting.py
(gate w26_processreg_e923be).

The live module is imported as-is.  Its two locks and its live-process probe are
replaced by fixed inputs: a fake registry exposes ``_any_running`` over a list
the fixture already called alive, ``completion_queue`` is a real ``queue.Queue``
filled from the fixture, and the module-level ``_in_flight`` Counter is set per
case and restored afterwards.  What is compared is exactly what those values
decide.
"""
import json
import queue
import sys
import threading
from collections import Counter
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_processreg_e923be"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tools import process_registry_awaiting as live  # noqa: E402

WANTED = {"owner", "notifies", "retained", "adjust", "awaits"}

SESSION_DEFAULTS = {
    "owner_task_id": None,
    "notify_on_complete": False,
    "watch_patterns": None,
    "_watch_disabled": False,
    "_finish_claimed": False,
    "exited": False,
}


class Session:
    """A registry entry: the attributes the live predicates read, nothing else."""

    def __init__(self, raw):
        data = dict(SESSION_DEFAULTS)
        data.update(raw if isinstance(raw, dict) else {})
        self.__dict__.update({k: v for k, v in data.items() if k != "_completion_event"})
        self._completion_event = threading.Event()
        if data.get("_completion_event"):
            self._completion_event.set()


class Registry:
    """The two stores plus a queue; _any_running answers over the given list."""

    def __init__(self, state):
        running = state.get("running")
        finished = state.get("finished")
        queued = state.get("queued")
        self._running = {i: Session(s) for i, s in enumerate(running if isinstance(running, list) else [])}
        self._finished = {1000 + i: Session(s) for i, s in enumerate(finished if isinstance(finished, list) else [])}
        self._lock = threading.RLock()
        self.completion_queue = queue.Queue()
        for evt in queued if isinstance(queued, list) else []:
            self.completion_queue.put(evt)
        inflight = state.get("in_flight")
        live._in_flight = Counter(inflight if isinstance(inflight, dict) else {})

    def _any_running(self, predicate):
        return any(predicate(s) for s in self._running.values())


def expected(req):
    f = req.get("func")
    if f == "owner":
        return live._owner(req.get("evt"))
    if f == "notifies":
        return live._notifies(Session(req.get("session")))
    if f == "retained":
        return live.retained_for_delivery(Session(req.get("session")))
    if f == "adjust":
        raw = req.get("in_flight")
        live._in_flight = Counter(raw if isinstance(raw, dict) else {})
        owners = req.get("owners")
        live._adjust(owners if isinstance(owners, list) else [], int(req.get("delta") or 0))
        return dict(live._in_flight)
    if f == "awaits":
        state = req.get("state")
        state = state if isinstance(state, dict) else {}
        owners = state.get("owners")
        return bool(live.awaits_notification(Registry(state), owners if isinstance(owners, list) else []))
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
