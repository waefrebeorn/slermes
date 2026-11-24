#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/gateway/hosted_room_driver.py
(gate w29_hostedroom_8927a5_p1: statement text and schema verdicts).

The live module is imported as a package module and driven with a scripted
connection: every ``conn.execute`` the schema step and the statement builders
make is recorded, and the answers a real SQLite would give for its own tables
are supplied by the adapter below.  That is the seam -- no file is opened, no
transaction is begun -- and what is compared is exactly the text those values
decide.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_hostedroom_8927a5_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite.gateway import hosted_room_driver as live  # noqa: E402

WANTED = {
    "task_update", "generation_update", "named_sql", "tasks_in_order_sql", "task_index_sql",
    "task_columns", "transition", "ddl", "schema_sql", "step_mode", "schema_objects_exist",
    "validate_schema", "schema_ready",
}

NAMED = {
    "select_lease": "_SELECT_LEASE",
    "select_task": "_SELECT_TASK",
    "settle_running_sql": "_SETTLE_RUNNING_SQL",
    "settle_stopping_sql": "_SETTLE_STOPPING_SQL",
    "requeue_running_sql": "_REQUEUE_RUNNING_SQL",
    "cancel_queued_sql": "_CANCEL_QUEUED_SQL",
    "begin_stop_sql": "_BEGIN_STOP_SQL",
    "complete_stop_sql": "_COMPLETE_STOP_SQL",
}

LEASE_COLUMNS = ["acquired_at", "authority_epoch", "expires_at", "gateway_id", "lease_generation",
                 "process_generation", "released_at", "room_id", "updated_at"]


class Seq:
    """A sqlite3.Row stand-in that answers by position and by name."""

    def __init__(self, values):
        self._v = list(values)

    def __getitem__(self, key):
        if isinstance(key, int):
            return self._v[key]
        raise KeyError(key)


class Row(dict):
    def __init__(self, values):
        super().__init__(values)
        self._v = list(values.values())

    def __getitem__(self, key):
        if isinstance(key, int):
            return self._v[key]
        return super().__getitem__(key)


class Cur:
    def __init__(self, rows, rowcount=None):
        self.rows = list(rows)
        self.rowcount = len(self.rows) if rowcount is None else rowcount

    def fetchone(self):
        return self.rows[0] if self.rows else None

    def fetchall(self):
        return list(self.rows)

    def __iter__(self):
        return iter(self.rows)


class Conn:
    """Records every statement; answers the schema probes from the fixture."""

    def __init__(self, answers=None):
        self.answers = list(answers or [])
        self.sql = []

    def execute(self, sql, params=()):
        import sqlite3

        self.sql.append(sql)
        for a in self.answers:
            if a["match"] in sql:
                if a.get("error"):
                    raise sqlite3.OperationalError(a["error"])
                return Cur(a.get("rows") or [], a.get("rowcount"))
        return Cur([], 1)


def schema_answers(req):
    tables = req.get("tables")
    tables = tables if isinstance(tables, list) else []
    lease_columns = req.get("lease_columns")
    lease_columns = lease_columns if isinstance(lease_columns, list) else LEASE_COLUMNS
    task_columns = req.get("task_columns")
    task_columns = task_columns if isinstance(task_columns, list) else list(live._TASK_COLUMN_ORDER)
    task_sql = req.get("task_sql")
    lease_fk = req.get("lease_fk", True)
    task_fk = req.get("task_fk", True)
    return [
        {"match": "SELECT sql FROM sqlite_master", "rows": [Row({"sql": task_sql})] if task_sql is not None else []},
        {"match": "SELECT name FROM sqlite_master", "rows": [Row({"name": t}) for t in tables]},
        {"match": "SELECT 1 FROM sqlite_master WHERE type='index'",
         "rows": [Row({"1": 1})] if req.get("has_index") else []},
        {"match": "PRAGMA table_info(hosted_room_driver_leases)",
         "rows": [Seq([i, c]) for i, c in enumerate(lease_columns)]},
        {"match": "PRAGMA table_info(hosted_room_driver_tasks)",
         "rows": [Seq([i, c]) for i, c in enumerate(task_columns)]},
        {"match": "PRAGMA foreign_key_list(hosted_room_driver_leases)",
         "rows": [Seq([0, 0, "hosted_rooms", "room_id", "room_id"])] if lease_fk else []},
        {"match": "PRAGMA foreign_key_list(hosted_room_driver_tasks)",
         "rows": [Seq([0, 0, "hosted_rooms", "room_id", "room_id"])] if task_fk else []},
    ]


