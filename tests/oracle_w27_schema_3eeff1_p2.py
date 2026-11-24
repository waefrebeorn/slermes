#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/_schema.py (gate w27_schema_3eeff1_p2).

Statement sequences are compared, not executed: the live builders are handed a
recording cursor (and, for ``_rebuild_table``, a connection stub whose
``in_transaction`` decides transaction ownership), so what is compared is
exactly the SQL text and order a real cursor would have received.

  * ``_rebuild_table_statements`` / ``_rebuild_table`` — RENAME/CREATE/COPY/
    DROP/indexes, and whether BEGIN IMMEDIATE .. COMMIT (or ROLLBACK) wraps it;
  * ``_rebuild_fts_indexes`` — external-content 'rebuild' vs the legacy DELETE
    + reinsert, and the deferred-marker clear;
  * ``_drop_fts_triggers`` — the six trigger names, in DDL order;
  * ``_fts_triggers_missing`` — the count comparison that gates repair;
  * ``_live_pk_columns`` — PRIMARY KEY names in key order from PRAGMA rows;
  * ``_fts_index_is_misaligned_source`` — stored CREATE vs the aligned view.
"""
import json
import sqlite3
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_schema_3eeff1_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite import _schema as live  # noqa: E402

WANTED = {
    "misaligned_source", "live_pk_columns", "rebuild_table_statements",
    "rebuild_table", "rebuild_fts_indexes", "drop_fts_triggers", "fts_triggers_missing",
}


class RecordingCursor:
    def __init__(self, in_transaction=False, fail_at=-1, rows=None, count=None):
        self.calls = []
        self.fail_at = fail_at
        self.rows = rows
        self.count = count
        self.connection = types.SimpleNamespace(in_transaction=in_transaction)

    def execute(self, sql, params=()):
        self.calls.append(sql)
        if self.fail_at >= 0 and len(self.calls) - 1 == self.fail_at:
            raise sqlite3.OperationalError("boom")
        return self

    def fetchall(self):
        if self.rows is None:
            raise sqlite3.OperationalError("no such table")
        return self.rows

    def fetchone(self):
        last = self.calls[-1] if self.calls else ""
        if last.startswith("SELECT COUNT(*)"):
            return (self.count,)
        if self.rows is None:
            return None
        return self.rows[0] if self.rows else None


def rebuild_table(r):
    in_txn = bool(r.get("in_transaction"))
    fail_at = int(r.get("fail_at", -1))
    # fail_at indexes the RENAME/CREATE/COPY/DROP/indexes sequence; an owned
    # transaction's BEGIN IMMEDIATE is call 0 and is never the failing one.
    cur = RecordingCursor(in_transaction=in_txn,
                          fail_at=-1 if fail_at < 0 else fail_at + (0 if in_txn else 1))
    try:
        live._rebuild_table(cur, r.get("table"), r.get("legacy_name"), r.get("ddl"),
                            r.get("copy_sql"), r.get("indexes") or ())
    except Exception as exc:  # noqa: BLE001
        return {"statements": cur.calls, "error": type(exc).__name__, "message": str(exc)}
    return {"statements": cur.calls}


def misaligned_source(r):
    cur = RecordingCursor(rows=[] if r.get("table_sql") is None else [(r.get("table_sql"),)])
    return live._fts_index_is_misaligned_source(cur)


def expected(r):
    f = r.get("func")
    if f == "misaligned_source":
        return misaligned_source(r)
    if f == "live_pk_columns":
        rows = r.get("rows")
        cur = RecordingCursor(rows=rows)
        if r.get("missing"):
            cur.rows = None
        return live._live_pk_columns(cur, r.get("table"))
    if f == "rebuild_table_statements":
        cur = RecordingCursor()
        live._rebuild_table_statements(cur, r.get("table"), r.get("legacy_name"), r.get("ddl"),
                                       r.get("copy_sql"), r.get("indexes") or ())
        return cur.calls
    if f == "rebuild_table":
        return rebuild_table(r)
    if f == "rebuild_fts_indexes":
        cur = RecordingCursor()
        live._rebuild_fts_indexes(cur, legacy=bool(r.get("legacy")),
                                  include_trigram=bool(r.get("include_trigram", True)))
        return cur.calls
    if f == "drop_fts_triggers":
        cur = RecordingCursor()
        live._drop_fts_triggers(cur)
        return cur.calls
    if f == "fts_triggers_missing":
        cur = RecordingCursor(count=int(r.get("found")))
        return live._fts_triggers_missing(cur, r.get("names") or ())
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
