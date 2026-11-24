#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/_schema.py (gate w27_schema_3eeff1_p3).

The degradation/capability layer, tested where it is decided:

  * ``_is_trigram_unavailable_error`` / ``_is_fts5_unavailable_error`` — the
    exact SQLite error strings that mean "this runtime cannot serve the index";
  * ``_db_has_legacy_inline_fts`` / ``_db_has_trigram_tool_calls_projection`` —
    predicates over the stored CREATE text (a stand-in cursor returns it);
  * ``_title_rank`` — how much authority a stored title_source carries;
  * ``_has_fts_trash`` — against a real in-memory sqlite_master, so the
    escaped-LIKE prefix (``fts\\_v22\\_trash\\_%`` ESCAPE '\\') is the live query;
  * ``next_title_in_lineage`` — against a real in-memory ``sessions`` table, so
    the " #N" suffix regex, the LIKE lookup and the numbering are the live ones.
"""
import json
import sqlite3
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_schema_3eeff1_p3"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite import _schema as live  # noqa: E402

WANTED = {
    "is_trigram_unavailable_error", "is_fts5_unavailable_error",
    "db_has_legacy_inline_fts", "db_has_trigram_tool_calls_projection",
    "title_rank", "has_fts_trash", "next_title_in_lineage",
    "retired_cjk_index_present", "constants",
}


class StoredSqlCursor:
    """Stand-in for the one sqlite_master point-read those predicates make."""

    def __init__(self, sql):
        self.sql = sql

    def execute(self, statement, params=()):
        return self

    def fetchone(self):
        return None if self.sql is None else (self.sql,)


def has_fts_trash(names):
    conn = sqlite3.connect(":memory:")
    try:
        for idx, name in enumerate(names or []):
            conn.execute('CREATE TABLE "%s" (x INTEGER)' % str(name).replace('"', '""'))
            del idx
        return live._has_fts_trash(None, conn)
    finally:
        conn.close()


def retired_cjk_index_present(names):
    conn = sqlite3.connect(":memory:")
    try:
        for name in names or []:
            conn.execute('CREATE TABLE "%s" (x INTEGER)' % str(name).replace('"', '""'))
        return live.retired_cjk_index_present(conn)
    finally:
        conn.close()


def next_title_in_lineage(base, titles):
    conn = sqlite3.connect(":memory:")
    try:
        conn.execute("CREATE TABLE sessions (title TEXT)")
        for title in titles or []:
            conn.execute("INSERT INTO sessions (title) VALUES (?)", (title,))
        return live.next_title_in_lineage(conn, base)
    finally:
        conn.close()


def expected(r):
    f = r.get("func")
    if f == "is_trigram_unavailable_error":
        return live._is_trigram_unavailable_error(sqlite3.OperationalError(r.get("message") or ""))
    if f == "is_fts5_unavailable_error":
        return live._is_fts5_unavailable_error(sqlite3.OperationalError(r.get("message") or ""))
    if f == "db_has_legacy_inline_fts":
        return live._db_has_legacy_inline_fts(StoredSqlCursor(r.get("table_sql")))
    if f == "db_has_trigram_tool_calls_projection":
        return live._db_has_trigram_tool_calls_projection(StoredSqlCursor(r.get("table_sql")))
    if f == "title_rank":
        return live._title_rank(r.get("source"))
    if f == "has_fts_trash":
        return has_fts_trash(r.get("tables"))
    if f == "retired_cjk_index_present":
        return retired_cjk_index_present(r.get("names"))
    if f == "next_title_in_lineage":
        return next_title_in_lineage(r.get("base") or "", r.get("titles"))
    if f == "constants":
        return {
            "title_source_derived": live.TITLE_SOURCE_DERIVED,
            "title_source_llm": live.TITLE_SOURCE_LLM,
            "title_source_user": live.TITLE_SOURCE_USER,
            "retired_cjk_objects": list(live._RETIRED_CJK_OBJECTS),
        }
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = total = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        total += 1
        try:
            r = json.loads(line)
        except ValueError:
            print("BAD JSON line %d" % total, file=sys.stderr)
            mism += 1
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            print("UNHANDLED %r" % (r.get("func") if isinstance(r, dict) else r,), file=sys.stderr)
            mism += 1
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
    if total != cases:
        print("ORACLE %s: %d fixture lines, only %d cases compared" % (GATE, total, cases),
              file=sys.stderr)
        mism += 1
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
