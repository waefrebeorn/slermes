#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/_store/messages.py
(gate w27_messages_87d5f2_p3: the display projection).

Seams, all read-only and injected:

  * ``_display_rows_from_conn`` / ``_fetch_conversation_rows`` run against a
    recording connection / holder, so the statement and binds are captured
    instead of executed;
  * ``get_messages`` runs with a holder whose ``_read_all`` returns the fixture
    rows and records the statement.  Only the single-segment, non-compacted
    branch is exercised (the compacted branches call ``_ensure_display_order``,
    a write).  Its argument-validation ValueErrors are compared too.

SQL text is whitespace-normalized on both sides; tokens, order and binds are
exact.  ``_fold_summary_carrier`` returns the mutated ``first_by_uid`` map
alongside the key so the fold's state is compared, not just its answer.
"""
import json
import re
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_messages_87d5f2_p3"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite.conversation._store import messages as live  # noqa: E402

WANTED = {
    "is_model_only_row",
    "fold_summary_carrier",
    "row_to_message_dict",
    "display_rows_from_conn",
    "active_clause",
    "fetch_conversation_rows",
    "get_messages",
}


def norm(sql):
    """Whitespace-normalized SQL: runs collapsed, and the space a wrapped
    parenthesis leaves next to "(" / ")" removed, so an indented f-string and a
    single-line builder compare on tokens only."""
    if not isinstance(sql, str):
        return sql
    text = " ".join(sql.split())
    text = re.sub(r"\(\s+", "(", text)
    text = re.sub(r"\s+\)", ")", text)
    return text


class RecordingCursor:
    def __init__(self, rows):
        self._rows = rows

    def fetchall(self):
        return self._rows


class RecordingConn:
    def __init__(self):
        self.calls = []

    def execute(self, sql, params=()):
        self.calls.append((sql, tuple(params)))
        return RecordingCursor([])


class RecordingHolder:
    def __init__(self, rows=None):
        self.rows = rows if rows is not None else []
        self.sql = None
        self.params = None

    def _read_all(self, sql, params):
        self.sql = sql
        self.params = list(params)
        return list(self.rows)


def display_rows(req):
    conn = RecordingConn()
    live._display_rows_from_conn(conn, req.get("session_id"), limit=req.get("limit"),
                                 offset=req.get("offset") or 0, latest=bool(req.get("latest")),
                                 after=req.get("after"))
    sql, params = conn.calls[-1]
    return {"sql": norm(sql), "params": list(params)}


def fetch_rows(req):
    holder = RecordingHolder()
    ids = req.get("session_ids")
    ids = ids if isinstance(ids, list) else []
    live._fetch_conversation_rows(holder, ids, req.get("active_clause") or "",
                                  with_session_id=bool(req.get("with_session_id")))
    return {"sql": norm(holder.sql), "params": list(holder.params)}


def get_messages(req):
    holder = RecordingHolder(req.get("rows") or [])
    try:
        out = live.get_messages(holder, req.get("session_id"),
                                include_inactive=bool(req.get("include_inactive")),
                                include_compacted=bool(req.get("include_compacted")),
                                limit=req.get("limit"), offset=req.get("offset") or 0,
                                latest=bool(req.get("latest")), after_id=req.get("after_id"),
                                include_ancestors=bool(req.get("include_ancestors")))
    except ValueError as exc:
        return {"error": str(exc)}
    return {"messages": out, "sql": norm(holder.sql), "params": list(holder.params)}


def expected(r):
    f = r.get("func")
    if f == "is_model_only_row":
        return live._is_model_only_row(None, r.get("row"))
    if f == "fold_summary_carrier":
        first = dict(r.get("first_by_uid") or {})
        key = live._fold_summary_carrier(r.get("row"), r.get("key"), first)
        return {"key": key, "first_by_uid": first}
    if f == "row_to_message_dict":
        return live._row_to_message_dict(None, r.get("row"), warn_context="oracle",
                                         summary_flag=bool(r.get("summary_flag")))
    if f == "display_rows_from_conn":
        return display_rows(r)
    if f == "active_clause":
        return live._active_clause(bool(r.get("include_inactive")), bool(r.get("include_compacted")))
    if f == "fetch_conversation_rows":
        return fetch_rows(r)
    if f == "get_messages":
        return get_messages(r)
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
        if isinstance(got, dict) and "sql" in got:
            got = dict(got, sql=norm(got["sql"]))
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
