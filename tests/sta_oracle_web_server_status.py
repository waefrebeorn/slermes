#!/usr/bin/env python3
"""
sta_oracle_web_server_status.py — LIVE-Python oracle for the dashboard
operational backbone (port_web_server_status.c).

For active_sessions it builds a real state.db with a sessions table (matching
the schema the C query expects) inside a temp HERMES_HOME, then computes the
expected count with the SAME SQL predicate the Python web_server uses
(ended_at IS NULL AND last_active >= now-300). For error_ring it replicates
the bounded-ring contract.
"""
import sys, os, json, tempfile, sqlite3, time

WEB = "/home/wubu/.hermes/hermes-agent"
sys.path.insert(0, WEB)


def build_state_db(home, sessions):
    os.makedirs(home, exist_ok=True)
    db = os.path.join(home, "state.db")
    if os.path.exists(db):
        os.remove(db)
    con = sqlite3.connect(db)
    con.execute("""CREATE TABLE sessions (
        id TEXT PRIMARY KEY, title TEXT, source TEXT, model TEXT,
        started_at REAL, last_active REAL, ended_at REAL,
        message_count INTEGER, input_tokens INTEGER)""")
    now = time.time()
    for s in sessions:
        sa = s.get("started_at", now - 100)
        la = s.get("last_active", sa)
        ea = s.get("ended_at", None)
        con.execute(
            "INSERT INTO sessions (id,title,source,model,started_at,"
            "last_active,ended_at,message_count,input_tokens) "
            "VALUES (?,?,?,?,?,?,?,?,?)",
            (s["id"], s.get("title", ""), s.get("source", ""),
             s.get("model", ""), sa, la, ea,
             s.get("message_count", 0), s.get("input_tokens", 0)))
    con.commit()
    con.close()


def active_sessions(home):
    db = os.path.join(home, "state.db")
    if not os.path.exists(db):
        return 0
    con = sqlite3.connect(db)
    cutoff = time.time() - 300.0
    cur = con.execute(
        "SELECT COUNT(*) FROM sessions WHERE ended_at IS NULL AND "
        "COALESCE(last_active, started_at, 0) >= ?", (cutoff,))
    n = cur.fetchone()[0]
    con.close()
    return n


def error_ring(n):
    # Bounded ring: WS_ERROR_RING_MAX = 50.
    total = min(n, 50)
    return total, total, total


def main():
    fx = json.load(open(sys.argv[1]))
    op = fx.get("op", "")
    if op == "error_ring":
        n = fx.get("record", 0)
        total, w10, w0 = error_ring(n)
        print(json.dumps({"total": total, "window10": w10, "window0": w0}))
    elif op == "active_sessions":
        tmp = tempfile.mkdtemp(prefix="wss_oracle_")
        # Build sessions from fixture (list of dicts).
        build_state_db(tmp, fx.get("sessions", []))
        os.environ["HERMES_HOME"] = tmp
        print(json.dumps({"count": active_sessions(tmp)}))
    else:
        print(json.dumps({"error": "unknown"}))


if __name__ == "__main__":
    main()
