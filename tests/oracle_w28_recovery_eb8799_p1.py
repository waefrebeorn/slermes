#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/admin/_recovery.py -- gate w28_recovery_eb8799_p1.

The live module is imported as a package module and its PURE decision functions are
called over fixture-supplied seam inputs:

  * ``_sidecar_path``          -- backup/restore path derivation
  * ``_format_bytes``          -- disk-space coercion
  * ``_quoted_columns``        -- statement construction (column list + placeholders)
  * ``_compatible_columns``    -- column-intersection predicate (PRAGMA seam injected)
  * ``_table_inventory``       -- inventory classification (columns/count seam injected)
  * ``_inspect_connection``    -- recoverability classification (conn seam injected)
  * ``_append_skipped_range``  -- range merging predicate
  * ``_state_meta_precheck``   -- state_meta terminal-status classification
  * ``_reconcile``             -- count-then-mutate statement construction

Every sqlite3 connection is a fake object: the SQL text the live code builds is
captured and compared byte-for-byte with what the C port builds.  ``--dump`` prints
the expected JSON instead of comparing, for fixture authoring.
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
GATE = "w28_recovery_eb8799_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite import admin as _admin_pkg  # noqa: E402
from persistence.sqlite.admin import _recovery as live  # noqa: E402

WANTED = {
    "sidecar_path", "format_bytes", "disk_space_preflight", "quoted_columns", "compatible_columns",
    "table_inventory", "inspect", "append_skipped_range", "state_meta_precheck", "reconcile",
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
    """Fake connection: records SQL, answers through ``handler``."""

    def __init__(self, handler=None):
        self.handler = handler or (lambda sql, params: Cursor([]))
        self.executed = []

    def execute(self, sql, params=()):
        self.executed.append(sql)
        return self.handler(sql, params)


def norm(path):
    """pathlib's separators are platform-local; the C port joins with '/'."""
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


def sidecar_path(req):
    return norm(live._sidecar_path(Path(req["path"]), req["suffix"]))


def format_bytes(req):
    return live._format_bytes(int(req["value"]))


def disk_space_preflight(req):
    bundle = int(req.get("bundle_bytes") or 0)
    work_free = int(req.get("work_free_bytes") or 0)
    output_free = int(req.get("output_free_bytes") or 0)
    work_dir = req.get("work_dir") or "work"
    output_dir = req.get("output_dir") or "out"

    class Usage:
        def __init__(self, free_bytes):
            self.free = free_bytes

    def disk_usage(path):
        return Usage(work_free if str(path) == work_dir else output_free)

    saved = live.shutil.disk_usage
    live.shutil.disk_usage = disk_usage
    try:
        output = Path(output_dir) if req.get("output_parent") else None
        with patched(_source_fingerprint=lambda _path: {"main": {"size": bundle, "mtime_ns": 0}},
                     _same_filesystem=lambda _a, _b: bool(req.get("same_filesystem", True))):
            return live._disk_space_preflight(Path("src.db"), Path(work_dir), output)
    except live.SessionRecoverySafetyError as exc:
        return {"error": str(exc)}
    finally:
        live.shutil.disk_usage = saved


def quoted_columns(req):
    quoted, placeholders = live._quoted_columns(list(req.get("columns") or []))
    return [quoted, placeholders]


def compatible_columns(req):
    src = list(req.get("source") or [])
    dst = list(req.get("destination") or [])
    calls = []

    def columns(conn, table):
        calls.append(table)
        return list(src) if len(calls) == 1 else list(dst)

    result = {}
    with patched(_table_columns=columns):
        out = live._compatible_columns(None, None, req.get("table") or "t", result)
    del out  # the live function reports through ``result``
    return result


def table_inventory(req):
    columns = req.get("columns", [])
    rows = req.get("rows")

    def get_columns(conn, table):
        if columns is None:
            raise sqlite3.DatabaseError(req.get("columns_error") or "database disk image is malformed")
        return list(columns)

    def count_rows(conn, table):
        if rows is None:
            raise sqlite3.DatabaseError(req.get("rows_error") or "database disk image is malformed")
        return int(rows)

    with patched(_table_columns=get_columns, _count_rows=count_rows):
        return live._table_inventory(None, req.get("table") or "t")


def inspect(req):
    journal_mode = req.get("journal_mode")
    journal_error = req.get("journal_error") or "database disk image is malformed"
    inventories = req.get("tables") or {}
    default = {"available": False, "columns": [], "rows": None}

    def handler(sql, params):
        if "journal_mode" in sql.lower():
            if journal_mode is None:
                raise sqlite3.DatabaseError(journal_error)
            return Cursor([[journal_mode]])
        return Cursor([])

    def inventory(conn, table):
        return json.loads(json.dumps(inventories.get(table, default)))

    conn = Conn(handler)
    with patched(_table_inventory=inventory):
        return live._inspect_connection(conn)


def append_skipped_range(req):
    ranges = [dict(item) for item in (req.get("ranges") or [])]
    live._append_skipped_range(ranges, int(req["low"]), int(req["high"]), req.get("error") or "")
    return ranges


def state_meta_precheck(req):
    src_cols = list(req.get("source_columns") or [])
    dst_cols = list(req.get("destination_columns") or [])
    source = Conn()
    destination = Conn()

    def columns(conn, table):
        return list(src_cols) if conn is source else list(dst_cols)

    rows = req.get("source_rows")
    with patched(_table_columns=columns):
        return live._state_meta_precheck(
            source, destination, None if rows is None else int(rows), salvage=bool(req.get("salvage")),
        )


def reconcile(req):
    count = int(req.get("count") or 0)

    def handler(sql, params):
        if "COUNT(*)" in sql:
            return Cursor([[count]])
        return Cursor([])

    conn = Conn(handler)
    used = live._reconcile(conn, req.get("table") or "t", req.get("where") or "", req.get("mutation") or "")
    return {"count": int(used), "statements": list(conn.executed)}


HANDLERS = {
    "sidecar_path": sidecar_path,
    "format_bytes": format_bytes,
    "disk_space_preflight": disk_space_preflight,
    "quoted_columns": quoted_columns,
    "compatible_columns": compatible_columns,
    "table_inventory": table_inventory,
    "inspect": inspect,
    "append_skipped_range": append_skipped_range,
    "state_meta_precheck": state_meta_precheck,
    "reconcile": reconcile,
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
