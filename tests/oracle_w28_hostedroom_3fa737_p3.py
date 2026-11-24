#!/usr/bin/env python3
"""Differential oracle for tui_gateway/hosted_room_driver.py (gate w28_hostedroom_3fa737_p3).

The live module is loaded by file path and the runtime is built with ``__new__``:
no supervisor, no lease keeper, no worker.  Each case drives one live method with
its seams fixed as data:

  * ``_defer_unavailable_route`` -- the doubling backoff, floored at min, capped
    at max, with next_attempt_at = clock + delay;
  * ``_renew_lease_if_needed`` -- the "a keeper already renewed later" substitution
    and the ttl/2 renewal window (``state.renew_lease`` is the only stub);
  * ``_cache_lease`` -- the same process/generation stale-write guard;
  * ``_mark_ambiguous`` -- the expiry the room waits out;
  * ``_complete_acknowledged_stop`` -- deadline Stop vs user Stop, with the store
    transitions stubbed to capture the payload they would commit;
  * ``_run_cycle`` -- one real cursor step and the bounded fan-out over the room
    threads (``_run_room_once`` is re-pointed at a collector, so the threads that
    spawn touch nothing);
  * ``_report_pending_action`` -- the approval action payload and its choice filter;
  * ``HostedRoomRuntime.__init__`` -- the argument validation, which raises before
    any store or thread is touched.
"""
import importlib.util
import json
import sys
import threading
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_hostedroom_3fa737_p3"


