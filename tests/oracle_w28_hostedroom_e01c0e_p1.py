#!/usr/bin/env python3
"""Differential oracle for the task/attempt identity half of
``tui_gateway/hosted_room_service.py`` (gate w28_hostedroom_e01c0e_p1).

The live module is imported by path from the quarry checkout and its pure
identity helpers are called directly: ``task_member_id``, ``_member_of``,
``attempt_action``, ``_approvable_attempts``, ``_attempt_actions`` and
``_member_is_peer``.  Tasks are rebuilt as the real ``HostedTaskIdentity``
dataclass so the live attribute access is exercised; the room state
``_member_is_peer`` reads is a fixed fixture (the store is the seam).
"""
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_hostedroom_e01c0e_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tui_gateway import hosted_room_service as live  # noqa: E402

WANTED = {"task_member_id", "member_of", "attempt_action", "approvable_attempts",
          "attempt_actions", "member_is_peer"}


def task(raw):
    t = dict(raw or {})
    ident = t.get("identity")
    if isinstance(ident, dict):
        t["identity"] = live.HostedTaskIdentity(
            room_id=str(ident.get("room_id", "")), task_id=str(ident.get("task_id", "")),
            thread_id=str(ident.get("thread_id", "")), turn_id=str(ident.get("turn_id", "")))
    return t


def tasks(raw):
    return [task(item) for item in (raw or []) if isinstance(item, dict)]


def service(room):
    """A service whose room read is the fixture: no store is opened."""
    svc = live.HostedRoomService.__new__(live.HostedRoomService)
    svc._fixture_room = room if isinstance(room, dict) else {}
    svc._room = lambda room_id: svc._fixture_room
    return svc


def expected(r):
    f = r.get("func")
    if f == "task_member_id":
        return live.task_member_id(task(r.get("task")))
    if f == "member_of":
        return live._member_of(task(r.get("task")))
    if f == "attempt_action":
        return live.attempt_action(r.get("kind") or "", task(r.get("task")))
    if f == "approvable_attempts":
        return sorted(list(row) for row in live._approvable_attempts(tasks(r.get("tasks"))))
    if f == "attempt_actions":
        svc = live.HostedRoomService.__new__(live.HostedRoomService)
        return svc._attempt_actions("room", tasks(r.get("tasks")))
    if f == "member_is_peer":
        return service(r.get("room"))._member_is_peer("room", r.get("member_id") or "")
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
