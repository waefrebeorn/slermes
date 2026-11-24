#!/usr/bin/env python3
"""Differential oracle for the envelope/tally/guard half of
``tui_gateway/hosted_room_service.py`` (gate w28_hostedroom_e01c0e_p3).

The live methods are driven with their I/O replaced by fixtures:

  * ``_append_room_status``  -- ``hosted_rooms.append_event`` captures the kwargs
    the room would write (the event id, actor and payload are the value under test);
  * ``_append_continue``     -- the room log read and the paused-discussion lookup
    are fixtures, the grant guard and the ``room.continued`` envelope are live;
  * ``_stop_room``           -- the task snapshot, the fence and each cancel answer
    are fixtures, the selection, the per-attempt cancel id and the count are live;
  * ``retry_room_task``      -- the retryable candidate list is a fixture, the
    match predicate and the two branches are live;
  * ``retry_owed_revokes``   -- the due-revoke list, the clock and each client
    answer are fixtures, the settle decision and the backoff are live.
"""
import json
import sys
import threading
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_hostedroom_e01c0e_p3"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tui_gateway import hosted_room_service as live  # noqa: E402
from tui_gateway.hosted_room_prepare_stall import PrepareStallTracker  # noqa: E402

WANTED = {"activity_event", "continue_event", "stop_room_tally", "retry_room_task", "owed_revokes"}

ROOM = {"room_id": "r1", "authority_gateway_id": "gw", "authority_epoch": 4}


class Captured:
    def __init__(self):
        self.calls = []

    def append_event(self, *args, **kwargs):
        self.calls.append(kwargs)
        return {"seq": 1}


def bare():
    svc = live.HostedRoomService.__new__(live.HostedRoomService)
    svc.store, svc.placement = None, "shared"
    return svc


def strip_placement(kwargs):
    return {k: v for k, v in kwargs.items() if k != "placement"}


def activity_event(req):
    cap = Captured()
    original = live.hosted_rooms.append_event
    live.hosted_rooms.append_event = cap.append_event
    try:
        svc = bare()
        room = dict(req.get("room") or ROOM)
        decision = types.SimpleNamespace(**req.get("decision") or {})
        svc._append_room_status(room, decision)
    finally:
        live.hosted_rooms.append_event = original
    return strip_placement(cap.calls[0]) if cap.calls else None


def continue_event(req):
    cap = Captured()
    original_append = live.hosted_rooms.append_event
    original_paused = live.router.paused_discussion
    live.hosted_rooms.append_event = cap.append_event
    paused = req.get("paused")
    live.router.paused_discussion = lambda events, event_id: paused
    try:
        svc = bare()
        events = [{"seq": int(seq)} for seq in req.get("event_seqs") or []]
        svc._events = lambda room_id: events
        try:
            svc._append_continue(req.get("room_id") or "", req.get("discussion_event_id") or "",
                                 req.get("gateway_id") or "", int(req.get("epoch") or 0))
        except Exception as exc:  # noqa: BLE001
            text = str(exc)
            if text == "This discussion is not paused.":
                return {"outcome": "not_paused"}
            if text == "This discussion already used every Continue.":
                return {"outcome": "exhausted"}
            raise
        return {"outcome": "append", "event": strip_placement(cap.calls[-1])}
    finally:
        live.hosted_rooms.append_event = original_append
        live.router.paused_discussion = original_paused


def stop_room_tally(req):
    fixture = req.get("fixture") or {}
    tasks = []
    for raw in fixture.get("tasks") or []:
        identity = raw.get("identity") or {}
        tasks.append({
            "identity": live.HostedTaskIdentity(str(identity.get("room_id", "")), str(identity.get("task_id", "")),
                                                str(identity.get("thread_id", "")), str(identity.get("turn_id", ""))),
            "status": raw.get("status"),
            "cancel_id": raw.get("cancel_id"),
            "execution_generation": raw.get("execution_generation", 0),
            "payload": {"source_event_seq": raw.get("source_event_seq", 0)},
        })
    results = {raw["identity"]["task_id"]: raw.get("cancel_result", "cancelled")
               for raw in fixture.get("tasks") or [] if raw.get("identity")}
    cancels = []

    class Runtime:
        def cancel(self, identity, cancel_id=None, **kwargs):
            cancels.append({"task_id": identity.task_id, "cancel_id": cancel_id})
            return {"status": results.get(identity.task_id, "cancelled")}

        def wakeup(self):
            pass

    class Service(live.HostedRoomService):
        def _room(self, room_id):
            return dict(ROOM)

        def _list_tasks(self, room_id, statuses):
            for task in tasks:
                if task["status"] in statuses:
                    yield task

    original_local = live.hosted_rooms.local_authority_gateway_id
    original_stop = live.hosted_rooms.request_room_stop
    live.hosted_rooms.local_authority_gateway_id = lambda: "gw"
    live.hosted_rooms.request_room_stop = lambda *a, **kw: {"seq": int(fixture.get("fence_seq") or 0)}
    try:
        svc = Service.__new__(Service)
        svc.store, svc.placement, svc.runtime = None, "shared", Runtime()
        svc._policy_lock, svc._state_lock = threading.RLock(), threading.Lock()
        svc._pending_actions = {}
        try:
            count = svc._stop_room(fixture.get("room_id") or "", cancel_id=fixture.get("cancel_id") or "",
                                   require_acknowledged=bool(fixture.get("require_acknowledged")))
        except live.RoomStopPendingError:
            return {"outcome": "refused", "count": None, "cancels": cancels}
        return {"outcome": "ok", "count": count, "cancels": cancels}
    finally:
        live.hosted_rooms.local_authority_gateway_id = original_local
        live.hosted_rooms.request_room_stop = original_stop


