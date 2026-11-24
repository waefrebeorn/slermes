#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/housekeeping/maintenance.py
filter assembly, guards, reports and statements (gate w33_maintenanc_3e1b75_p2).

Same seam as the p1 oracle: a recording fake holder/store stands in for the
SQLite store, and ``time.time`` is pinned.  The statements come from the live
verbs, so the comparison is byte-for-byte SQL plus bound-parameter order.
"""
import contextlib
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w33_maintenanc_3e1b75_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
sys.path.append(str(Path(__file__).resolve().parent))

from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

from persistence.core.errors import ExportTooLarge  # noqa: E402
from persistence.sqlite.housekeeping import maintenance as live  # noqa: E402

WANTED = {
    "prune_filter_names", "prune_filter_where", "prune_where", "count_open_sql",
    "prune_open_guard", "reclaim_estimate", "export_safe", "tree_key",
    "inside_tree", "tree_serve_facts", "auto_archive_extra_set",
    "heartbeat_register", "heartbeat_delete", "prune_batches",
    "retained_tree_sql", "served_tree_sql",
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


def _with_clock(now, fn, *args, **kwargs):
    original = live.time
    live.time = SimpleNamespace(time=lambda: now)
    try:
        return fn(*args, **kwargs)
    finally:
        live.time = original


def _error(exc):
    return {"error": type(exc).__name__, "message": str(exc)}


def prune_filter_where(req):
    filters = req.get("filters")
    filters = dict(filters) if isinstance(filters, dict) else {}
    try:
        where, params = live._prune_filter_where(**filters)
    except TypeError as exc:
        return _error(exc)
    return {"where": where, "params": list(params)}


def prune_where(req):
    filters = req.get("filters")
    filters = dict(filters) if isinstance(filters, dict) else {}
    # JSON has one number type: read the retention as the float the C side sees.
    older = req.get("older_than_days")
    older = None if older is None else float(older)
    source = req.get("source")
    whole = bool(req.get("whole_lineages"))
    try:
        out = _with_clock(req.get("now", 0.0), live._prune_where, older, source, filters,
                          whole_lineages=whole)
    except (ValueError, TypeError) as exc:
        return _error(exc)
    where, params = out
    return {"where": where, "params": list(params)}


def count_open_sql(req):
    filters = req.get("filters")
    filters = dict(filters) if isinstance(filters, dict) else {}
    if filters.get("include_open"):
        return {"zero": True}
    older = req.get("older_than_days")
    older = None if older is None else float(older)
    source = req.get("source")
    try:
        where, params = _with_clock(req.get("now", 0.0), live._prune_where, older, source, filters)
    except (ValueError, TypeError) as exc:
        return _error(exc)
    ended_guard = "s.ended_at IS NOT NULL"
    if not where.startswith(ended_guard):
        return {"error": "RuntimeError", "message": "prune filter lost its ended-session safety guard"}
    open_where = f"s.ended_at IS NULL{where[len(ended_guard):]}"
    return {"sql": f"SELECT COUNT(*) FROM sessions s WHERE {open_where}", "params": list(params)}


def reclaim_estimate(req):
    prunable = int(req.get("prunable", 0))
    total = int(req.get("total", 0))
    prunable_bytes, total_bytes = 0, None
    if prunable:
        prunable_bytes = req.get("prunable_bytes", 0)
        if req.get("measure_total_bytes"):
            total_bytes = req.get("total_bytes")
    return {
        "prunable": int(prunable),
        "total": int(total),
        "prunable_bytes": int(prunable_bytes),
        "total_bytes": None if total_bytes is None else int(total_bytes),
    }


def export_safe(req):
    max_messages = int(req.get("max_messages", 0))
    count = int(req.get("message_count", 0))
    if max_messages < 0:
        return _error(ValueError("max_messages must be non-negative"))
    if max_messages == 0:
        return {"count": 0}
    if count > max_messages:
        return _error(ExportTooLarge(req.get("session_id", ""), count, max_messages))
    return {"count": count}


def tree_key(path):
    return os.path.normcase(str(path)).replace("\\", "/").rstrip("/")


def inside_tree(req):
    inside = live._inside_tree(req.get("worktree_path", ""))
    return inside(req.get("cwd"))


def tree_serve_facts(req):
    chat_ids = [c for c in (req.get("chat_ids") or []) if isinstance(c, str)]
    repo, holder = _repo()
    if req.get("served"):
        holder.resp = [["1"]]
    else:
        holder.resp = [[], [[c] for c in chat_ids]]
    facts = repo.tree_served_sessions("C:/fake/worktree")
    out = {"served": facts.served}
    out["chat_ids"] = None if facts.chat_ids is None else sorted(facts.chat_ids)
    return out


def auto_archive_extra_set():
    captured = {}

    def _fake(holder, conn, column, session_id, value, extra_set_sql=None):
        captured["extra"] = extra_set_sql
        return True

    original = live._set_lineage_column_in_transaction
    live._set_lineage_column_in_transaction = _fake
    try:
        repo, _holder = _repo()
        repo.auto_archive_lineage("sid")
    finally:
        live._set_lineage_column_in_transaction = original
    return captured.get("extra")


def heartbeat_register(req):
    repo, holder = _repo()
    _with_clock(req.get("now", 0.0), repo.register_backend_heartbeat,
                backend_id=req.get("backend_id", ""),
                pid=req.get("pid", 0),
                started_at=req.get("started_at", 0.0),
                last_heartbeat=req.get("last_heartbeat"),
                profile=req.get("profile", ""),
                host=req.get("host", ""))
    if not holder.sql:
        return {"skip": True}
    sql, params = holder.sql[0]
    return {"skip": False, "sql": sql, "params": params}


def heartbeat_delete(req):
    repo, holder = _repo()
    repo.clear_backend_heartbeat(req.get("backend_id", ""))
    if not holder.sql:
        return {"skip": True}
    sql, params = holder.sql[0]
    return {"skip": False, "sql": sql, "params": params}


def prune_batches(req):
    ids = list(req.get("ids") or [])
    size = int(req.get("size", 900))
    out = []
    for chunk in live._id_chunks(ids, size):
        ph = live._placeholders(chunk)
        out.append({
            "placeholders": ph,
            "update_parent_sql": f"UPDATE sessions SET parent_session_id = NULL WHERE parent_session_id IN ({ph})",
            "delete_messages_sql": f"DELETE FROM messages WHERE session_id IN ({ph})",
            "delete_sessions_sql": f"DELETE FROM sessions WHERE id IN ({ph})",
            "params": list(chunk),
        })
    return out


def retained_tree_sql(pending_only):
    repo, holder = _repo()
    repo.retained_tree_is_live("C:/fake/worktree", pending_only=bool(pending_only))
    return holder.sql[0][0]


def expected(r):
    f = r.get("func")
    if f == "prune_filter_names":
        return sorted(live._PRUNE_FILTER_NAMES)
    if f == "prune_filter_where":
        return prune_filter_where(r)
    if f == "prune_where":
        return prune_where(r)
    if f == "count_open_sql":
        return count_open_sql(r)
    if f == "prune_open_guard":
        if r.get("include_open"):
            return _error(ValueError("prune_sessions() never deletes open sessions: include_open is archive-only"))
        return {"ok": True}
    if f == "reclaim_estimate":
        return reclaim_estimate(r)
    if f == "export_safe":
        return export_safe(r)
    if f == "tree_key":
        return tree_key(r.get("path", ""))
    if f == "inside_tree":
        return inside_tree(r)
    if f == "tree_serve_facts":
        return tree_serve_facts(r)
    if f == "auto_archive_extra_set":
        return auto_archive_extra_set()
    if f == "heartbeat_register":
        return heartbeat_register(r)
    if f == "heartbeat_delete":
        return heartbeat_delete(r)
    if f == "prune_batches":
        return prune_batches(r)
    if f == "retained_tree_sql":
        return retained_tree_sql(bool(r.get("pending_only")))
    if f == "served_tree_sql":
        return live._SERVED_TREE_SQL
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
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n  c =%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
