#!/usr/bin/env python3
"""Differential oracle for gate w29_sessions_2114c9_p4.

Truth: persistence/sqlite/conversation/sessions.py's listing/recency/roster
envelopes.  _list_entry, _recency_entry and _roster_session are called live
(_list_entry's holder argument is unused for a row without an explicit unread
flag); the in-method projections of list_row / live_listing_facts /
list_recent_sessions / roster_sessions are rebuilt with the same live
dataclasses, and the summary() closure of _read_bot_sessions is transcribed
line for line because it only runs inside a read connection.
"""
import dataclasses
import enum
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_sessions_2114c9_p4"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite.conversation import sessions as live  # noqa: E402

WANTED = {
    "list_entry", "recency_entry", "roster_session", "roster_summary",
    "roster_last_session", "list_row", "live_listing_row", "pending_by_session",
    "recent_entry", "worker_session",
}


def ser(v):
    if dataclasses.is_dataclass(v):
        return {f.name: ser(getattr(v, f.name)) for f in dataclasses.fields(v)}
    if isinstance(v, enum.Enum):
        return v.value
    if isinstance(v, tuple):
        return [ser(x) for x in v]
    if isinstance(v, dict):
        return {str(k): ser(x) for k, x in v.items()}
    return v


def col(row, key):
    if key not in row:
        raise KeyError(key)
    return row[key]


def summary(req):
    root = req.get("root") or {}
    target = req.get("target") or {}
    message = req.get("text")
    text = " ".join(str(message or "").split()) if message else ""
    return {
        "id": root["id"],
        "resolved_id": req.get("tip"),
        "title": target["title"] or "",
        "root_title": root["title"] or "",
        "preview": text[:80] + "..." if len(text) > 80 else text,
        "started_at": target["started_at"] or 0,
        "last_active": target["last_activity_at"] or target["started_at"] or 0,
        "message_count": target["message_count"] or 0,
    }


def expected(r):
    f = r.get("func")
    if f == "list_entry":
        row = r.get("row") or {}
        return ser(live._list_entry(None, row, compact=bool(r.get("compact"))))
    if f == "recency_entry":
        return ser(live._recency_entry(r.get("row") or {}))
    if f == "roster_session":
        return ser(live._roster_session(r.get("fields")))
    if f == "roster_summary":
        return summary(r)
    if f == "roster_last_session":
        canonical = r.get("canonical")
        latest = r.get("latest")
        if latest and canonical and latest["id"] == canonical["id"]:
            last = dict(canonical)
        elif latest:
            last = latest
        else:
            last = None
        return last
    if f == "list_row":
        row = r.get("row") or {}
        return ser(live.SessionListRow(
            session_id=live.SessionId(col(row, "id")), source=col(row, "source") or "",
            title=col(row, "title"), started_at=float(col(row, "started_at") or 0.0),
            ended_at=col(row, "ended_at"), message_count=col(row, "message_count"),
            model=col(row, "model"), hidden=bool(col(row, "hidden")),
            archived=bool(col(row, "archived"))))
    if f == "live_listing_row":
        row = r.get("row") or {}
        return ser(live.LiveListingRow(
            id=live.SessionId(col(row, "id")), source=col(row, "source") or "",
            title=col(row, "title"), started_at=float(col(row, "started_at") or 0.0),
            ended_at=col(row, "ended_at"), message_count=col(row, "message_count"),
            model=col(row, "model"), hidden=bool(col(row, "hidden")),
            archived=bool(col(row, "archived")),
            runtime_revision=int(col(row, "runtime_revision") or 0),
            runtime_generation=int(col(row, "runtime_generation") or 0),
            target_ended_at=col(row, "target_ended_at")))
    if f == "pending_by_session":
        by_session = {}
        for row in r.get("rows") or []:
            by_session.setdefault(live.SessionId(row["target_session_id"]), []).append(
                live.PendingAdmissionRef(admission_id=row["admission_id"],
                                         seq=int(row["seq"]), status=row["status"]))
        return ser({sid: tuple(refs) for sid, refs in by_session.items()})
    if f == "recent_entry":
        row = r.get("row") or {}
        return ser(live.RecentSessionEntry(
            session_id=live.SessionId(col(row, "id")), source=col(row, "source") or "",
            model=col(row, "model"), title=col(row, "title"),
            started_at=float(col(row, "started_at") or 0.0), ended_at=col(row, "ended_at"),
            end_reason=col(row, "end_reason"), message_count=col(row, "message_count"),
            tool_call_count=col(row, "tool_call_count"),
            last_active=live._last_active(row), preview=row.get("preview") or "",
            lineage_root_id=row.get("_lineage_root_id"), unread=bool(row.get("unread"))))
    if f == "worker_session":
        row = r.get("row") or {}
        return ser(live.RosterWorker(
            id=live.SessionId(col(row, "id")), source=(col(row, "source") or "").strip().lower(),
            title=col(row, "title") or "", last_active=col(row, "last_active") or 0))
    raise AssertionError("unknown func %r" % (f,))


def norm(v):
    return json.loads(json.dumps(v, sort_keys=True))


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
            py = norm(expected(r))
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            if isinstance(py, dict) and isinstance(got, dict):
                diff = {k: (py.get(k), got.get(k)) for k in set(py) | set(got)
                        if py.get(k) != got.get(k)}
                print("MISMATCH %s case=%r fields=%r" % (r.get("func"), r.get("case"), diff),
                      file=sys.stderr)
            else:
                print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                      file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism or not cases else 0)


if __name__ == "__main__":
    main()
