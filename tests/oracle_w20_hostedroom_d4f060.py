#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/gateway/_hosted_rooms_legacy_import.py
(w20_hostedroom_d4f060).

The live module is imported with its import-time dependencies stubbed
(``session_rules.hosted_rooms.clock``, ``._hosted_rooms_common``), and each case
drives the real helpers: ``source_path`` with a ``PurePosixPath``, ``settled``
against a stubbed connection + ``table_exists``, and ``_select_expressions``
against stubbed sqlite connections and a stubbed ``_LEGACY_COLUMNS``.  The
INSERT traffic of ``_copy_rows`` and the SAVEPOINT handling of
``import_legacy_rooms`` need a live sqlite3 handle and stay upstream; the
filter/order decisions they make are compared here.
"""
import importlib.util
import json
import os
import sys
import types
from pathlib import PurePosixPath

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


for _pkg in ("persistence", "persistence.sqlite", "persistence.sqlite.gateway"):
    if _pkg not in sys.modules:
        _mk(_pkg)
_mk("session_rules")
_mk("session_rules.hosted_rooms", clock=lambda when=None: 0.0)
_mk("persistence.sqlite.gateway._hosted_rooms_common",
    table_columns=lambda conn, name: [],
    table_exists=lambda conn, name: False)
SCHEMA = _mk("persistence.sqlite.gateway._hosted_rooms_schema",
             _LEGACY_COLUMNS=[],
             _EVENT_BYTES_BACKFILL="UPDATE hosted_rooms SET event_bytes=0 WHERE {where}")

MODNAME = "persistence.sqlite.gateway._hosted_rooms_legacy_import"
sp = importlib.util.spec_from_file_location(
    MODNAME, os.path.join(Q, "persistence", "sqlite", "gateway",
                          "_hosted_rooms_legacy_import.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[MODNAME] = live
sp.loader.exec_module(live)


class StubRow:
    def __init__(self, row):
        self._row = row

    def fetchone(self):
        return self._row

    def __iter__(self):
        return iter(self._row or [])


class StubConn:
    """Just enough sqlite3.Connection for the helpers under test."""

    def __init__(self, rows):
        self.rows = rows

    def execute(self, sql, params=()):
        if isinstance(self.rows, dict):
            for key, value in self.rows.items():
                if key in sql:
                    return StubRow(value)
            return StubRow(None)
        return StubRow(self.rows)


def expected(r):
    f = r.get("func")
    if f == "source_path":
        p = r.get("db_path")
        if not isinstance(p, str):
            return None
        out = live.source_path(PurePosixPath(p))
        return None if out is None else str(out)

    if f == "settled":
        p = r.get("db_path")
        if not isinstance(p, str):
            return True
        live._failed_sources.clear()
        if r.get("in_failed"):
            # The module records the *source* it failed on, not the target path.
            failed = live.source_path(PurePosixPath(p))
            if failed is not None:
                live._failed_sources.add(failed)
        flag = bool(r.get("table_exists"))
        live.table_exists = lambda conn, name: flag
        conn = StubConn((1,) if r.get("marker_row") else None)
        return bool(live.settled(conn, PurePosixPath(p)))

    if f == "select_expressions":
        src = r.get("source_columns") or []
        tgt = r.get("target_columns") or []
        legacy = r.get("legacy_columns") or []
        SCHEMA._LEGACY_COLUMNS = [tuple(row) for row in legacy]
        live.table_columns = lambda conn, name: list(tgt)
        legacy_rows = [[0, col, "TEXT", 0] for col in src]
        return [list(pair) for pair in
                live._select_expressions(StubConn(legacy_rows), StubConn([]),
                                         r.get("name"))]

    if f == "table_skipped":
        name = r.get("name")
        if not isinstance(name, str):
            return False
        return bool(name in live._SKIP_TABLES or name == live.MARKER_TABLE
                    or name.endswith(("_next", "_migrating")))

    if f == "ordered_tables":
        names = r.get("names")
        if not isinstance(names, list):
            return []
        return sorted(names, key=lambda n: (n != "hosted_rooms", n))

    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_hostedroom_d4f060: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
