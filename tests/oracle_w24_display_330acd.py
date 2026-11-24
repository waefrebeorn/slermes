#!/usr/bin/env python3
"""Differential oracle for tools/connectors/display.py (gate w24_display_330acd).

The live module is imported and driven through its own code:

  * ``connected_slugs`` over a stub client whose ``list_connectors()`` answers
    the case's rows;
  * ``apply_answer`` on a real ``DisplayOperation`` (no targets, no I/O: the
    operation's only side effect is logger.info and a wake event), so the
    stored ``said`` and the settle reason are the live ones;
  * ``_result`` over a duck-typed operation and the live ``SettleReason`` enum,
    which is what the ``timed_out`` flag is decided against;
  * the off-desktop note and the deadline from the live module.

A live exception is the seam's error answer: the C port returns null there, so
the expected value is None.
"""
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_display_330acd"

_HOME = REPO.parent
for _key, _val in (("USERPROFILE", str(_HOME)),
                   ("LOCALAPPDATA", str(_HOME / "AppData" / "Local")),
                   ("APPDATA", str(_HOME / "AppData" / "Roaming"))):
    if not os.environ.get(_key):
        os.environ[_key] = _val

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import tools.connectors.display as live  # noqa: E402
from tools.connectors.operation import OPERATION_DEADLINE_SECONDS  # noqa: E402

WANTED = {"connected_slugs", "apply_answer", "result", "constants"}


def _client(items):
    rows = items if isinstance(items, list) else []
    return SimpleNamespace(list_connectors=lambda: rows)


def connected_slugs(r):
    return sorted(live.connected_slugs(_client(r.get("items"))))


def apply_answer(r):
    op = live.DisplayOperation([], session_key="w24", tool_call_id=None, call=None, suggested=[])
    live.apply_answer(op, r.get("raw"))
    return {"said": op.said,
            "settled": op.settled_at is not None,
            "settled_by": op.settled_by.value if op.settled_by else None}


def result(r):
    raw_before = r.get("before")
    before = {x for x in (raw_before if isinstance(raw_before, list) else []) if isinstance(x, str)}
    suggested = r.get("suggested") if isinstance(r.get("suggested"), list) else []
    by = r.get("settled_by")
    op = SimpleNamespace(suggested=suggested,
                         said=r.get("said"),
                         settled_by=live.SettleReason(by) if by is not None else None)
    return live._result(_client(r.get("items")), before, op)


def constants():
    return {"off_desktop_note": live.OFF_DESKTOP_NOTE,
            "deadline_seconds": float(OPERATION_DEADLINE_SECONDS),
            "settle_reasons": [reason.value for reason in live.SettleReason]}


def expected(r):
    f = r.get("func")
    if f == "connected_slugs":
        return connected_slugs(r)
    if f == "apply_answer":
        return apply_answer(r)
    if f == "result":
        return result(r)
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
        except Exception:  # noqa: BLE001 - the live ValueError/AttributeError/JSONDecodeError seam
            py = None
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
