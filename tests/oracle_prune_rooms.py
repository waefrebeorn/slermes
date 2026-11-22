#!/usr/bin/env python3
"""Oracle for gateway/hosted_rooms.py:_prune_disbanded_rooms_locked.
Builds an identical in-memory sqlite DB, runs the LIVE function, and compares
the returned count plus a canonical dump of the affected tables."""
import ast
import json
import sqlite3
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/gateway/hosted_rooms.py"
DEPS = [
    "hosted_room_policy_transcript_state", "hosted_room_policy_transcript",
    "hosted_room_policy_publications", "hosted_room_policy_watermarks",
    "hosted_room_policy_events", "hosted_room_policy_threads",
    "hosted_room_policy_cursors", "hosted_room_driver_tasks",
    "hosted_room_driver_leases", "hosted_room_remote_runs", "hosted_room_links",
    "hosted_room_peer_reservations", "hosted_room_events"]
ns = {"set": set, "str": str, "int": int, "tuple": tuple, "sorted": sorted,
      "DISBANDED_ROOM_RETENTION_SECONDS": 90 * 24 * 60 * 60,
      "MAX_DISBANDED_ROOM_TOMBSTONES": 512,
      "_DEPENDENT_TABLES": tuple(DEPS),
      "_RETIRE_FROM_ROOMS": (
          "INSERT OR IGNORE INTO hosted_room_retired_ids (room_id, retired_at)"
          " SELECT room_id, disbanded_at FROM hosted_rooms WHERE {where}")}


def _room_ids(conn, sql, params):
    return [str(row["room_id"]) for row in conn.execute(sql, params).fetchall()]


def _gateway_event_bytes(conn):
    return int(conn.execute("SELECT COALESCE(SUM(event_bytes), 0) FROM hosted_rooms").fetchone()[0])


def table_exists(conn, table):
    return conn.execute("SELECT 1 FROM sqlite_master WHERE type='table' AND name=?",
                        (table,)).fetchone() is not None


ns["_room_ids"] = _room_ids
ns["_gateway_event_bytes"] = _gateway_event_bytes
ns["table_exists"] = table_exists
src = open(SRC, encoding="utf-8").read()
for n in ast.parse(src).body:
    if isinstance(n, ast.FunctionDef) and n.name == "_prune_disbanded_rooms_locked":
        n.decorator_list = []
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
live = ns["_prune_disbanded_rooms_locked"]


def build(case):
    conn = sqlite3.connect(":memory:")
    conn.row_factory = sqlite3.Row
    conn.execute("CREATE TABLE hosted_rooms(room_id TEXT PRIMARY KEY, disbanded_at REAL, event_bytes INTEGER)")
    conn.execute("CREATE TABLE hosted_room_retired_ids(room_id TEXT, retired_at REAL)")
    for t in DEPS:
        conn.execute(f"CREATE TABLE {t}(room_id TEXT, v TEXT)")
    for r in case.get("rooms", []):
        conn.execute("INSERT OR REPLACE INTO hosted_rooms VALUES(?,?,?)",
                     (r["room_id"], r["disbanded_at"], r["event_bytes"]))
    for (rid,) in conn.execute("SELECT room_id FROM hosted_rooms").fetchall():
        conn.execute("INSERT INTO hosted_room_events VALUES(?,?)", (rid, "ev"))
        conn.execute("INSERT INTO hosted_room_links VALUES(?,?)", (rid, "lk"))
    conn.commit()
    return conn


def dump(conn):
    out = {}
    for key, table, col in [("hosted_rooms", "hosted_rooms", "disbanded_at"),
                            ("retired", "hosted_room_retired_ids", "retired_at"),
                            ("events", "hosted_room_events", "v"),
                            ("links", "hosted_room_links", "v")]:
        rows = conn.execute(f"SELECT room_id, {col} FROM {table} ORDER BY room_id, {col}").fetchall()
        out[key] = [[str(r[0]), str(r[1])] for r in rows]
    return out


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        c = json.loads(line)
    except Exception:
        continue
    conn = build(c)
    now = c.get("now") if "now" in c else None
    cap = c.get("max_bytes") if "max_bytes" in c else None
    py_out = live(conn, now=now, max_gateway_event_bytes=cap)
    py_db = dump(conn)
    c_out, c_db = c["out"], c.get("db", {})
    cases += 1
    if py_out != c_out or py_db != c_db:
        mism += 1
        print(f"MISMATCH out py={py_out} c={c_out}", file=sys.stderr)
        for k in py_db:
            if py_db[k] != c_db.get(k):
                print(f"  {k}: py={py_db[k]} c={c_db.get(k)}", file=sys.stderr)
print(f"ORACLE prune_rooms: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
