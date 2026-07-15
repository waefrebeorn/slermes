"""Oracle for hermes_cli/sqlite_util.py port.

Loads the LIVE upstream module and replays the fixture ops against a real
sqlite3 connection, serializing results to match the C harness for byte-diff:

  add -> 1 (added) / 0 (exists) / -1 (other error)
  txn_begin -> rc (0 ok)
  txn_end -> rc (0 ok)

argv[1] = fixture: {"db":"/tmp/x.db","ops":[...]}.
"""
import sys, os, json
from pathlib import Path

ROOT = Path("/home/wubu/hermes-agent-dev")
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

import importlib.util
spec = importlib.util.spec_from_file_location(
    "hermes_cli.sqlite_util", str(ROOT / "hermes_cli" / "sqlite_util.py"))
assert spec and spec.loader
mod = importlib.util.module_from_spec(spec)
sys.modules["hermes_cli.sqlite_util"] = mod
spec.loader.exec_module(mod)

import sqlite3


def main():
    if len(sys.argv) < 2:
        return 1
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        data = json.load(f)

    dbpath = data["db"]
    os.makedirs(os.path.dirname(dbpath), exist_ok=True)
    if os.path.exists(dbpath):
        os.remove(dbpath)
    conn = sqlite3.connect(dbpath)
    conn.execute("CREATE TABLE IF NOT EXISTS t(id INTEGER)")
    conn.execute("CREATE TABLE IF NOT EXISTS other(x INTEGER)")
    conn.commit()

    results = []
    for o in data.get("ops", []):
        op = o["op"]
        if op == "add":
            try:
                added = mod.add_column_if_missing(conn, o["table"], o["column"], o["ddl"])
                results.append(1 if added else 0)
            except Exception:
                results.append(-1)
        elif op == "txn_begin":
            try:
                conn.execute("BEGIN IMMEDIATE")
                results.append(0)
            except Exception:
                results.append(-1)
        elif op == "txn_end":
            committed = o.get("committed", True)
            try:
                conn.execute("COMMIT" if committed else "ROLLBACK")
                results.append(0)
            except Exception:
                results.append(-1)
    conn.close()
    sys.stdout.write(json.dumps(results, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
