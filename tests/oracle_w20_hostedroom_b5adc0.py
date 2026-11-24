#!/usr/bin/env python3
"""Differential oracle for gateway/hosted_rooms_legacy_import.py
(gate w20_hostedroom_b5adc0).

The live module is imported against stubbed persistence packages. Every compared
value comes from the real source: `source_path`, `settled` and
`_select_expressions` are called directly, and the table plan is read off the
live `_copy_rows` by recording the PRAGMA it issues for each table (fake
connections, monkeypatched sqlite3), so the ordering/skip rules are the
module's own.
"""
import importlib.util
import json
import os
import sys
import types
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

GATE = "w20_hostedroom_b5adc0"


def _mk(dotted, **attrs):
    parts = dotted.split(".")
    for i in range(1, len(parts) + 1):
        dn = ".".join(parts[:i])
        if dn not in sys.modules or sys.modules[dn] is None:
            m = types.ModuleType(dn)
            m.__path__ = []
            sys.modules[dn] = m
    m = sys.modules[dotted]
    for k, v in attrs.items():
        setattr(m, k, v)
    return m


_mk("gateway")
_mk("gateway.hosted_rooms_common", clock=lambda now: 0.0,
    table_columns=lambda conn, table: frozenset(), table_exists=lambda conn, table: False)
_mk("gateway.hosted_rooms", _LEGACY_COLUMNS=(), _EVENT_BYTES_BACKFILL="")

NAME = GATE + "_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "gateway", "hosted_rooms_legacy_import.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


class Cursor:
    def __init__(self, rows):
        self.rows = list(rows)

    def fetchone(self):
        return self.rows[0] if self.rows else None

    def __iter__(self):
        return iter(self.rows)


class FakeConn:
    """Just enough sqlite surface for _copy_rows / _select_expressions."""

    def __init__(self, names=None, log=None):
        self.names = names or []
        self.log = log if log is not None else []

    def execute(self, sql, params=()):
        if sql.startswith("PRAGMA table_info("):
            self.log.append(sql[len("PRAGMA table_info("):-1])
            return Cursor([])
        if sql.startswith("SELECT name FROM sqlite_master"):
            return Cursor([(n,) for n in self.names])
        if sql.startswith("SELECT sql FROM sqlite_master"):
            return Cursor([("CREATE TABLE t (x)",)])
        return Cursor([])

    def executemany(self, sql, rows):
        return None

    def close(self):
        return None


def live_plan(names):
    log = []
    live.table_exists = lambda conn, table: True
    live.table_columns = lambda conn, table: frozenset()
    live.sqlite3 = SimpleNamespace(connect=lambda *a, **k: FakeConn(names, log))
    live._copy_rows(FakeConn(), "C:/tmp/state.db")
    return log


def run_case(r):
    f = r.get("func")
    if f == "source_path":
        p = r.get("db_path")
        got = live.source_path(Path(p)) if isinstance(p, str) else None
        return None if got is None else str(got).replace("\\", "/")
    if f == "settled":
        live.table_exists = lambda conn, table: bool(r.get("table_exists"))
        live._failed_sources = ({Path(r["db_path"]).with_name("state.db")}
                                if r.get("source_failed") else set())
        conn = FakeConn()
        conn.execute = lambda sql, params=(): Cursor(
            [("1",)] if r.get("marker_row") else [])
        db_path = Path(r.get("db_path") or "")
        return bool(live.settled(conn, db_path))
    if f == "select_expressions":
        defaults = r.get("defaults") if isinstance(r.get("defaults"), list) else []
        legacy_rows = [(d[0], d[1], "", d[2]) for d in defaults]
        mod = sys.modules["gateway.hosted_rooms"]
        mod._LEGACY_COLUMNS = tuple(legacy_rows)
        live.table_columns = lambda conn, table: frozenset(r.get("target_cols") or [])
        conn = FakeConn()
        src = r.get("source_cols") or []
        conn.execute = lambda sql, params=(): Cursor([(i, c, "", 0, None, 0)
                                                     for i, c in enumerate(src)])
        pairs = live._select_expressions(conn, conn, r.get("name"))
        return [[a, b] for a, b in pairs]
    if f == "plan_tables":
        return live_plan(list(r.get("names") or []))
    if f == "table_included":
        name = r.get("name") or ""
        return not (name in live._SKIP_TABLES or name == live.MARKER_TABLE
                    or name.endswith(("_next", "_migrating")))
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
        exp = run_case(r)
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
print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
sys.exit(1 if mism else 0)
