#!/usr/bin/env python3
"""Differential oracle for gate w29_sessions_2114c9_p2.

Truth: persistence/sqlite/conversation/sessions.py session_record and the
constant tables its markers/title vocabulary come from.  The row is a plain
dict, so the whole shaping runs with no store at all.
"""
import dataclasses
import enum
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_sessions_2114c9_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite.conversation import sessions as live  # noqa: E402
from persistence.core import conventions as conv  # noqa: E402

WANTED = {"session_record", "constants"}


def record(r):
    row = r.get("row")
    if not isinstance(row, dict):
        row = {}
    rec = live.session_record(row)
    out = {}
    for f in dataclasses.fields(rec):
        v = getattr(rec, f.name)
        if isinstance(v, enum.Enum):
            v = v.value
        out[f.name] = v
    return out


def constants():
    return {
        "markers": [[key, kind.value] for key, kind in live._MARKERS],
        "title_sources": list(live._TITLE_SOURCES),
        "canonical_title_locked": live._CANONICAL_TITLE_LOCKED,
        "prompt_hash_pattern": live._PROMPT_HASH.pattern,
        "untrusted_instant": live.UNTRUSTED_INSTANT.timestamp(),
        "epoch_min": conv._STORED_EPOCH_MIN,
        "epoch_max": conv._STORED_EPOCH_MAX,
    }


def expected(r):
    f = r.get("func")
    if f == "session_record":
        return record(r)
    if f == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (f,))


def norm(v):
    return json.loads(json.dumps(v, sort_keys=True))


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
            py = norm(expected(r))
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            if isinstance(py, dict) and isinstance(got, dict):
                diff = {k: (py.get(k), got.get(k)) for k in set(py) | set(got)
                        if py.get(k) != got.get(k)}
                print("MISMATCH %s case=%r fields=%r" % (r.get("func"), r.get("case"), diff),
                      file=sys.stderr)
            else:
                print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                      file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism or not cases else 0)


if __name__ == "__main__":
    main()
