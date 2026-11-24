#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/_store/messages.py
(gate w27_messages_87d5f2_p2: row identity, kept prefix, clone statement).

Every function compared here is called with values only:

  * ``_row_identity`` — the four columns a kept-prefix match compares;
  * ``_loaded_view_content`` — the loader lens (sanitize_context + strip);
  * ``_stamp_kept_live_prefix`` — driven with the six live-row columns as
    plain tuples, exactly what ``_LIVE_IDENTITY_SQL`` returns;
  * ``_clone_message_rows`` — driven through a fake holder whose
    ``_message_columns_cache`` is the PRAGMA answer and a recording connection,
    so the statement text and its binds are captured instead of run.

The Python tuple returned by ``_row_identity`` is compared as a list (a JSON
array has no tuple type); element order and values are exact.  SQL text is
whitespace-normalized on both sides.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_messages_87d5f2_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite.conversation._store import messages as live  # noqa: E402

WANTED = {"row_identity", "loaded_view_content", "stamp_kept_live_prefix", "clone_message_rows"}


def norm(sql):
    return " ".join(sql.split()) if isinstance(sql, str) else sql


def norm_out(value):
    if isinstance(value, dict) and "sql" in value and "params" in value:
        return dict(value, sql=norm(value["sql"]))
    return value


class RecordingCursor:
    def __init__(self, rows):
        self._rows = rows

    def fetchall(self):
        return self._rows


class RecordingConn:
    def __init__(self, pragma_rows):
        self.pragma_rows = pragma_rows
        self.calls = []

    def execute(self, sql, params=()):
        self.calls.append((sql, tuple(params)))
        if sql.startswith("PRAGMA table_info"):
            return RecordingCursor(self.pragma_rows)
        return RecordingCursor([])


class FakeHolder:
    """The seam: the column cache _message_column_names fills from PRAGMA."""

    def __init__(self):
        self._message_columns_cache = None


def clone(req):
    columns = req.get("columns")
    columns = columns if isinstance(columns, list) else []
    holder = FakeHolder()
    conn = RecordingConn([(0, c, "TEXT", 0, None, 0) for c in columns])
    tail_ids = req.get("tail_ids")
    tail_ids = tail_ids if isinstance(tail_ids, list) else []
    live._clone_message_rows(holder, conn, tail_ids, session_id=req.get("session_id"))
    sql, params = conn.calls[-1]
    return {"sql": norm(sql), "params": list(params)}


def stamp(req):
    raw = req.get("live")
    rows = []
    for row in raw if isinstance(raw, list) else []:
        if isinstance(row, (list, tuple)) and len(row) >= 6:
            rows.append(tuple(row[:6]))
    messages = []
    for msg in req.get("messages") or []:
        messages.append(dict(msg) if isinstance(msg, dict) else msg)
    kept = live._stamp_kept_live_prefix(None, rows, messages)
    return {"kept": kept, "messages": messages}


def expected(r):
    f = r.get("func")
    if f == "row_identity":
        return list(live._row_identity(r.get("role"), r.get("content"), r.get("tool_call_id"),
                                       r.get("tool_calls")))
    if f == "loaded_view_content":
        return live._loaded_view_content(r.get("role"), r.get("content"))
    if f == "stamp_kept_live_prefix":
        return stamp(r)
    if f == "clone_message_rows":
        return clone(r)
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
