#!/usr/bin/env python3
"""Differential oracle for gateway/session_completion_notes.py (gate w26_sessioncom_fd19d0).

The admission queue, the store and the viewer set are the seam.  This oracle
drives the module's own pure decision functions on the fixture values:

  * ``_producer`` over an encoded identity string (json.loads(...)[0]);
  * ``noted_completion`` over a local completion admission row;
  * ``_wakes_unviewed_parent`` with ``_closed_by_user`` supplied as a fixed
    boolean (the close registry is the seam) and a plain counter object in
    place of the live session.
"""
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_sessioncom_fd19d0"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
os.environ.setdefault("USERPROFILE", str(QUARRY.parent))
os.environ.setdefault("HOME", str(QUARRY.parent))

from gateway import session_completion_notes as live  # noqa: E402

WANTED = {"producer", "noted_completion", "wakes", "constants"}


def producer(req):
    return live._producer(req.get("identity"))


def noted_completion(req):
    row = req.get("row")
    if not isinstance(row, dict):
        return False
    payload = row.get("payload")
    if not isinstance(payload, dict):  # a non-mapping payload cannot be indexed
        return False
    return bool(live.noted_completion({"payload": payload}))


def wakes(req):
    descriptor = req.get("descriptor")
    descriptor = descriptor if isinstance(descriptor, dict) else {}
    counter = req.get("unattended_wakes")
    counter = counter if isinstance(counter, (int, float)) and not isinstance(counter, bool) else 0
    session = SimpleNamespace(unattended_wakes=int(counter))
    closed = bool(req.get("closed"))
    with mock.patch.object(live, "_closed_by_user", lambda authority, session_id: closed):
        ran = live._wakes_unviewed_parent(None, "s-1", session, descriptor)
    return {"wakes": bool(ran), "unattended_wakes": session.unattended_wakes}


def constants():
    return {
        "noted_producers": sorted(live._NOTED_PRODUCERS),
        "always_noted": sorted(live._ALWAYS_NOTED),
        "unattended_wake_limit": live.UNATTENDED_WAKE_LIMIT,
    }


def expected(r):
    f = r.get("func")
    if f == "producer":
        return producer(r)
    if f == "noted_completion":
        return noted_completion(r)
    if f == "wakes":
        return wakes(r)
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
