#!/usr/bin/env python3
"""Differential oracle for the status/predicate half of
``tui_gateway/hosted_room_service.py`` (gate w28_hostedroom_e01c0e_p2).

Every value a room reports without touching the store is compared against the
live method: the grant-refresh terminal predicate (real ``PeerRunsHTTPError``),
``_route_statuses`` over a route map, ``live_status_sig`` over a runtime status
dict + pending actions + the real ``PrepareStallTracker``, the turn budget (a
HERMES_AGENT_TIMEOUT fixture), the module constants and ``_authority``.
"""
import json
import os
import sys
import threading
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_hostedroom_e01c0e_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tui_gateway import hosted_room_service as live  # noqa: E402
from tui_gateway.hosted_room_prepare_stall import PrepareStallTracker  # noqa: E402

WANTED = {"grant_revoke_is_terminal", "route_statuses", "live_status_sig",
          "turn_timeout_seconds", "constants", "authority"}


def bare():
    return live.HostedRoomService.__new__(live.HostedRoomService)


def grant_revoke_is_terminal(req):
    kwargs = {}
    if req.get("status_code") is not None:
        kwargs["status_code"] = req["status_code"]
    if req.get("error_code") is not None:
        kwargs["error_code"] = req["error_code"]
    return live._grant_revoke_is_terminal(live.PeerRunsHTTPError("probe", **kwargs))


def route_statuses(req):
    svc = bare()
    svc._state_lock = threading.Lock()
    svc._peer_route_status = {}
    for row in req.get("routes") or []:
        if isinstance(row, dict):
            svc._peer_route_status[(str(row.get("room_id", "")), str(row.get("member_id", "")))] = str(row.get("status", ""))
    room_id = req["room_id"] if "room_id" in req else None
    return svc._route_statuses(room_id)


def live_status_sig(req):
    svc = bare()
    svc._state_lock = threading.Lock()
    svc._peer_route_status = {}
    for row in req.get("routes") or []:
        if isinstance(row, dict):
            svc._peer_route_status[(str(row.get("room_id", "")), str(row.get("member_id", "")))] = str(row.get("status", ""))
    svc._pending_actions = {}
    for row in req.get("pending_actions") or []:
        if isinstance(row, dict):
            svc._pending_actions[(str(row.get("room_id", "")), str(row.get("member_id", "")))] = dict(row.get("action") or {})
    tracker = PrepareStallTracker()
    for row in req.get("prepare_stalls") or []:
        if not isinstance(row, dict):
            continue
        room_id = str(row.get("room_id", ""))
        error = row.get("error")
        if row.get("stalled"):
            tracker.stall(room_id, RuntimeError(error if error else "stalled"))
        elif error:
            tracker.failed(room_id, RuntimeError(error))
    svc._prepare_stalls = tracker
    status = req.get("runtime") or {}
    svc.runtime = type("R", (), {"status": lambda self: {
        "running": bool(status.get("running")), "stopping": bool(status.get("stopping")),
        "blocked_rooms": list(status.get("blocked_rooms") or [])}})()
    sig = svc.live_status_sig()
    return {room: [list(row) if isinstance(row, (list, tuple)) else row for row in rows]
            for room, rows in sig.items()}


def turn_timeout(req):
    env = req.get("env")
    if env is None:
        os.environ.pop("HERMES_AGENT_TIMEOUT", None)
    else:
        os.environ["HERMES_AGENT_TIMEOUT"] = str(env)
    try:
        return live._hosted_room_turn_timeout_seconds()
    finally:
        os.environ.pop("HERMES_AGENT_TIMEOUT", None)


def constants():
    return {
        "idle_fallback_seconds": live._HOSTED_ROOM_IDLE_FALLBACK_SECONDS,
        "active_poll_seconds": live._HOSTED_ROOM_ACTIVE_POLL_SECONDS,
        "terminal_grace_seconds": live._HOSTED_ROOM_TERMINAL_GRACE_SECONDS,
        "fenced_append_attempts": live._FENCED_APPEND_ATTEMPTS,
        "owed_revokes_per_cycle": live._OWED_REVOKES_PER_CYCLE,
        "owed_revoke_timeout_seconds": live._OWED_REVOKE_TIMEOUT_SECONDS,
        "owed_revoke_first_retry_seconds": live._OWED_REVOKE_FIRST_RETRY_SECONDS,
        "owed_revoke_max_retry_seconds": live._OWED_REVOKE_MAX_RETRY_SECONDS,
        "terminal_statuses": list(live._TERMINAL_STATUSES),
        "live_statuses": list(live._LIVE_STATUSES),
        "stoppable_statuses": list(live._STOPPABLE_STATUSES),
        "retryable_statuses": list(live._RETRYABLE_STATUSES),
        "approvable_statuses": list(live._APPROVABLE_STATUSES),
        "peer_run_ended": sorted(live._PEER_RUN_ENDED),
    }


def expected(r):
    f = r.get("func")
    if f == "grant_revoke_is_terminal":
        return grant_revoke_is_terminal(r)
    if f == "route_statuses":
        return route_statuses(r)
    if f == "live_status_sig":
        return live_status_sig(r)
    if f == "turn_timeout_seconds":
        return turn_timeout(r)
    if f == "constants":
        return constants()
    if f == "authority":
        gateway, epoch = live._authority(r.get("room") or {})
        return [gateway, epoch]
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
