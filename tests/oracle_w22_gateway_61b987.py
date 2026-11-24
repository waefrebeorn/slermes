#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/_store/gateway.py
(gate w22_gateway_61b987).

`list_gateway_sessions` is a listing reader: it drains queued async accounting
deltas, composes a session query from the ``platform`` / ``active_only``
switches, and maps rows through ``_session_row_dict``.  The store handle is the
DB seam, so a recorder holder stands in: it counts ``flush_for_read()`` calls and
captures the ``(query, params)`` handed to ``_read_all`` (which returns no rows,
so ``_session_row_dict`` -- core.py, a different C home -- is never reached).

The expected answer is the live statement verbatim plus the live parameter list
and the flush count, so the C port is judged on the exact SQL text, not on a
summary of the switches.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "persistence" / "sqlite" / "conversation" / "_store" / "gateway.py"
GATE = "w22_gateway_61b987"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("gateway_store_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["gateway_store_live"] = live
_spec.loader.exec_module(live)

WANTED = {"sessions_query"}


class RecordingHolder:
    """The store seam: flush is counted, the read records its arguments."""

    def __init__(self):
        self.flushes = 0
        self.calls = []

    def flush_for_read(self):
        self.flushes += 1

    def _read_all(self, query, params):
        self.calls.append((query, params))
        return []


def expected(r):
    holder = RecordingHolder()
    kwargs = {}
    if "platform" in r:
        kwargs["platform"] = r["platform"]
    if "active_only" in r:
        kwargs["active_only"] = r["active_only"]
    rows = live.list_gateway_sessions(holder, **kwargs)
    assert rows == [], rows
    assert len(holder.calls) == 1, holder.calls
    query, params = holder.calls[0]
    return {"sql": query, "params": params, "flushed": holder.flushes}


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
