#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/_store/messages.py
(gate w27_messages_87d5f2_p1: the payload codecs + the counter statement).

The live module is imported as a package module; every function compared here
is called with plain values — no StoreHolder, no connection, no clock:

  * ``_encode_display_metadata`` / ``_decode_display_metadata`` (the TEXT
    column codec, including the legacy double-encoded layer),
  * ``_reasoning_json_text`` (strings verbatim, structures dumped),
  * ``_decoded_repair_row`` (one durable row -> the live dict, no dedupe),
  * ``_bump_session_counters`` driven through a recording fake connection,
  * ``_reaction_list`` (display metadata -> the dict-only reactions).

SQL text is whitespace-normalized on both sides (the C builder emits one line;
the f-strings upstream carry indentation) — the statement's tokens, order and
binds are compared exactly.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_messages_87d5f2_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite.conversation._store import messages as live  # noqa: E402

WANTED = {
    "encode_display_metadata",
    "decode_display_metadata",
    "reasoning_json_text",
    "decoded_repair_row",
    "bump_session_counters",
    "reaction_list",
}


def norm(sql):
    return " ".join(sql.split()) if isinstance(sql, str) else sql


def norm_out(value):
    if isinstance(value, dict) and "sql" in value and "params" in value:
        return dict(value, sql=norm(value["sql"]))
    return value


class RecordingConn:
    """The only seam: conn.execute, recorded instead of run."""

    def __init__(self):
        self.calls = []

    def execute(self, sql, params=()):
        self.calls.append((sql, tuple(params)))
        return self


def expected(r):
    f = r.get("func")
    if f == "encode_display_metadata":
        return live._encode_display_metadata(r.get("display_metadata"))
    if f == "decode_display_metadata":
        return live._decode_display_metadata(r.get("raw"))
    if f == "reasoning_json_text":
        return live._reasoning_json_text(r.get("value"))
    if f == "decoded_repair_row":
        row = r.get("row")
        if not isinstance(row, dict):
            raise AssertionError("row must be an object")
        return live._decoded_repair_row(None, row)
    if f == "bump_session_counters":
        conn = RecordingConn()
        live._bump_session_counters(conn, r.get("session_id"), r.get("inserted"),
                                    r.get("tool_calls"), unit=bool(r.get("unit")))
        if not conn.calls:
            return {"sql": None, "params": []}
        sql, params = conn.calls[-1]
        return {"sql": norm(sql), "params": list(params)}
    if f == "reaction_list":
        return live._reaction_list(None, r.get("meta"))
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
        got = norm_out(r.get("out"))
        if norm_out(py) != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
