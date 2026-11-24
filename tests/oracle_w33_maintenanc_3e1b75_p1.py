#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/housekeeping/maintenance.py
builders (gate w33_maintenanc_3e1b75_p1).

The live ``SQLiteSessionMaintenance`` is driven through a recording fake
holder/store: the SQLite connection, the writer transaction and the clock never
start, so what is compared is exactly the SQL bytes, the bound-parameter order
and the plans those methods produce.
"""
import contextlib
import json
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w33_maintenanc_3e1b75_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
sys.path.append(str(Path(__file__).resolve().parent))

from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

from persistence.sqlite.housekeeping import maintenance as live  # noqa: E402

WANTED = {
    "escape_like", "like", "placeholders", "id_chunks", "cwd_prefix_clause",
    "last_active_sql", "continuation_edge", "continued_ancestors_sql",
    "pinned_tail_sql", "not_pinned_sql", "row_bytes_sql", "projection_fields",
    "sweep_scope", "idle_archive_sql",
}


class Cur:
    def __init__(self, rows):
        self.rows = rows

    def fetchall(self):
        return self.rows

    def fetchone(self):
        return self.rows[0] if self.rows else None

    def __iter__(self):
        return iter(self.rows)


class Conn:
    def __init__(self, holder):
        self.h = holder
        self.sql = []

    def execute(self, sql, params=()):
        self.sql.append((sql, list(params)))
        return Cur(self.h.next_rows(sql, params))

    def create_function(self, *args, **kwargs):
        pass


class Holder:
    """Records every statement and answers from a scripted row list."""

    def __init__(self, rows=None):
        self.sql = []
        self.resp = list(rows or [])

    def next_rows(self, sql, params):
        return self.resp.pop(0) if self.resp else []

    def _read_all(self, sql, params):
        self.sql.append((sql, list(params)))
        return self.next_rows(sql, params)

    def _read_one(self, sql, params):
        self.sql.append((sql, list(params)))
        rows = self.next_rows(sql, params)
        return rows[0] if rows else None

    def _execute_write(self, fn):
        conn = Conn(self)
        out = fn(conn)
        self.sql.extend(conn.sql)
        return out

    def _write_sql(self, sql, params=()):
        self.sql.append((sql, list(params)))

    def _write_rowcount(self, sql, params=()):
        self.sql.append((sql, list(params)))
        return 1

    @contextlib.contextmanager
    def _read_ctx_with_timeout(self, timeout):
        conn = Conn(self)
        yield conn
        self.sql.extend(conn.sql)


class Store:
    def __init__(self, holder):
        self.h = holder

    def holder(self):
        return self.h


def _repo(rows=None):
    holder = Holder(rows)
    return live.SQLiteSessionMaintenance(Store(holder)), holder


def _at(now):
    """Pin the module clock; time.time() is the only clock read."""
    return SimpleNamespace(time=lambda: now)


def _with_clock(now, fn, *args, **kwargs):
    original = live.time
    live.time = _at(now)
    try:
        return fn(*args, **kwargs)
    finally:
        live.time = original


def sweep_scope(req):
    sources = tuple(s for s in (req.get("sources") or []) if isinstance(s, str))
    repo, holder = _repo()
    out = _with_clock(
        req.get("now", 0.0),
        repo.sweep_orphaned_sessions,
        max_idle_seconds=req.get("max_idle_seconds", 0.0),
        sources=sources,
        exclude_pinned=bool(req.get("exclude_pinned")),
        heartbeat_staleness_seconds=req.get("heartbeat_staleness_seconds"),
        heartbeat_ownership_grace_seconds=req.get("heartbeat_ownership_grace_seconds"),
        respect_gateway_heartbeats=bool(req.get("respect_gateway_heartbeats")),
    )
    if not holder.sql:
        return {"skip": True}
    sql, params = holder.sql[0]
    return {"skip": False, "select_sql": sql, "select_params": params}


def idle_archive_sql(req):
    idle_days = req.get("idle_days")
    repo, holder = _repo()
    _with_clock(
        req.get("now", 0.0),
        repo.list_idle_archive_candidates,
        idle_days,
        exclude_pinned=bool(req.get("exclude_pinned")),
    )
    if not holder.sql:
        return {"skip": True}
    sql, params = holder.sql[0]
    return {"skip": False, "sql": sql, "params": params}


def projection_fields(available):
    fields = tuple(f for f in live.SESSION_PROJECTION_FIELDS if f in set(available))
    if not fields:
        return None
    order = " ORDER BY id" if "id" in available else ""
    return {"fields": list(fields), "order": order}


def row_bytes_sql(columns):
    holder = Holder()
    holder.resp = [columns]
    return live._row_bytes_sql(holder)


def expected(r):
    f = r.get("func")
    if f == "escape_like":
        return live._escape_like(r.get("text", ""))
    if f == "like":
        return live._like(r.get("value", ""))
    if f == "placeholders":
        return live._placeholders(int(r.get("count", 0)))
    if f == "id_chunks":
        size = r.get("size", 900)
        return list(live._id_chunks(list(r.get("ids") or []), int(size)))
    if f == "cwd_prefix_clause":
        clauses, params = live._cwd_prefix_filter(r.get("prefix", ""))
        return {"clause": clauses[0], "params": list(params)}
    if f == "last_active_sql":
        return live._sql_session_last_active(r.get("alias", "s"))
    if f == "continuation_edge":
        return live._CONTINUATION_EDGE_SQL
    if f == "continued_ancestors_sql":
        return live._continued_ancestors_sql(r.get("where", ""))
    if f == "pinned_tail_sql":
        return live._PINNED_TAIL_SQL
    if f == "not_pinned_sql":
        return live._not_pinned_sql(r.get("alias", "s"))
    if f == "row_bytes_sql":
        return row_bytes_sql(r.get("columns") or [])
    if f == "projection_fields":
        return projection_fields(r.get("available") or [])
    if f == "sweep_scope":
        return sweep_scope(r)
    if f == "idle_archive_sql":
        return idle_archive_sql(r)
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
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n  c =%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
