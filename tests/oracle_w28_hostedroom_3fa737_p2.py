#!/usr/bin/env python3
"""Differential oracle for tui_gateway/hosted_room_driver.py (gate w28_hostedroom_3fa737_p2).

The live module is loaded by file path; the runtime is built with ``__new__`` so
no supervisor thread, lease keeper, store or transport ever starts.  The one
substitution is the *transport seam*: a fake answers ``resolve_exact/resume/
info/discard/interrupt`` with the fixture's fixed observations, exactly the way
a real member route would.  What the live code then decides is compared:

  * ``_info_active`` / ``_info_is_active_for`` (activity and exact admission);
  * ``_peer_stop_acknowledged`` (a peer's durable Stop receipt);
  * ``_stop_outcome`` -- the whole router: absent session, persisted-Stop
    Discard, inactive ack, stale-admission interrupt, live interrupt ack;
  * ``_discard_unknown_stop`` (guard + retained-answer validation);
  * ``_past_execution_deadline`` and ``_defer_unconfirmed_stop``'s window;
  * ``_expire_attempt_deadline``'s cancel id;
  * ``retry_indeterminate``'s routing.

Seams that are stubbed only to observe a decision: ``state.get_task`` (returns
the fixture task), ``state.begin_task_cancel`` (captures the cancel id),
``_ensure_lease`` (raises a sentinel once the guards have passed), and
``_tasks`` (empty room task list).  ``_reconcile_indeterminate``'s deferral
deadline is a single inline expression with no seam-free callable; the oracle
transcribes that line verbatim and says so here.
"""
import importlib.util
import json
import sys
import threading
from dataclasses import asdict
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_hostedroom_3fa737_p2"