def retry_room_task(req):
    fixture = req.get("fixture") or {}
    tasks = []
    for raw in fixture.get("tasks") or []:
        identity = raw.get("identity") or {}
        tasks.append({
            "identity": live.HostedTaskIdentity(str(identity.get("room_id", "")), str(identity.get("task_id", "")),
                                                str(identity.get("thread_id", "")), str(identity.get("turn_id", ""))),
            "status": raw.get("status"),
            "execution_generation": raw.get("execution_generation", 0),
            "payload": dict(raw.get("payload") or {}),
        })

    class Runtime:
        def retry_indeterminate(self, identity, execution_generation=None):
            return {"identity": identity, "status": "queued", "execution_generation": execution_generation}

        def wakeup(self):
            pass

    class Service(live.HostedRoomService):
        def _list_tasks(self, room_id, statuses):
            for task in tasks:
                if task["status"] in statuses:
                    yield task

        def bindings(self):
            return (live.HostedRoomBinding(fixture.get("room_id") or "", "gw", 4),) \
                if fixture.get("binding_present") else ()

        def _prepare_tracked(self, binding, retry_stalled=False):
            if not fixture.get("prepare_ok", True):
                raise RuntimeError("prepare failed")

    svc = Service.__new__(Service)
    svc.store, svc.placement, svc.runtime = None, "shared", Runtime()
    svc._policy_lock, svc._state_lock = threading.RLock(), threading.Lock()
    svc._pending_actions = {}
    svc._prepare_stalls = PrepareStallTracker()
    if fixture.get("stalled"):
        svc._prepare_stalls.stall(fixture.get("room_id") or "", RuntimeError("stalled"))
    try:
        result = svc.retry_room_task(fixture.get("room_id") or "", task_id=fixture.get("task_id") or "",
                                     member_id=fixture.get("member_id"),
                                     execution_generation=fixture.get("execution_generation"))
    except live.InvalidTaskTransitionError:
        return {"outcome": "invalid"}
    except Exception as exc:  # noqa: BLE001
        return {"outcome": "error", "error": type(exc).__name__}
    if result.get("status") == "prepared":
        return {"outcome": "prepared", "task_id": result["identity"].task_id,
                "execution_generation": result["execution_generation"]}
    return {"outcome": "retry", "task_id": result["identity"].task_id,
            "execution_generation": result["execution_generation"]}


class RevokeClient:
    def __init__(self, mode, text):
        self.mode, self.text = mode, text

    def revoke_grant(self, grant=None):
        if self.mode == "fault":
            raise live.PeerRunsHTTPError(self.text)
        if self.mode == "terminal":
            raise live.PeerRunsHTTPError("gone", status_code=403, error_code="invalid_room_grant")
        return {"ok": True}


def owed_revokes(req):
    fixture = req.get("fixture") or {}
    owed = list(fixture.get("owed") or [])
    settles = []
    errors = []

    class Runtime:
        def clock(self):
            return float(fixture.get("clock") or 0.0)

        def _record_error(self, message):
            errors.append(message)

        def wakeup(self):
            pass

    svc = live.HostedRoomService.__new__(live.HostedRoomService)
    svc.store, svc.placement, svc.runtime = None, "shared", Runtime()
    svc._policy_lock = threading.RLock()
    svc._may_owe_revokes = bool(fixture.get("may_owe"))
    svc.peer_routes = {("r0", "m0"): object()} if fixture.get("peer_routes") else {}
    svc._revoke_clients = {}
    for row in owed:
        svc._revoke_clients[str(row.get("grant_digest"))] = RevokeClient(
            str(row.get("result") or "ok"), str(row.get("fault_text") or "boom"))

    original_due = live.hosted_rooms.due_room_grant_revokes
    original_settle = live.hosted_rooms.settle_room_grant_revoke
    original_owes = live.hosted_rooms.owes_room_grant_revokes
    live.hosted_rooms.due_room_grant_revokes = lambda *a, **kw: owed
    live.hosted_rooms.settle_room_grant_revoke = lambda *a, **kw: settles.append(kw)
    live.hosted_rooms.owes_room_grant_revokes = lambda *a, **kw: bool(fixture.get("owes_after"))
    try:
        settled = svc.retry_owed_revokes()
    finally:
        live.hosted_rooms.due_room_grant_revokes = original_due
        live.hosted_rooms.settle_room_grant_revoke = original_settle
        live.hosted_rooms.owes_room_grant_revokes = original_owes
    rows = []
    for kwargs in settles:
        clean = strip_placement(kwargs)
        rows.append({"room_id": clean.get("room_id"), "member_id": clean.get("member_id"),
                     "grant_digest": clean.get("grant_digest"), "retry_at": clean.get("retry_at")})
    return {"settled": settled, "settles": rows, "errors": errors,
            "may_owe_after": bool(svc.peer_routes) or bool(fixture.get("owes_after"))}


def expected(r):
    f = r.get("func")
    if f == "activity_event":
        return activity_event(r)
    if f == "continue_event":
        return continue_event(r)
    if f == "stop_room_tally":
        return stop_room_tally(r)
    if f == "retry_room_task":
        return retry_room_task(r)
    if f == "owed_revokes":
        return owed_revokes(r)
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
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