def objects_answers(exists):
    if exists:
        return [{"match": "SELECT name FROM sqlite_master",
                 "rows": [Row({"name": "hosted_room_driver_leases"}), Row({"name": "hosted_room_driver_tasks"})]},
                {"match": "SELECT 1 FROM sqlite_master WHERE type='index'", "rows": [Row({"1": 1})]}]
    return [{"match": "SELECT name FROM sqlite_master", "rows": []}]


def expected(r):
    f = r["func"]
    if f == "task_update":
        return live._task_update(r["set"], r["fence"])
    if f == "generation_update":
        return live._generation_update(r["set"], r["status"])
    if f == "named_sql":
        return getattr(live, NAMED[r["name"]])
    if f == "tasks_in_order_sql":
        conn = Conn()
        live._tasks_in_order(conn, "r", r.get("status") if r.get("status") is not None else None)
        return conn.sql[0]
    if f == "task_index_sql":
        return live._TASK_INDEX_SQL.format(if_not_exists="IF NOT EXISTS " if r.get("if_not_exists") else "")
    if f == "task_columns":
        return list(live._TASK_COLUMN_ORDER)
    if f == "transition":
        status, set_clause, generation_stale, stale = live._GENERATION_TRANSITIONS[r["name"]]
        return {"status": status, "set": set_clause, "generation_stale": generation_stale, "stale": stale,
                "sql": live._generation_update(set_clause, status)}
    if f == "ddl":
        conn = Conn(schema_answers({}))
        if r["kind"] == "leases":
            live._initialize_schema(conn)
        else:
            live._create_task_table(conn, r.get("table") or "hosted_room_driver_tasks")
        return conn.sql[0]
    if f == "schema_sql":
        conn = Conn(schema_answers({"tables": ["hosted_room_driver_leases", "hosted_room_driver_tasks"],
                                    "has_index": True} if r.get("objects_exist") else {"tables": []}))
        live._initialize_store(conn, None)
        return conn.sql
    if f == "step_mode":
        conn = Conn(schema_answers({"tables": ["hosted_room_driver_leases", "hosted_room_driver_tasks"],
                                    "has_index": True} if r.get("objects_exist") else {"tables": []}))
        live._initialize_store(conn, None)
        return "migrate" if any(s.lstrip().upper().startswith("DROP INDEX") for s in conn.sql) else "initialize"
    if f == "schema_objects_exist":
        conn = Conn(schema_answers(r))
        return live._schema_objects_exist(conn)
    if f == "validate_schema":
        conn = Conn(schema_answers(r))
        try:
            live._validate_schema(conn)
        except live.DriverStateError as exc:
            return {"error": type(exc).__name__, "message": str(exc)}
        return None
    if f == "schema_ready":
        conn = Conn(schema_answers(r))
        out = {"ready": False, "error": None, "message": None}
        try:
            out["ready"] = bool(live._schema_ready(conn, None))
        except live.DriverStateError as exc:
            out["error"] = type(exc).__name__
            out["message"] = str(exc)
        return out
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            if isinstance(py, str) and isinstance(got, str):
                for i, (a, b) in enumerate(zip(py, got)):
                    if a != b:
                        print("MISMATCH %s case=%r first-diff at %d py=%r c=%r"
                              % (r.get("func"), r.get("case"), i, py[max(0, i - 40):i + 40],
                                 got[max(0, i - 40):i + 40]), file=sys.stderr)
                        break
                else:
                    print("MISMATCH %s case=%r length py=%d c=%d" % (r.get("func"), r.get("case"), len(py), len(got)),
                          file=sys.stderr)
            else:
                print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
