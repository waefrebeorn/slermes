#!/usr/bin/env python3
"""Create the shared session_detail oracle fixture DB.

Builds a fresh sqlite store with the real Hermes schema (via SessionDB's own
migrations) and seeds two sessions (one with a child compression continuation)
plus a few messages. The same DB is then opened by both the C harness and the
Python oracle so their outputs are directly comparable.
"""
import os
import sys
import sqlite3
import json

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", "..."))
# import hermes_state from the live python source
_PY = "/home/wubu/.hermes/hermes-agent"
if _PY not in sys.path:
    sys.path.insert(0, _PY)

from hermes_state import SessionDB
import pathlib

HERE = os.path.dirname(os.path.abspath(__file__))
DB = os.path.join(HERE, "seed.db")

def main():
    if os.path.exists(DB):
        os.remove(DB)
    db = SessionDB(pathlib.Path(DB), read_only=False)

    # Session A (root) with messages.
    db._conn.execute(
        "INSERT INTO sessions(id,source,started_at,ended_at,end_reason,message_count,title) "
        "VALUES(?,?,?,?,?,?,?)",
        ("sessA", "cli", 1000.0, None, None, 2, "Alpha Session"),
    )
    db._conn.execute(
        "INSERT INTO messages(id,session_id,role,content,timestamp,active) "
        "VALUES(?,?,?,?,?,?)",
        (1, "sessA", "user", "hello world", 1001.0, 1),
    )
    db._conn.execute(
        "INSERT INTO messages(id,session_id,role,content,tool_calls,timestamp,active) "
        "VALUES(?,?,?,?,?,?,?)",
        (2, "sessA", "assistant", "hi there", '[{"id":"c1","name":"search"}]', 1002.0, 1),
    )
    # compression continuation child of A (tip)
    db._conn.execute(
        "INSERT INTO sessions(id,source,parent_session_id,started_at,ended_at,end_reason,message_count,title) "
        "VALUES(?,?,?,?,?,?,?,?)",
        ("sessA_child", "cli", "sessA", 2000.0, None, "compression", 1, "Alpha Continued"),
    )
    db._conn.execute(
        "INSERT INTO messages(id,session_id,role,content,timestamp,active) "
        "VALUES(?,?,?,?,?,?)",
        (3, "sessA_child", "user", "after compression", 2001.0, 1),
    )
    # Session B: title with control chars + too-long candidate
    db._conn.execute(
        "INSERT INTO sessions(id,source,started_at,ended_at,message_count,title) "
        "VALUES(?,?,?,?,?,?)",
        ("sessB", "cli", 3000.0, None, 0, "  Beta\tSession  "),
    )
    db._conn.commit()
    db.close()
    print("seeded", DB)

if __name__ == "__main__":
    main()
