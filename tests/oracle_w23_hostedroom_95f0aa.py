#!/usr/bin/env python3
"""Differential oracle for persistence/gateway/hosted_rooms.py (gate
w23_hostedroom_95f0aa).

The hosted-room repositories are sqlite Protocols, so nothing about them is
differential here. What is compared is the pure half of the module, called
live with no seam at all:

  * ``grant_digest`` -- the SHA-256 hex an owed revoke is keyed under;
  * ``room_grant_hard_expiry`` -- the horizon read from a grant's unsigned
    payload through lenient urlsafe base64 and JSON, with every unreadable
    grant landing on ``now + MAX_OWED_REVOKE_SECONDS``;
  * ``_task_identifier`` and ``HostedTaskIdentity`` -- the room-driver
    identifier rule and the stable task identity built from it;
  * the bounded constants and the task-status sets.
"""
import importlib
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w23_hostedroom_95f0aa"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

live = importlib.import_module("persistence.gateway.hosted_rooms")
WANTED = {"constants", "grant_digest", "hard_expiry", "task_identifier", "task_identity"}


def constants(_req):
    return {
        "MAX_OWED_REVOKE_SECONDS": live.MAX_OWED_REVOKE_SECONDS,
        "MAX_OWED_REVOKES_PER_MEMBER": live.MAX_OWED_REVOKES_PER_MEMBER,
        "MAX_OWED_REVOKES": live.MAX_OWED_REVOKES,
        "TASK_STATUSES": sorted(live.TASK_STATUSES),
        "TERMINAL_STATUSES": sorted(live.TERMINAL_STATUSES),
        "SHOWN_TASK_STATUSES": list(live.SHOWN_TASK_STATUSES),
    }


def grant_digest(req):
    return live.grant_digest(req.get("grant"))


def hard_expiry(req):
    return live.room_grant_hard_expiry(req.get("grant"), now=req.get("now"))


def task_identifier(req):
    try:
        value = live._task_identifier(req.get("value"), label=req.get("label", "value"))
    except Exception as exc:  # noqa: BLE001 -- DriverValidationError's message is the answer
        return {"ok": False, "error": str(exc)}
    return {"ok": True, "value": value}


def task_identity(req):
    try:
        ident = live.HostedTaskIdentity(room_id=req.get("room_id"), task_id=req.get("task_id"),
                                        thread_id=req.get("thread_id"), turn_id=req.get("turn_id"))
    except Exception as exc:  # noqa: BLE001
        return {"ok": False, "error": str(exc)}
    return {"ok": True, "value": {"room_id": ident.room_id, "task_id": ident.task_id,
                                 "thread_id": ident.thread_id, "turn_id": ident.turn_id}}


def expected(r):
    f = r.get("func")
    req = r.get("req")
    req = req if isinstance(req, dict) else {}
    if f == "constants":
        return constants(req)
    if f == "grant_digest":
        return grant_digest(req)
    if f == "hard_expiry":
        return hard_expiry(req)
    if f == "task_identifier":
        return task_identifier(req)
    if f == "task_identity":
        return task_identity(req)
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
