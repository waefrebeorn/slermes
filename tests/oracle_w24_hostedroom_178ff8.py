#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/gateway/_hosted_rooms_common.py
(gate w24_hostedroom_178ff8).

The live helpers take a ``sqlite3.Connection``; here that connection is a tiny
in-memory object that returns the rows the case supplies.  No database is
opened, no statement is compiled: the seam is exactly ``conn.execute(...)``,
and what is compared is the decision each helper makes over those values --
the CAS rowcount verdict, the epoch fence, the catalog existence probe and the
PRAGMA column set.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_hostedroom_178ff8"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite.gateway import _hosted_rooms_common as live  # noqa: E402

WANTED = {"fenced_update", "epoch", "table_exists", "table_columns", "constants"}


class _Cursor:
    def __init__(self, rows, rowcount):
        self._rows = list(rows)
        self.rowcount = rowcount

    def fetchone(self):
        return self._rows[0] if self._rows else None

    def __iter__(self):
        return iter(self._rows)


class FakeConn:
    """Only ``execute`` is ever used by the ported helpers."""

    def __init__(self, rows=(), rowcount=0):
        self.rows = list(rows)
        self.rowcount = rowcount

    def execute(self, sql, params=()):  # noqa: ARG002 - the SQL is not compiled
        return _Cursor(self.rows, self.rowcount)


class Fence(Exception):
    """The caller's error object, raised by fenced_update when the fence is lost."""


def fenced_update(req):
    try:
        live.fenced_update(FakeConn(rowcount=req.get("rowcount")), "UPDATE t SET a=1 WHERE b=2", (), Fence)
    except Fence:
        return {"ok": False}
    return {"ok": True}


def _typed(value, kind):
    if kind == "null":
        return None
    if kind == "str":
        return str(value)
    if kind == "bool":
        return bool(value)
    if kind == "float":
        return float(value)
    if kind == "int":
        return int(value)
    return value


def epoch(req):
    value = _typed(req.get("epoch"), req.get("epoch_type") or "int")
    row = (_typed(req.get("row_epoch"), req.get("epoch_type") or "int"),) if req.get("row_present") else None
    conn = FakeConn(rows=[row] if row is not None else [])
    try:
        live._epoch(conn, value)
    except live.FenceStale as exc:
        return {"ok": False, "reason": exc.reason, "entity": exc.entity, "entity_id": exc.entity_id}
    return {"ok": True}


def table_exists(req):
    return bool(live.table_exists(FakeConn(rows=[(1,)] if req.get("row_present") else []), "hosted_rooms"))


def table_columns(req):
    rows = req.get("rows")
    rows = rows if isinstance(rows, list) else []
    try:
        return sorted(live.table_columns(FakeConn(rows=rows), "hosted_rooms"))
    except IndexError:
        return {"error": "index"}
    except TypeError:
        return {"error": "type"}


def constants():
    return {
        "stale_fence_kinds": [kind.value for kind in live.StaleFenceKind],
        "epoch_reason": live.StaleFenceKind.STALE_EPOCH.value,
    }


def expected(r):
    f = r.get("func")
    if f == "fenced_update":
        return fenced_update(r)
    if f == "epoch":
        return epoch(r)
    if f == "table_exists":
        return table_exists(r)
    if f == "table_columns":
        return table_columns(r)
    if f == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (f,))


def eq(a, b):
    """Python equality: absent key == null == None, True == 1, 1 == 1.0."""
    if isinstance(a, bool) or isinstance(b, bool):
        return isinstance(a, (bool, int, float)) == isinstance(b, (bool, int, float)) and a == b
    if isinstance(a, dict) and isinstance(b, dict):
        return set(a) == set(b) and all(eq(a[k], b[k]) for k in a)
    if isinstance(a, list) and isinstance(b, list):
        return len(a) == len(b) and all(eq(x, y) for x, y in zip(a, b))
    if a is None or b is None:
        return a is b
    return a == b


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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if not eq(py, got):
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
