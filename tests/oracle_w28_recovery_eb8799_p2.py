#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/admin/_recovery.py -- gate w28_recovery_eb8799_p2.

Salvage geometry and verification classification of the recovery lane, driven over
fixture-supplied seam inputs:

  * ``_probe_populated_edge``    -- galloping finite bound for a damaged rowid edge
  * ``_salvage_rowid_bounds``    -- readable-edge discovery + fallback/aggregate policy
  * ``_repair_out_of_window_timestamps`` -- epoch-window coercion + statement text
  * ``_verify_structure``        -- integrity/FK/schema/FTS-meta classification
  * ``_verify_row_counts``       -- count shortfall vs. tolerated-loss classification
  * ``_lost_and_found_plausibility_errors`` -- systematic mis-mapping tell
  * ``_recovery_report``         -- verdict header (complete/partial/verified)

The sqlite3 connection is a fake object fed by the fixture: a table is a rowid set
and an optional damaged candidate interval, and every SQL string the live code
builds is captured so the C port's text can be compared byte-for-byte.
"""
import json
import sqlite3
import sys
from contextlib import contextmanager
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_recovery_eb8799_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite.admin import _recovery as live  # noqa: E402

WANTED = {
    "probe_edge", "salvage_bounds", "repair_plan", "verify_structure", "verify_row_counts",
    "plausibility", "recovery_report",
}

DUMP = "--dump" in sys.argv


class Cursor:
    def __init__(self, rows=None):
        self._rows = list(rows or [])

    def fetchone(self):
        return self._rows[0] if self._rows else None

    def fetchall(self):
        return list(self._rows)

    def fetchmany(self, size):
        head, self._rows = self._rows[:size], self._rows[size:]
        return head


class Conn:
    def __init__(self, handler=None):
        self.handler = handler or (lambda sql, params: Cursor([]))
        self.executed = []

    def execute(self, sql, params=()):
        self.executed.append(sql)
        return self.handler(sql, params)


def norm(path):
    return str(path).replace("\\", "/")


@contextmanager
def patched(**attrs):
    saved = {name: getattr(live, name) for name in attrs}
    try:
        for name, value in attrs.items():
            setattr(live, name, value)
        yield
    finally:
        for name, value in saved.items():
            setattr(live, name, value)


def damaged(candidate, req):
    for low, high in req.get("damage") or []:
        if int(low) <= candidate <= int(high):
            return True
    return False


def probe_source(req):
    """A rowid set plus damaged candidate intervals; answers the probe SQL."""
    occupied = sorted(int(v) for v in (req.get("occupied") or []))
    error = req.get("error") or "database disk image is malformed"

    def handler(sql, params):
        flat = " ".join(sql.split())
        candidate = params[0]
        if damaged(candidate, req):
            raise sqlite3.DatabaseError(error)
        if "rowid > ?" in flat:
            above = [rowid for rowid in occupied if rowid > candidate]
            return Cursor([[above[0]]]) if above else Cursor([])
        below = [rowid for rowid in occupied if rowid < candidate]
        return Cursor([[below[-1]]]) if below else Cursor([])

    return Conn(handler)


def probe_edge(req):
    source = probe_source(req)
    return live._probe_populated_edge(
        source, req.get("table") or "messages", edge=req.get("edge") or "high",
        anchor=int(req.get("anchor") or 0),
    )


def bounds_source(req):
    occupied = sorted(int(v) for v in (req.get("occupied") or []))
    error = req.get("error") or "database disk image is malformed"
    probe = probe_source(req)

    def handler(sql, params):
        flat = " ".join(sql.split())
        if "min(rowid), max(rowid)" in flat:
            if not req.get("aggregate_ok", True):
                raise sqlite3.DatabaseError(error)
            if not occupied:
                return Cursor([[None, None]])
            return Cursor([[occupied[0], occupied[-1]]])
        if "?" in flat:
            return probe.handler(sql, params)
        if "ASC" in flat:
            if not req.get("low_ok", True):
                raise sqlite3.DatabaseError(error)
            return Cursor([[occupied[0]]]) if occupied else Cursor([])
        if not req.get("high_ok", True):
            raise sqlite3.DatabaseError(error)
        return Cursor([[occupied[-1]]]) if occupied else Cursor([])

    return Conn(handler)


def salvage_bounds(req):
    return live._salvage_rowid_bounds(bounds_source(req), req.get("table") or "messages")


def repair_plan(req):
    message_columns = list(req.get("messages_columns") or [])
    session_columns = list(req.get("sessions_columns") or [])
    repairs = int(req.get("repair_count") or 0)

    def columns(conn, table):
        return list(message_columns) if table == "messages" else list(session_columns)

    def handler(sql, params):
        if "COUNT(*)" in sql:
            return Cursor([[repairs]])
        return Cursor([])

    conn = Conn(handler)
    with patched(_table_columns=columns, _immediate_transaction=lambda _conn: _nullcontext()):
        repaired = live._repair_out_of_window_timestamps(conn)
    return {"repaired": int(repaired), "statements": list(conn.executed)}


@contextmanager
def _nullcontext():
    yield


def verify_structure(req):
    fts_meta = dict(req.get("fts_meta") or {})
    journal_mode = req.get("journal_mode") or "wal"
    schema_version = req.get("schema_version")

    def handler(sql, params):
        flat = " ".join(sql.split())
        if flat.startswith("PRAGMA integrity_check"):
            return Cursor([[str(value)] for value in (req.get("integrity_rows") or [])])
        if flat.startswith("PRAGMA foreign_key_check"):
            return Cursor([list(row) for row in (req.get("foreign_key_rows") or [])])
        if flat.startswith("PRAGMA journal_mode"):
            return Cursor([[journal_mode]])
        if flat.startswith("SELECT version FROM schema_version"):
            return Cursor([]) if schema_version is None else Cursor([[schema_version]])
        if flat.startswith("SELECT key, value FROM state_meta"):
            return Cursor([[key, value] for key, value in fts_meta.items()])
        return Cursor([])

    verification = {"errors": [], "warnings": []}
    live._verify_structure(Conn(handler), verification)
    return verification


def verify_row_counts(req):
    present = set(req.get("present_tables") or [])
    counts = dict(req.get("counts") or {})
    expected = dict(req.get("expected_counts") or {})

    def columns(conn, table):
        return ["id"] if table in present else []

    def count_rows(conn, table):
        return int(counts.get(table, 0))

    verification = {"errors": [], "warnings": [], "loss_detected": False}
    with patched(_table_columns=columns, _count_rows=count_rows):
        live._verify_row_counts(
            Conn(), verification, expected_counts=expected, copy_report=req.get("copy_report") or {},
            allow_partial=bool(req.get("allow_partial")),
            orphan_cleanup=req.get("orphan_cleanup"),
        )
    return verification


def plausibility(req):
    sessions = req.get("sessions") or {}
    messages = req.get("messages") or {}

    def handler(sql, params):
        flat = " ".join(sql.split())
        bucket = sessions if "FROM sessions" in flat else messages
        if "IS NULL" in flat:
            return Cursor([[int(bucket.get("implausible") or 0)]])
        return Cursor([[int(bucket.get("total") or 0)]])

    return live._lost_and_found_plausibility_errors(Conn(handler))


def recovery_report(req):
    inspection = json.loads(json.dumps(req.get("inspection") or {}))
    verification = json.loads(json.dumps(req.get("verification") or {}))
    unchanged = bool(req.get("source_unchanged", True))
    fingerprint = inspection.get("source_fingerprint") or {}
    other = {"main": {"size": 1, "mtime_ns": 2}}

    with patched(_source_fingerprint=lambda _path: dict(fingerprint) if unchanged else dict(other)):
        return live._recovery_report(
            Path(req.get("source") or "state.db"), Path(req.get("output") or "recovered.db"),
            inspection, req.get("disk_space") or {}, verification,
            on_source_change=req.get("on_source_change") or "healthy",
            **(req.get("fields") or {}),
        )


HANDLERS = {
    "probe_edge": probe_edge,
    "salvage_bounds": salvage_bounds,
    "repair_plan": repair_plan,
    "verify_structure": verify_structure,
    "verify_row_counts": verify_row_counts,
    "plausibility": plausibility,
    "recovery_report": recovery_report,
}


def expected(req):
    func = req.get("func")
    if func not in HANDLERS:
        raise AssertionError("unknown func %r" % (func,))
    return HANDLERS[func](req)


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except ValueError:
            continue
        if not isinstance(req, dict) or req.get("func") not in WANTED:
            continue
        cases += 1
        try:
            want = expected(req)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (req.get("func"), req.get("case"), exc),
                  file=sys.stderr)
            continue
        if DUMP:
            print(json.dumps(want, sort_keys=True))
            continue
        got = req.get("out")
        if want != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (req.get("func"), req.get("case"), want, got),
                  file=sys.stderr)
    if DUMP:
        return
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