def load_live():
    if str(QUARRY) not in sys.path:
        sys.path.insert(0, str(QUARRY))
    path = QUARRY / "tui_gateway" / "hosted_room_driver.py"
    spec = importlib.util.spec_from_file_location("hrd_live_p3", path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


live = load_live()
from persistence.gateway.hosted_rooms import (  # noqa: E402
    DriverLease, HostedTaskIdentity, TaskAttempt)

WANTED = {
    "retry_delay", "renew_decision", "cache_lease_keep", "ambiguous_expiry",
    "deadline_settlement", "cycle_plan", "approval_action", "validate_config",
}


class Seam(BaseException):
    """Raised by a stubbed seam."""


def identity(raw=None):
    raw = raw if isinstance(raw, dict) else {}
    return HostedTaskIdentity(
        room_id=str(raw.get("room_id") or "room-1"),
        task_id=str(raw.get("task_id") or "task-1"),
        thread_id=str(raw.get("thread_id") or "thread-1"),
        turn_id=str(raw.get("turn_id") or "turn-1"),
    )


def task_of(raw):
    raw = raw if isinstance(raw, dict) else {}
    out = dict(raw)
    out["identity"] = identity(raw.get("identity"))
    out.setdefault("execution_generation", 0)
    out.setdefault("cancel_generation", 0)
    out.setdefault("payload", {"target_profile": "profile-a", "target_member_id": "member-9"})
    return out


def lease_of(raw):
    raw = raw if isinstance(raw, dict) else {}
    return DriverLease(
        room_id=str(raw.get("room_id") or "room-1"),
        gateway_id=str(raw.get("gateway_id") or "gw"),
        authority_epoch=int(raw.get("authority_epoch") or 1),
        process_generation=str(raw.get("process_generation") or "pg"),
        lease_generation=int(raw.get("lease_generation") or 1),
        expires_at=float(raw.get("expires_at") or 0.0),
    )


def bare(**attrs):
    r = live.HostedRoomRuntime.__new__(live.HostedRoomRuntime)
    r._status_lock = threading.Lock()
    r._leases = {}
    r._ambiguous_rooms = {}
    r._unavailable_route_retries = {}
    r._blocked_rooms = set()
    r.rpc = None
    r.transport_resolver = None
    r.remote_member = None
    r.pending_action = None
    r.task_progress = None
    r.publish_terminal = None
    r.prepare_room = None
    r.store = None
    r.placement = object()
    r.process_generation = "pg"
    r.lease_ttl_seconds = 30.0
    r.turn_timeout_seconds = 1830.0
    r.indeterminate_defer_seconds = 60.0
    r.unavailable_retry_min_seconds = 1.0
    r.unavailable_retry_max_seconds = 30.0
    r.max_concurrent_rooms = 4
    r.clock = lambda: 0.0
    r._rooms_provider = lambda: []
    for key, value in attrs.items():
        setattr(r, key, value)
    return r


def binding_for(room_id="room-1"):
    return live.HostedRoomBinding(room_id=room_id, gateway_id="gw", authority_epoch=1)


# ------------------------------------------------------------------ handlers

def h_retry_delay(req):
    now = float(req["now"])
    runtime = bare(clock=lambda: now)
    runtime.unavailable_retry_min_seconds = float(req["lo"])
    runtime.unavailable_retry_max_seconds = float(req["hi"])
    previous = req.get("previous")
    if previous is not None:
        runtime._unavailable_route_retries[("room-1", "member-9")] = dict(previous)
    delay = runtime._defer_unavailable_route(task_of(req.get("task")))
    return {"delay": delay, "next_attempt_at": now + delay}


def h_renew_decision(req):
    now = float(req["now"])
    ttl = float(req["ttl"])
    runtime = bare(clock=lambda: now)
    runtime.lease_ttl_seconds = ttl
    lease = lease_of(req.get("lease"))
    known = req.get("known")
    if known is not None:
        runtime._leases[lease.room_id] = lease_of(known)
    calls = []
    original = live.state.renew_lease

    def fake_renew(store, current, **kw):
        calls.append(current.lease_generation)
        return DriverLease(room_id=current.room_id, gateway_id=current.gateway_id,
                           authority_epoch=current.authority_epoch,
                           process_generation=current.process_generation,
                           lease_generation=current.lease_generation, expires_at=now + ttl)

    live.state.renew_lease = fake_renew
    try:
        runtime._renew_lease_if_needed(lease, force=bool(req.get("force")))
    finally:
        live.state.renew_lease = original
    return {"renewed": bool(calls)}


def h_cache_lease_keep(req):
    runtime = bare()
    incoming = lease_of(req.get("incoming"))
    known_raw = req.get("known")
    known = None
    if known_raw is not None:
        known = lease_of(known_raw)
        runtime._leases[incoming.room_id] = known
    returned = runtime._cache_lease(incoming)
    keep = known is not None and returned is known
    return {"keep_known": keep, "expires_at": returned.expires_at}


def h_ambiguous_expiry(req):
    runtime = bare()
    generation = int(req["attempt_generation"])
    expires_at = float(req["attempt_expires_at"])
    known = req.get("known")
    if known is not None:
        runtime._leases["room-1"] = lease_of(known)
    attempt = TaskAttempt(
        identity=identity(), execution_generation=generation, cancel_generation=1,
        lease=DriverLease(room_id="room-1", gateway_id="gw", authority_epoch=1,
                          process_generation="pg", lease_generation=generation,
                          expires_at=expires_at))
    runtime._mark_ambiguous(binding_for("room-1"), attempt)
    return runtime._ambiguous_rooms["room-1"]


def h_deadline_settlement(req):
    task = task_of(req.get("task"))
    runtime = bare()
    runtime.turn_timeout_seconds = float(req["timeout"])
    captured = {}
    original_cancel = live.state.complete_task_cancel
    original_settle = live.state.settle_stopping_task

    def fake_cancel(store, task_identity, **kw):
        captured.update(kw)
        return {"ok": True}

    def fake_settle(store, task_identity, lease, **kw):
        captured.update(kw)
        return {"ok": True}

    live.state.complete_task_cancel = fake_cancel
    live.state.settle_stopping_task = fake_settle
    try:
        runtime._complete_acknowledged_stop(binding_for("room-1"), task,
                                            lease_of({"expires_at": 100.0}))
    finally:
        live.state.complete_task_cancel = original_cancel
        live.state.settle_stopping_task = original_settle
    if "settlement_id" in captured:
        return {"branch": "deadline", "settlement_id": captured["settlement_id"],
                "status": captured["status"], "result": captured["result"]}
    return {"branch": "cancel", "cancel_id": captured.get("cancel_id"),
            "expected_cancel_generation": captured.get("expected_cancel_generation")}


class _AliveThread:
    """A room thread that is already running (the fixture's active room)."""

    def is_alive(self):
        return True

    def join(self, timeout=None):
        return None


def h_cycle_plan(req):
    rooms = list(req.get("rooms") or [])
    active = list(req.get("active") or [])
    runtime = bare()
    runtime._thread = threading.current_thread()
    runtime._stop = threading.Event()
    if req.get("stop"):
        runtime._stop.set()
    runtime._room_threads = {room: _AliveThread() for room in active}
    runtime._rooms_provider = lambda: [
        live.HostedRoomBinding(room_id=room, gateway_id="gw", authority_epoch=1) for room in rooms]
    runtime._room_schedule_cursor = int(req["cursor"])
    runtime.max_concurrent_rooms = int(req["max_concurrent"])
    spawned = []
    runtime._run_room_once = lambda binding: spawned.append(binding.room_id)
    runtime._run_cycle()
    for thread in list(runtime._room_threads.values()):
        thread.join()
    return {"next_cursor": runtime._room_schedule_cursor, "chosen": sorted(set(spawned))}


def h_approval_action(req):
    spec = req.get("spec") or {}
    task = task_of(spec.get("task"))
    info = spec.get("info") or {}
    captured = []
    runtime = bare()
    if spec.get("has_pending"):
        runtime.pending_action = (
            lambda room_id, member_id, action: captured.append(
                {"room_id": room_id, "member_id": member_id, "action": action}))
    runtime._report_pending_action(task, session_id=spec.get("session_id"), info=info)
    if not captured:
        return {"called": False, "room_id": task["identity"].room_id,
                "member_id": live._member_id(task), "action": None}
    return {"called": True, **captured[0]}


CONFIG_KEYS = (
    "lease_ttl_seconds", "poll_interval_seconds", "active_poll_interval_seconds",
    "turn_timeout_seconds", "indeterminate_defer_seconds", "max_concurrent_rooms",
    "unavailable_retry_min_seconds", "unavailable_retry_max_seconds",
)


def h_validate_config(req):
    cfg = req.get("cfg") or {}
    kwargs = dict(store=None, placement=None, rooms=[], turn_lock=None,
                  rpc=object() if cfg.get("has_rpc") else None,
                  transport_resolver=(lambda binding, task: None)
                  if cfg.get("has_transport_resolver") else None)
    for key in CONFIG_KEYS:
        kwargs[key] = cfg[key]
    try:
        live.HostedRoomRuntime(**kwargs)
    except ValueError as exc:
        return {"ok": False, "error": str(exc)}
    return {"ok": True, "error": None}


HANDLERS = {
    "retry_delay": h_retry_delay,
    "renew_decision": h_renew_decision,
    "cache_lease_keep": h_cache_lease_keep,
    "ambiguous_expiry": h_ambiguous_expiry,
    "deadline_settlement": h_deadline_settlement,
    "cycle_plan": h_cycle_plan,
    "approval_action": h_approval_action,
    "validate_config": h_validate_config,
}


def expected(req):
    handler = HANDLERS.get(req.get("func"))
    if handler is None:
        raise AssertionError("unknown func %r" % (req.get("func"),))
    return handler(req)


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except ValueError:
            continue
        if not isinstance(req, dict) or req.get("func") not in WANTED:
            continue
        cases += 1
        try:
            want = expected(req)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (req.get("func"), req.get("case"), exc),
                  file=sys.stderr)
            continue
        got = req.get("out")
        if want != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (req.get("func"), req.get("case"), want, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
