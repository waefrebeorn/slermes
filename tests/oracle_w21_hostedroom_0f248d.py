#!/usr/bin/env python3
"""Differential oracle for tui_gateway/hosted_room_prepare_stall.py
(gate w21_hostedroom_0f248d).

The live module imports cleanly, so the oracle drives it directly:

* `_describe` and `retry_action` are called as-is;
* `PrepareStallTracker` is the live tracker -- the fixture's `kind` selects a
  real exception instance (`TransientStoreError`, a `PermanentStoreError` whose
  `__cause__` is sqlite's "no such table", or a plain failure), and the answers
  (`stalled`, `last_error`, `emitted`, the private streak count) come from it;
* `report_stall` runs against a fake `hosted_rooms` namespace that records the
  one `append_event` it would make, so the "already reported" rule is the live
  one rather than a re-statement of it.
"""
import importlib.util
import json
import sqlite3
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "tui_gateway" / "hosted_room_prepare_stall.py"
GATE = "w21_hostedroom_0f248d"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.core.errors import PermanentStoreError, TransientStoreError  # noqa: E402

_spec = importlib.util.spec_from_file_location("w21_prepare_stall_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["w21_prepare_stall_live"] = live
_spec.loader.exec_module(live)

_PLAIN = {"RuntimeError": RuntimeError, "ValueError": ValueError, "OSError": OSError}
_TYPES = {}


def _exc_class(name):
    """An exception class with the fixture's `type(exc).__name__` whose str()
    is just the message (unlike KeyError, which reprs its args)."""
    if not name:
        return RuntimeError
    if name in _PLAIN:
        return _PLAIN[name]
    if name not in _TYPES:
        _TYPES[name] = type(name, (RuntimeError,), {})
    return _TYPES[name]


def make_exc(kind, message, type_name):
    if kind == "transient":
        return TransientStoreError(message)
    if kind == "missing_table":
        err = PermanentStoreError(message)
        err.__cause__ = sqlite3.OperationalError("no such table: telegram_dm_topic_bindings")
        return err
    return _exc_class(type_name)(message)


def expected_describe(r):
    return live._describe(make_exc("permanent", r.get("message") or "", r.get("type")))


def expected_feed(r):
    tracker = live.PrepareStallTracker()
    room = r.get("room_id") or "room-1"
    emitted = []
    for ev in r.get("events") or []:
        op = ev.get("op", "failed")
        if op == "clear":
            tracker.clear(room)
        elif op == "stall":
            tracker.stall(room, make_exc(ev.get("kind", "permanent"), ev.get("message") or "",
                                         ev.get("type")))
        else:
            emitted.append(bool(tracker.failed(room, make_exc(ev.get("kind", "permanent"),
                                                              ev.get("message") or "",
                                                              ev.get("type")))))
    streak = tracker._rooms.get(room)
    return {
        "stalled": tracker.stalled(room),
        "error": tracker.last_error(room),
        "failures": streak.failures if streak else 0,
        "emitted": emitted,
    }


def expected_retry(r):
    return live.retry_action(r.get("room_id") or "", r.get("room") or {})


class FakeRooms:
    def __init__(self, room, tail):
        self.room = room
        self.tail = tail
        self.appended = []

    def room_state(self, store, *, placement, room_id):
        return self.room

    def read_events(self, store, *, placement, room_id, since_seq, limit):
        return {"events": self.tail}

    def append_event(self, store, **kwargs):
        self.appended.append(kwargs)


def expected_report(r):
    exc = make_exc("permanent", r.get("message") or "", r.get("type"))
    latest_seq = int(r.get("latest_seq") or 0)
    tail_id = r.get("tail_event_id")
    room = {"latest_seq": latest_seq, "authority_gateway_id": "gw-7", "authority_epoch": 4}
    tail = [{"event_id": tail_id}] if tail_id else []
    fake = FakeRooms(room, tail)
    real = live.hosted_rooms
    live.hosted_rooms = fake
    try:
        live.report_stall(None, r.get("room_id") or "room-1", exc, placement=None)
    finally:
        live.hosted_rooms = real
    if fake.appended:
        step = fake.appended[0]
        return {"append": True, "event_id": step["event_id"], "kind": step["kind"],
                "payload": step["payload"]}
    return {
        "append": False,
        "event_id": "%s%d" % (live._EVENT_PREFIX, latest_seq),
        "kind": "member.unavailable",
        "payload": {"error": "Members could not be woken: %s" % live._describe(exc),
                    "reason_code": "prepare_failed"},
    }


def expected(r):
    f = r.get("func")
    if f == "describe":
        return expected_describe(r)
    if f == "feed":
        return expected_feed(r)
    if f == "retry_action":
        return expected_retry(r)
    if f == "report_plan":
        return expected_report(r)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"describe", "feed", "retry_action", "report_plan"}
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