def load_live():
    if str(QUARRY) not in sys.path:
        sys.path.insert(0, str(QUARRY))
    path = QUARRY / "tui_gateway" / "hosted_room_driver.py"
    spec = importlib.util.spec_from_file_location("hrd_live_p2", path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


live = load_live()
from persistence.gateway.hosted_rooms import (  # noqa: E402
    DriverLease, HostedTaskIdentity, InvalidTaskTransitionError, RoomUnavailableError, StaleTaskError)

WANTED = {
    "info_active", "info_is_active_for", "peer_stop_acknowledged", "interrupt_ack",
    "discard_guard", "discard_outcome", "stop_outcome", "past_execution_deadline",
    "defer_unconfirmed_guard", "deadline_id", "reconcile_deadline", "retry_route",
}


class Seam(BaseException):
    """Raised by a stubbed seam to prove the code reached it."""


class FakeTransport:
    """The member route, reduced to the observations a fixture fixes."""

    def __init__(self, info=None, session_id="s-1", discard_result=None, interrupt_result=None,
                 served_elsewhere=False, with_discard=True):
        self.info_payload = dict(info or {})
        self.session_id = session_id
        self.discard_result = discard_result
        self.interrupt_result = interrupt_result
        self.served_elsewhere = served_elsewhere
        self.accepts_attachments = False
        if not with_discard:
            self.discard = None

    def resolve_exact(self, **kw):
        return None if self.session_id is None else {"session_id": self.session_id}

    def create(self, **kw):
        return {"session_id": self.session_id or "created-session"}

    def resume(self, **kw):
        return {"session_id": self.session_id}

    def info(self, **kw):
        return dict(self.info_payload)

    def history(self, **kw):
        return []

    def discard(self, **kw):
        return self.discard_result

    def interrupt(self, **kw):
        return self.interrupt_result


def identity(raw=None, task_id="task-1"):
    raw = raw if isinstance(raw, dict) else {}
    return HostedTaskIdentity(
        room_id=str(raw.get("room_id") or "room-1"),
        task_id=str(raw.get("task_id") or task_id),
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


def bare(**attrs):
    r = live.HostedRoomRuntime.__new__(live.HostedRoomRuntime)
    r._status_lock = threading.Lock()
    r._leases = {}
    r._ambiguous_rooms = {}
    r._unavailable_route_retries = {}
    r._inspected_indeterminate_attempts = set()
    r._recovered_leases = set()
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
    r._tasks = lambda binding, status: []
    r.clock = lambda: 0.0
    r._rooms_provider = lambda: []
    for key, value in attrs.items():
        setattr(r, key, value)
    return r


def binding_for(room_id="room-1"):
    return live.HostedRoomBinding(room_id=room_id, gateway_id="gw", authority_epoch=1)


def lease_for(task):
    return DriverLease(room_id="room-1", gateway_id="gw", authority_epoch=1,
                       process_generation="pg", lease_generation=1, expires_at=100.0)


# ------------------------------------------------------------------ handlers

def h_info_active(req):
    return live._info_active(req.get("info"))


def h_info_is_active_for(req):
    return live._info_is_active_for(req.get("info"), identity(task_id=req["task_id"]),
                                    require_exact=bool(req.get("require_exact")))


def h_peer_stop_acknowledged(req):
    task = task_of(req.get("task"))
    fake = FakeTransport(info=req.get("info"),
                         session_id="s-1" if req.get("has_session") else None)
    runtime = bare(transport_resolver=lambda binding, task: fake)
    return runtime._peer_stop_acknowledged(binding_for(task["identity"].room_id), task)


def h_interrupt_ack(req):
    task = task_of(req.get("task"))
    info = dict(req.get("info") or {})
    fake = FakeTransport(info=info, interrupt_result=req.get("result"))
    runtime = bare(rpc=fake)
    if not live._info_is_active_for(info, task["identity"], require_exact=True):
        raise AssertionError("interrupt_ack fixture must describe an active exact turn")
    return runtime._stop_outcome(binding_for(task["identity"].room_id), task) is True


def h_discard_guard(req):
    task = task_of(req.get("task"))
    info = dict(req.get("info") or {})
    fake = FakeTransport(info=info, discard_result=req.get("discard_result"))
    result = live.HostedRoomRuntime._discard_unknown_stop(fake, "profile-a", "s-1", task, info)
    return result is not False


def h_discard_outcome(req):
    task = task_of(req.get("task"))
    info = dict(req.get("info") or {})
    if not (live._info_active(info) is False and info.get("task_id") == task["identity"].task_id):
        raise AssertionError("discard_outcome fixture must describe the exact lost attempt")
    fake = FakeTransport(info=info, discard_result=req.get("result"))
    result = live.HostedRoomRuntime._discard_unknown_stop(fake, "profile-a", "s-1", task, info)
    if result is False:
        return {"usable": False, "outcome": "none", "receipt": None}
    if result is True:
        return {"usable": True, "outcome": "cancelled", "receipt": None}
    return {"usable": True, "outcome": "receipt", "receipt": asdict(result)}


def h_stop_outcome(req):
    spec = req.get("spec") or {}
    task = task_of(spec.get("task"))
    local = bool(spec.get("local"))
    fake = FakeTransport(info=spec.get("info"),
                         session_id="s-1" if spec.get("has_session") else None,
                         discard_result=spec.get("discard_result"),
                         interrupt_result=spec.get("interrupt_result"),
                         with_discard=bool(spec.get("has_discard")))
    runtime = bare(rpc=fake if local else None,
                   transport_resolver=None if local else (lambda binding, task: fake))
    outcome = runtime._stop_outcome(binding_for(task["identity"].room_id), task)
    if outcome is True:
        return {"outcome": "ack", "receipt": None}
    if outcome is False:
        return {"outcome": "nack", "receipt": None}
    return {"outcome": "receipt", "receipt": asdict(outcome)}


def h_past_execution_deadline(req):
    task = task_of(req.get("task"))
    runtime = bare(clock=lambda: float(req["now"]))
    return runtime._past_execution_deadline(task)


def h_defer_unconfirmed_guard(req):
    task = task_of(req.get("task"))
    now = float(req["now"])
    fake = FakeTransport(info={}, served_elsewhere=bool(req.get("served_elsewhere")))
    runtime = bare(transport_resolver=lambda binding, task: fake, clock=lambda: now)
    runtime.indeterminate_defer_seconds = float(req.get("defer_seconds") or 0.0)
    runtime._ensure_lease = lambda binding: (_ for _ in ()).throw(Seam())
    try:
        runtime._defer_unconfirmed_stop(binding_for(task["identity"].room_id), task, lease_for(task))
    except Seam:
        return True
    return False


def h_deadline_id(req):
    task = task_of(req.get("task"))
    captured = {}
    original = live.state.begin_task_cancel

    def fake_begin(store, task_identity, **kw):
        captured.update(kw)
        raise Seam()

    live.state.begin_task_cancel = fake_begin
    try:
        runtime = bare(clock=lambda: 0.0)
        runtime._expire_attempt_deadline(binding_for(task["identity"].room_id), task, lease_for(task))
    except Seam:
        pass
    finally:
        live.state.begin_task_cancel = original
    if "cancel_id" not in captured:
        raise AssertionError("deadline id fixture needs a running/indeterminate task")
    return captured["cancel_id"]


def h_reconcile_deadline(req):
    # Expression mirror of _reconcile_indeterminate's deadline (single inline
    # expression: no seam-free callable exists to call).
    task = task_of(req.get("task"))
    now = float(req["now"])
    defer = float(req.get("defer_seconds") or 0.0)
    base = (task.get("indeterminate_at") or task.get("updated_at") or task.get("created_at") or now)
    return defer + float(base)


def h_retry_route(req):
    task = task_of(req.get("task"))
    requested = req.get("requested_generation")
    has_binding = bool(req.get("has_binding"))
    original = live.state.get_task
    live.state.get_task = lambda *args, **kw: dict(task)
    try:
        runtime = bare()
        runtime._rooms_provider = (lambda: [binding_for(task["identity"].room_id)]) if has_binding else (lambda: [])
        runtime._ensure_lease = lambda binding: (_ for _ in ()).throw(Seam())
        try:
            runtime.retry_indeterminate(task["identity"], execution_generation=requested)
        except Seam:
            return task["status"]
        except StaleTaskError:
            return "stale"
        except InvalidTaskTransitionError:
            return "invalid"
        except RoomUnavailableError:
            return "unavailable"
        raise AssertionError("retry routed nowhere")
    finally:
        live.state.get_task = original


HANDLERS = {
    "info_active": h_info_active,
    "info_is_active_for": h_info_is_active_for,
    "peer_stop_acknowledged": h_peer_stop_acknowledged,
    "interrupt_ack": h_interrupt_ack,
    "discard_guard": h_discard_guard,
    "discard_outcome": h_discard_outcome,
    "stop_outcome": h_stop_outcome,
    "past_execution_deadline": h_past_execution_deadline,
    "defer_unconfirmed_guard": h_defer_unconfirmed_guard,
    "deadline_id": h_deadline_id,
    "reconcile_deadline": h_reconcile_deadline,
    "retry_route": h_retry_route,
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
