#!/usr/bin/env python3
"""sta_oracle_web_server_sessions_admin.py — LIVE-Python oracle using the
REAL hermes_state.SessionDB class (no extraction, no mocks)."""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path("/home/wubu/hermes-agent-dev")  # live upstream tree))
from hermes_state import SessionDB  # noqa: E402


def main():
    fx = json.loads(Path(sys.argv[1]).read_text())
    op = fx["op"]
    dbp = Path(fx["db"])

    def dump(o):
        print(json.dumps(o, separators=(",", ":"), ensure_ascii=False))

    if op == "count_empty":
        db = SessionDB(dbp)
        dump({"count": db.count_empty_sessions()})
        db.close()
    elif op == "delete_empty":
        db = SessionDB(dbp)
        deleted = db.delete_empty_sessions()
        remaining = db.session_count(include_archived=True)
        empty_left = db.count_empty_sessions()
        db.close()
        dump({"deleted": deleted, "remaining": remaining,
              "empty_left": empty_left})
    elif op == "message_count":
        db = SessionDB(dbp)
        dump({"count": db.message_count(fx.get("sid"))})
        db.close()
    elif op == "session_count":
        db = SessionDB(dbp)
        dump({"count": db.session_count(
            source=fx.get("source"),
            include_archived=bool(fx.get("include_archived")),
            archived_only=bool(fx.get("archived_only")),
            exclude_children=bool(fx.get("exclude_children")),
            min_message_count=int(fx.get("min_messages") or 0))})
        db.close()
    elif op == "stats":
        # Replay of the get_session_stats endpoint body over the REAL class.
        db = SessionDB(dbp)
        try:
            total = db.session_count(include_archived=True)
            active_store = db.session_count(include_archived=False)
            archived = db.session_count(archived_only=True)
            messages = db.message_count()
            by_source = {}
            try:
                for s in db.list_sessions_rich(limit=10000,
                                               include_archived=True,
                                               compact_rows=True):
                    src = str(s.get("source") or "cli")
                    by_source[src] = by_source.get(src, 0) + 1
            except Exception:
                pass
            dump({"total": total, "active_store": active_store,
                  "archived": archived, "messages": messages,
                  "by_source": by_source})
        finally:
            db.close()
    else:
        raise SystemExit(f"unknown op {op}")


if __name__ == "__main__":
    main()
