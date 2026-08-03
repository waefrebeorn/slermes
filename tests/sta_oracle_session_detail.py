#!/usr/bin/env python3
"""sta_oracle_session_detail.py — Python oracle for the web_server
session-detail stack. Opens the SAME seed.db the C harness used and computes
the equivalent outputs with the live hermes_state.SessionDB, emitting one JSON
object per op (separators compact, ensure_ascii=False) so the diff runner
can exact-compare against t_port_session_detail.c output.

Usage: sta_oracle_session_detail.py <seed.db>
"""
import os
import sys
import json

_PY = "/home/wubu/.hermes/hermes-agent"
if _PY not in sys.path:
    sys.path.insert(0, _PY)

from hermes_state import SessionDB

def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_session_detail.py <seed.db>\n")
        return 2
    db_path = sys.argv[1]
    import pathlib
    db = SessionDB(pathlib.Path(db_path), read_only=False)

    def j(obj):
        return json.dumps(obj, separators=(",", ":"), ensure_ascii=False)

    # 1. sanitize_title
    r = SessionDB.sanitize_title("  Hello\x01World\nFoo\tBar  ")
    print(j({"op": "sanitize", "out": r, "err": None}))
    big = "x" * 120
    try:
        rb = SessionDB.sanitize_title(big)
        print(j({"op": "sanitize_too_long", "out": rb, "err": None}))
    except ValueError as e:
        print(j({"op": "sanitize_too_long", "out": None, "err": str(e)}))

    # 2. get_session
    print(j({"op": "get_session", "out": db.get_session("sessA")}))
    print(j({"op": "get_session_missing", "out": db.get_session("nope")}))

    # 3. get_messages
    print(j({"op": "get_messages", "out": db.get_messages("sessA")}))

    # 4. compression_tip
    print(j({"op": "compression_tip", "out": db.get_compression_tip("sessA")}))

    # 5. resolve_resume_id
    print(j({"op": "resolve_resume", "out": db.resolve_resume_session_id("sessA")}))

    # 6. set_title + get_title
    err = None
    try:
        ok = db.set_session_title("sessB", "New Beta Title")
    except ValueError as e:
        ok = False
        err = str(e)
    got = db.get_session_title("sessB")
    print(j({"op": "set_title", "out": ok, "title": got, "err": err}))

    # 7. set_archived
    print(j({"op": "set_archived", "out": db.set_session_archived("sessA", True)}))

    # 8. export_session
    print(j({"op": "export", "out": db.export_session("sessA")}))

    # 9. latest_descendant — mirror the LIVE _session_latest_descendant:
    # if the session is missing (or resolve fails), return (None, []) which
    # the endpoint turns into a 404.
    sid = db.resolve_session_id("sessA")
    if not sid or not db.get_session(sid):
        print(j({"op": "latest_descendant", "out": {
            "status": 404, "detail": "Session not found"}}))
    else:
        rows = []
        conn = getattr(db, "conn", None) or getattr(db, "_conn", None)
        raw = conn.execute(
            "WITH RECURSIVE descendants(id, parent_session_id, started_at) AS ("
            "  SELECT id, parent_session_id, started_at FROM sessions WHERE id = ? "
            "  UNION "
            "  SELECT s.id, s.parent_session_id, s.started_at FROM sessions s "
            "  JOIN descendants d ON s.parent_session_id = d.id) "
            "SELECT id, parent_session_id, started_at FROM descendants", (sid,)
        ).fetchall()
        for row in raw:
            rows.append({"id": row["id"], "parent_session_id": row["parent_session_id"],
                         "started_at": row["started_at"]})
        children = {}
        for row in rows:
            rid = row["id"]; parent = row["parent_session_id"]
            if rid and parent:
                children.setdefault(parent, []).append(row)
        def started(row):
            try: return float(row.get("started_at") or 0)
            except Exception: return 0.0
        current = sid; path = [sid]; seen = {sid}
        while children.get(current):
            cands = [r for r in children[current] if r["id"] not in seen]
            if not cands: break
            cands.sort(key=started, reverse=True)
            current = cands[0]["id"]; path.append(current); seen.add(current)
        print(j({"op": "latest_descendant", "out": {
            "requested_session_id": "sessA", "session_id": current, "path": path,
            "changed": bool(path and current != "sessA")}}))

    # 10. delete_session
    ok = db.delete_session("sessB")
    after = db.get_session("sessB")
    print(j({"op": "delete", "out": bool(ok), "after": after}))

    db.close()
    return 0

if __name__ == "__main__":
    sys.exit(main())
