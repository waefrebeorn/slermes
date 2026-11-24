#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/_store/session_list.py
(gate w21_sessionlis_08fd81).

The live module is imported as-is; the two DB reads it makes are seams and are
replaced per case with the fixture's `chains` / `tip_rows` maps -- the same
values the Python function holds after its batched query:

    live.get_compression_chain(db, sid)          -> chains.get(sid, [])
    live._get_session_rich_rows_batch(db, ids, compact_rows=...) -> subset(tip_rows)

`project_compression_tips` itself (which roots rotate, the field override set,
the title carry-back, `_lineage_*`, `continuation_kind`) is executed live.
"""
import importlib
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w21_sessionlis_08fd81"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

live = importlib.import_module("persistence.sqlite.conversation._store.session_list")


def expected(r):
    chains = r.get("chains")
    tip_rows = r.get("tip_rows")
    chains = chains if isinstance(chains, dict) else {}
    tip_rows = tip_rows if isinstance(tip_rows, dict) else {}
    live.get_compression_chain = lambda _db, sid: chains.get(sid, [])
    live._get_session_rich_rows_batch = (
        lambda _db, ids, compact_rows=False: {k: v for k, v in tip_rows.items() if k in ids}
    )
    try:
        return live.project_compression_tips(None, r.get("sessions"), bool(r.get("compact_rows", False)))
    except Exception as exc:  # noqa: BLE001 - reported as the harness shapes it
        return {"status": "error", "type": type(exc).__name__, "message": str(exc)}


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
        if not isinstance(r, dict) or r.get("func") not in ("project", "tips"):
            continue
        cases += 1
        py = expected(r)
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH case=%r\n  py=%r\n   c=%r" % (r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
