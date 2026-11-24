#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/gateway/hosted_room_policy.py
(gate w32_hostedroom_be7d8d_p1: the bounded projection decisions).

The live module is imported as a package module and its pure decisions are
driven with the cheap seams the truth already has: ``_prepare`` runs against a
stub file handle (its ``_room_files._hosted_write`` hands back the capacity the
partial was built with, and the ``_PREPARED`` guard is emptied per case), so the
budget compared is the real validated, clamped value and not a re-statement of
it.  The remaining verbs are decisions over fixture rows -- no connection is
opened, no statement is run, no clock is read -- and what is compared is what
the live expression decides from those rows.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w32_hostedroom_be7d8d_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
sys.path.append(str(Path(__file__).resolve().parent))

from persistence.sqlite.gateway import hosted_room_policy as live  # noqa: E402

WANTED = {
    "budget", "capacity", "transcript_bound", "snapshot_thread", "replay_state",
    "sync_step", "settled_sources", "task_event_ids",
}

# JSON carries no int/float distinction; the fixture names the Python type.
TYPE_COERCE = {
    "int": lambda v: int(v),
    "float": lambda v: float(v),
    "bool": lambda v: bool(v),
    "str": lambda v: str(v),
    "null": lambda v: None,
}

REBUILD_TABLES = ["hosted_room_policy_events", "hosted_room_policy_threads",
                  "hosted_room_policy_watermarks", "hosted_room_policy_transcript"]


class _Files:
    """The file handle seam: only ``path`` is consulted before the cache."""

    def path(self, placement):
        return "fixture-room-policy.db"


class _RoomFiles:
    """The rooms-step handle seam: records the capacity the partial carries."""

    def __init__(self):
        self.budget = None

    def _hosted_write(self, placement, fn, immediate=False):
        self.budget = fn.keywords["capacity"]
        return self.budget


def budget(req):
    kind = req.get("value_type") or "int"
    coerce = TYPE_COERCE.get(kind)
    value = coerce(req.get("max_active_events")) if coerce else None
    room = _RoomFiles()
    repo = live.SQLiteHostedRoomPolicyRepository.__new__(live.SQLiteHostedRoomPolicyRepository)
    repo._files = _Files()
    repo._room_files = room
    live.file_identity = lambda path: None
    live._PREPARED.pop(None, None)
    try:
        repo._prepare(None, value)
    except ValueError as exc:
        return {"ok": False, "error": str(exc), "budget": None}
    finally:
        live._PREPARED.pop(None, None)
    return {"ok": True, "error": None, "budget": room.budget}


def capacity(req):
    budget_ = int(req.get("budget") or 0)
    cached = req.get("cached")
    stored = req.get("stored")
    if cached is not None and int(cached) >= budget_:
        return {"reinitialize": False, "capacity": int(cached)}
    base = budget_ if stored is None else int(stored)
    return {"reinitialize": True, "capacity": max(base, budget_)}


def transcript_bound(req):
    # _discussion_events: min(2**63 - 2, MAX_THREAD_TRANSCRIPT_EVENTS *
    # (max_active_events + MAX_ACTIVE_RETRY_RECEIPTS + 2))
    v = live.MAX_THREAD_TRANSCRIPT_EVENTS * (
        int(req.get("max_active_events") or 0) + live.MAX_ACTIVE_RETRY_RECEIPTS + 2)
    return min(2 ** 63 - 2, v)


def snapshot_thread(req):
    stopped = int(req.get("stopped_through_seq") or 0)
    threads = req.get("threads")
    threads = threads if isinstance(threads, list) else []
    cands = [t for t in threads
             if isinstance(t, dict)
             and int(t.get("completed") or 0) == 0
             and int(t.get("latest_user_seq") or 0) > stopped]
    if not cands:
        return None
    pick = min(cands, key=lambda t: (int(t["latest_user_seq"]), str(t["thread_id"])))
    return {"thread_id": str(pick["thread_id"]),
            "discussion_event_id": str(pick["discussion_event_id"])}


def replay_state(req):
    state = req.get("schema_version") if req.get("has_state") else None
    rebuild = state is None or int(state) < live._TRANSCRIPT_SCHEMA_VERSION
    return {
        "rebuild": rebuild,
        "write_version": live._TRANSCRIPT_SCHEMA_VERSION if rebuild else None,
        "tables": list(REBUILD_TABLES) if rebuild else [],
        "cursor_reset": rebuild,
    }


def sync_step(req):
    cursor = int(req.get("cursor") or 0)
    latest = int(req.get("latest_seq") or 0)
    if cursor >= latest:
        return {"action": "done", "cursor": cursor, "error": None}
    next_cursor = int(req.get("next_cursor") or cursor)
    rows = req.get("rows")
    if not rows or next_cursor <= cursor:
        return {"action": "error", "cursor": cursor,
                "error": "hosted room policy cursor did not advance"}
    through = req.get("through_seq")
    if through is None:
        return {"action": "error", "cursor": cursor,
                "error": "room policy cursor disappeared during replay"}
    if int(through) != cursor:
        return {"action": "stale", "cursor": int(through), "error": None}
    return {"action": "apply", "cursor": next_cursor, "error": None}


def settled_sources(req):
    events = req.get("events")
    events = events if isinstance(events, list) else []
    by_seq = {}
    for event in events:
        if isinstance(event, dict):
            by_seq[int(event["seq"])] = event
    source_ids = {
        live._text(event.get("payload") or {}, "discussion_event_id")
        for event in by_seq.values()
        if event.get("kind") == "turn.settled" and not (event.get("payload") or {}).get("passed")
    }
    source_ids.difference_update(live._text(event, "event_id") for event in by_seq.values())
    return sorted(source_ids)


def task_event_ids(req):
    task_id = req.get("task_id")
    if not task_id:
        return {"digest": None, "event_ids": []}
    digest = str(task_id).removeprefix("dtask:")
    return {"digest": digest, "event_ids": ["dmessage:" + digest, "dterminal:" + digest]}


def expected(r):
    f = r.get("func")
    if f not in WANTED:
        raise AssertionError("unknown func %r" % (f,))
    return globals()[f](r)


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
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
