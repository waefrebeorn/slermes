#!/usr/bin/env python3
"""Oracle for the tui_gateway/hosted_room_driver.py UNCLAIMED stop/cancel/recovery
remainder (src/gateway/hosted_room_driver_flow2.c).

AST-extracts the 18 target methods plus the pure module leaves (run REAL, no PoP
re-claim) and the _TerminalReceipt/_RecoveryInspection dataclasses, seeds faithful
DriverStateError-family exception doubles and TaskAttempt/DriverLease dataclasses,
and replaces every CLAIMED sibling (the 42 hosted_room_driver_flow ports) with a
recording "self.<name>" seam stub returning the SAME scripted results the C harness
feeds its universal seam. Ported targets run INLINE (real extracted defs bound onto
a SimpleNamespace self double) and compose with each other; the pure leaves run
real as module globals. State ops, transport methods, injected callbacks, the clock
and the turn_lock context manager are seamed exactly as in flow1.

State is injected as a JSON mirror (same shape as flow1); the oracle materializes
the runtime double, lets live mutate it (only wakeup + the inline inspectors touch
it — seamed siblings abstract their effects), then norms the final state back to
JSON. Compares ordered seam trace + return + final state. BaseException is caught
per case; scripted raises round-trip bare (identity-matched to the Raise instance).
Transport identity (`transport is self.rpc`) is modeled by a per-id object registry,
mirrored by id comparison in C; `callable(getattr(transport,"discard",None))` reads
the discard presence flag.

Input: stdin JSON lines from t_port_hosted_room_driver_flow2.
Summary: ORACLE hosted_room_driver_flow2: N cases, M mismatches. Fail-closed on 0.
"""
import ast
import asyncio
import builtins
import contextlib
import copy
import dataclasses
import json
import os
import sys
import types
import typing
from pathlib import Path
from types import SimpleNamespace

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "hrd2_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "tui_gateway", "hosted_room_driver.py")

# 18 UNCLAIMED targets (stop cluster + public ops + recovery).
METHOD_TARGETS = [
    "_stop_outcome", "_interrupt_stale_admission", "_interrupt_stopping_task",
    "_stop_or_settle", "_finish_stop", "_settle_stopping_completion",
    "_retry_stopping_tasks", "cancel", "retry_indeterminate", "_retry_deferred",
    "deferred_stop_confirmed", "_expire_attempt_deadline", "_inspect_session",
    "_inspect_recovery_session", "_inspect_local_recovery_session",
    "_inspect_abandoned_attempts", "_harvest_previous_attempt",
]
STATIC_METHODS = {"_discard_unknown_stop"}
TARGETS = METHOD_TARGETS + list(STATIC_METHODS)
# pure module leaves: run REAL on both sides (C inlines them, no PoP re-claim).
LEAF_TARGETS = [
    "_session_kw", "_fences", "room_session_title", "_member_id", "_session_id",
    "_truncate_utf8", "_bounded_terminal_result", "_receipt_from",
    "discarded_receipt", "_find_terminal_receipt", "_info_active",
    "_info_is_active_for",
]
HELPER_METHODS = ["wakeup"]   # bound real, inline state step (unclaimed)
# CLAIMED siblings (hosted_room_driver_flow ports) -> seamed "self.<name>".
SEAMED_SIBLINGS = [
    "_open_session", "_terminal_from_history", "_fenced", "_complete_cancel",
    "_peer_stop_acknowledged", "_binding_for_room", "_ensure_lease",
    "_renew_lease_if_needed", "_record_error", "_set_blocked", "_tasks",
    "_remote", "_transport_for", "_resolve_indeterminate", "_requeue",
    "_defer_unconfirmed_stop", "_complete_acknowledged_stop", "_served_elsewhere",
    "_resume_exact", "_report_pending_action",
]
STATE_FUNCS = ["get_task", "cancel_task", "begin_task_cancel", "settle_task",
               "resolve_indeterminate_cancellation", "requeue_indeterminate_task",
               "requeue_deferred_task", "reopen_deferred_task", "settle_stopping_task",
               "resolve_indeterminate_task"]

CFG_DEFAULTS = {
    "lease_ttl_seconds": 30.0, "poll_interval_seconds": 5.0,
    "active_poll_interval_seconds": 0.25, "turn_timeout_seconds": 1830.0,
    "indeterminate_defer_seconds": 60.0, "unavailable_retry_min_seconds": 1.0,
    "unavailable_retry_max_seconds": 30.0, "process_generation": "pg-test",
}
CB_DEFAULTS = {
    "remote_member": False, "confirm_deferred_stop": False,
    "publish_terminal": False, "pending_action": False, "task_progress": False,
    "retire_task_progress": False, "transport_resolver": False, "rpc": True,
    "prepare_room": False, "maintain": False,
}

CUR = {"func": None, "in": {}, "trace": [], "io": {}, "pos": {}, "raises": [],
       "transports": {}, "self": None, "cfg": {}, "cb": {}, "placement": "shared",
       "rpc_id": "rpc"}


def rec(ev):
    CUR["trace"].append(ev)


# ── faithful exception doubles (live MRO matters for except/suppress) ────────
class PersistenceError(Exception):
    pass


class NotFound(PersistenceError):
    pass


class Conflict(PersistenceError):
    pass


class DriverStateError(PersistenceError, ValueError):
    pass


class DriverValidationError(DriverStateError):
    pass


class RoomUnavailableError(DriverStateError, NotFound):
    pass


class LeaseHeldError(DriverStateError, Conflict):
    pass


class StaleLeaseError(DriverStateError, Conflict):
    pass


class StaleTaskError(DriverStateError, Conflict):
    pass


class InvalidTaskTransitionError(DriverStateError, Conflict):
    pass


class TaskConflictError(DriverStateError, Conflict):
    pass


DRIVER_EXC = {
    "DriverStateError": DriverStateError, "DriverValidationError": DriverValidationError,
    "RoomUnavailableError": RoomUnavailableError, "LeaseHeldError": LeaseHeldError,
    "StaleLeaseError": StaleLeaseError, "StaleTaskError": StaleTaskError,
    "InvalidTaskTransitionError": InvalidTaskTransitionError,
    "TaskConflictError": TaskConflictError,
}


def make_exc(spec):
    kind = spec.get("__raise__", "Exception")
    msg = spec.get("message", "")
    if kind in DRIVER_EXC:
        return DRIVER_EXC[kind](msg)
    if kind == "CancelledError":
        return asyncio.CancelledError(msg)
    if kind == "KeyError":
        try:
            key = ast.literal_eval(msg)
        except Exception:
            key = msg
        return KeyError(key)
    cls = getattr(builtins, kind, None)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        return cls(msg)
    return type(kind, (Exception,), {})(msg)


class Raise:
    def __init__(self, spec):
        self.spec = spec
        self.exc = make_exc(spec)


# ── script access (mirrors the harness stub semantics, fail-closed) ──────────
def next_script(name):
    io = CUR["io"]
    if name not in io:
        return Raise({"__raise__": "RuntimeError", "message": "no script for " + name})
    v = io[name]
    if isinstance(v, list):
        i = CUR["pos"].get(name, 0)
        if i >= len(v):
            return Raise({"__raise__": "RuntimeError", "message": "script exhausted for " + name})
        CUR["pos"][name] = i + 1
        return v[i]
    if isinstance(v, dict) and v.get("__script__"):
        return v.get("ret")
    return v


def seam(name, args):
    r = next_script(name)
    if isinstance(r, Raise):
        CUR["raises"].append(r)
        rec({"ev": name, "args": args, "ret": r.spec})
        raise r.exc
    rec({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        e = make_exc(r)
        CUR["raises"].append(Raise(r))
        raise e
    return copy.deepcopy(r) if isinstance(r, (dict, list)) else r


# ── transport doubles (per-id registry so `transport is self.rpc` agrees) ────
class TransportDouble:
    def __init__(self, tid, served_elsewhere=False, discard=False):
        self.id = tid
        self.served_elsewhere = served_elsewhere
        for nm in ("resolve_exact", "create", "resume", "history", "info", "interrupt"):
            setattr(self, nm, self._mk(nm))
        if discard:   # callable(getattr(transport, "discard", None)) reflects this
            setattr(self, "discard", self._mk("discard"))

    def _mk(self, nm):
        def m(**kw):
            args = {"id": self.id}
            args.update(kw)
            return seam("transport." + nm, args)
        return m


def transport_double(d):
    if not isinstance(d, dict):
        return None
    tid = d.get("id")
    reg = CUR["transports"]
    if tid not in reg:
        reg[tid] = TransportDouble(tid, bool(d.get("served_elsewhere", False)),
                                   bool(d.get("discard", False)))
    return reg[tid]


class NoOpLock:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class TurnLockDouble:
    """self.turn_lock(profile) CM: inert unless the handle carries __exit__ raise."""
    def __init__(self, handle):
        self.handle = handle

    def __enter__(self):
        return self

    def __exit__(self, *a):
        h = self.handle
        if isinstance(h, dict) and isinstance(h.get("__exit__"), dict) and "__raise__" in h["__exit__"]:
            spec = h["__exit__"]
            e = make_exc(spec)
            CUR["raises"].append(Raise(spec))
            raise e
        return False


class EventDouble:
    def __init__(self, flag=False):
        self._flag = bool(flag)

    def is_set(self):
        return self._flag

    def set(self):
        self._flag = True

    def clear(self):
        self._flag = False

    def wait(self, timeout=None):
        return self._flag


# ── faithful TaskAttempt / DriverLease dataclasses (no slots: vars() projection) ─
@dataclasses.dataclass(frozen=True)
class DriverLease:
    room_id: str
    gateway_id: str
    authority_epoch: int
    process_generation: str
    lease_generation: int
    expires_at: float
    reclaimed: bool = False


@dataclasses.dataclass(frozen=True)
class TaskAttempt:
    identity: object
    lease: object
    execution_generation: int
    cancel_generation: int


# ── projections (seam args) and materialization (fixture -> live doubles) ────
def proj_identity(x):
    if dataclasses.is_dataclass(x) and not isinstance(x, type):
        return dataclasses.asdict(x)
    return dict(vars(x)) if isinstance(x, SimpleNamespace) else x


def proj_binding(x):
    return dict(vars(x)) if isinstance(x, SimpleNamespace) else x


def proj_lease(x):
    if dataclasses.is_dataclass(x) and not isinstance(x, type):
        return dataclasses.asdict(x)
    return dict(vars(x)) if isinstance(x, SimpleNamespace) else x


def proj_transport(x):
    if isinstance(x, TransportDouble):
        return {"id": x.id}
    return x


def proj_task(x):
    if not isinstance(x, dict):
        return x
    return {k: (proj_identity(v) if k == "identity" else v) for k, v in x.items()}


def proj_attempt(x):
    if dataclasses.is_dataclass(x) and not isinstance(x, type):
        return {"identity": proj_identity(x.identity), "lease": proj_lease(x.lease),
                "execution_generation": x.execution_generation,
                "cancel_generation": x.cancel_generation}
    if isinstance(x, SimpleNamespace):
        return {"identity": proj_identity(x.identity), "lease": proj_lease(x.lease),
                "execution_generation": x.execution_generation,
                "cancel_generation": x.cancel_generation}
    return x


def mat_ident(d):
    return SimpleNamespace(**copy.deepcopy(d)) if isinstance(d, dict) else d


def mat_binding(d):
    return SimpleNamespace(**copy.deepcopy(d)) if isinstance(d, dict) else d


def mat_lease(d):
    if not isinstance(d, dict):
        return d
    d = copy.deepcopy(d)
    d.setdefault("reclaimed", False)
    return SimpleNamespace(**d)


def mat_task(d):
    if not isinstance(d, dict):
        return d
    d = copy.deepcopy(d)
    if isinstance(d.get("identity"), dict):
        d["identity"] = SimpleNamespace(**d["identity"])
    return d


def mat_receipt(d):
    if not isinstance(d, dict):
        return d
    return CUR["ns"]["_TerminalReceipt"](**copy.deepcopy(d))


# ── extraction ───────────────────────────────────────────────────────────────
def extract():
    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("hosted_room_driver_flow2_extracted")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    ns.update({
        "json": json, "contextlib": contextlib, "copy": copy,
        "asdict": dataclasses.asdict, "dataclass": dataclasses.dataclass,
        "field": dataclasses.field, "suppress": contextlib.suppress,
        "cast": typing.cast, "Any": typing.Any, "Callable": typing.Callable,
        "TerminalStatus": str, "TaskAttempt": TaskAttempt, "DriverLease": DriverLease,
        "HostedTaskIdentity": object, "HostedRoomBinding": object,
        "TERMINAL_STATUSES": frozenset({"settled", "failed", "cancelled"}),
        "Iterable": typing.Iterable, "Mapping": typing.Mapping,
        "Sequence": typing.Sequence, "ContextManager": typing.ContextManager,
        "Protocol": typing.Protocol, "TYPE_CHECKING": False,
        "InternalSessionRPC": object,
        "logging": SimpleNamespace(getLogger=lambda name: SimpleNamespace()),
        "DriverStateError": DriverStateError,
        "DriverValidationError": DriverValidationError,
        "RoomUnavailableError": RoomUnavailableError, "LeaseHeldError": LeaseHeldError,
        "StaleLeaseError": StaleLeaseError, "StaleTaskError": StaleTaskError,
        "InvalidTaskTransitionError": InvalidTaskTransitionError,
        "TaskConflictError": TaskConflictError,
    })
    tree = ast.parse(src)
    import __future__
    flags = __future__.annotations.compiler_flag
    for node in tree.body:
        if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
            with contextlib.suppress(Exception):
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            with contextlib.suppress(Exception):
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(node, ast.ClassDef) and node.name in ("_TerminalReceipt", "_RecoveryInspection"):
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=flags), ns)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in LEAF_TARGETS:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=flags), ns)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "HostedRoomRuntime":
            for m in node.body:
                if isinstance(m, ast.FunctionDef) and m.name in TARGETS + HELPER_METHODS:
                    m2 = copy.copy(m)
                    m2.decorator_list = []
                    exec(compile(ast.Module(body=[m2], type_ignores=[]), SRC, "exec", flags=flags), ns)
    for name in TARGETS + HELPER_METHODS + LEAF_TARGETS:
        assert name in ns, "extraction missed %s" % name
    for name in ("_STOP_ACK_STATUSES", "_CANCEL_ROUTE_RETRIES", "_STOP_PENDING",
                 "ROOM_SESSION_SOURCE", "MAX_TERMINAL_TEXT_BYTES",
                 "_TERMINAL_TRUNCATION_NOTICE", "_TerminalReceipt", "_RecoveryInspection",
                 "_NO_INSPECTION"):
        assert name in ns, "constant missed %s" % name
    ns["state"] = SimpleNamespace(**{nm: mk_state(nm) for nm in STATE_FUNCS})
    # module-level logger Assign replaced the seeded one; restore a silent logger
    ns["logger"] = SimpleNamespace(getLogger=lambda name: SimpleNamespace(),
                                   warning=lambda *a, **k: None,
                                   exception=lambda *a, **k: None)
    CUR["ns"] = ns
    return ns


# ── state sibling recorders (claimed gateway.hosted_room_driver functions) ───
def mk_state(name):
    def f(store, target=None, lease=None, **kw):
        if name == "get_task":
            args = {"identity": proj_identity(target), "placement": kw.get("placement")}
        elif name in ("cancel_task", "begin_task_cancel"):
            args = {"identity": proj_identity(target), "placement": kw.get("placement"),
                    "cancel_id": kw.get("cancel_id"),
                    "expected_cancel_generation": kw.get("expected_cancel_generation")}
        elif name == "settle_task":
            args = {"attempt": proj_attempt(target), "placement": kw.get("placement"),
                    "settlement_id": kw.get("settlement_id"), "status": kw.get("status"),
                    "result": kw.get("result")}
        else:
            args = {"identity": proj_identity(target), "lease": proj_lease(lease),
                    "placement": kw.get("placement")}
            for k, v in kw.items():
                if k in ("placement", "clock"):
                    continue
                args[k] = v
        r = seam("state." + name, args)
        if name in ("get_task", "cancel_task", "begin_task_cancel") and isinstance(r, dict):
            return mat_task(r)
        return r
    f._seam_name = name
    return f


# ── claimed-sibling seam stubs (self.<name>) ─────────────────────────────────
def _asdict(x):
    return dataclasses.asdict(x) if dataclasses.is_dataclass(x) and not isinstance(x, type) else x


SELF_ARGS = {
    "_open_session": lambda a, k: {"binding": proj_binding(a[0]), "task": proj_task(a[1]),
                                   "peer_only": k.get("peer_only", False)},
    "_terminal_from_history": lambda a, k: {"transport": proj_transport(a[0]), "profile": a[1],
                                            "session_id": a[2], "task": proj_task(a[3])},
    "_fenced": lambda a, k: {"op": a[0]._seam_name, "binding": proj_binding(a[1]) if a[1] is not None else None,
                             "task": proj_task(a[2]), "lease": proj_lease(a[3]),
                             "publish": k.get("publish", True),
                             "extra": {kk: vv for kk, vv in k.items() if kk != "publish"}},
    "_complete_cancel": lambda a, k: {"task": proj_task(a[0]), "cancel_id": k.get("cancel_id")},
    "_peer_stop_acknowledged": lambda a, k: {"binding": proj_binding(a[0]), "task": proj_task(a[1])},
    "_binding_for_room": lambda a, k: {"room_id": a[0]},
    "_ensure_lease": lambda a, k: {"binding": proj_binding(a[0])},
    "_renew_lease_if_needed": lambda a, k: {"lease": proj_lease(a[0]), "force": k.get("force", False)},
    "_record_error": lambda a, k: {"message": a[0]},
    "_set_blocked": lambda a, k: {"room_id": a[0], "blocked": a[1]},
    "_tasks": lambda a, k: {"binding": proj_binding(a[0]), "status": a[1]},
    "_remote": lambda a, k: {"task": proj_task(a[0])},
    "_transport_for": lambda a, k: {"binding": proj_binding(a[0]), "task": proj_task(a[1])},
    "_resolve_indeterminate": lambda a, k: {"binding": proj_binding(a[0]), "task": proj_task(a[1]),
                                            "lease": proj_lease(a[2]), "terminal": _asdict(a[3]),
                                            "publish": k.get("publish", True)},
    "_requeue": lambda a, k: {"requeue": a[0]._seam_name, "task": proj_task(a[1]),
                              "lease": proj_lease(a[2]), "room_id": a[3]},
    "_defer_unconfirmed_stop": lambda a, k: {"binding": proj_binding(a[0]), "task": proj_task(a[1]),
                                             "lease": proj_lease(a[2])},
    "_complete_acknowledged_stop": lambda a, k: {"binding": proj_binding(a[0]), "task": proj_task(a[1]),
                                                 "lease": proj_lease(a[2])},
    "_served_elsewhere": lambda a, k: {"binding": proj_binding(a[0]), "task": proj_task(a[1])},
    "_resume_exact": lambda a, k: {"transport": proj_transport(a[0]), "room_id": a[1], "profile": a[2]},
    "_report_pending_action": lambda a, k: {"task": proj_task(a[0]), "session_id": k.get("session_id"),
                                            "info": k.get("info")},
}
SELF_MAT = {
    "_open_session": lambda r: [transport_double(r[0]), r[1], r[2]] if isinstance(r, list) else r,
    "_terminal_from_history": lambda r: mat_receipt(r),
    "_fenced": lambda r: mat_task(r),
    "_complete_cancel": lambda r: mat_task(r),
    "_peer_stop_acknowledged": lambda r: r,
    "_binding_for_room": lambda r: mat_binding(r),
    "_ensure_lease": lambda r: mat_lease(r),
    "_renew_lease_if_needed": lambda r: mat_lease(r),
    "_record_error": lambda r: None,
    "_set_blocked": lambda r: None,
    "_tasks": lambda r: [mat_task(t) for t in r] if isinstance(r, list) else r,
    "_remote": lambda r: r,
    "_transport_for": lambda r: transport_double(r),
    "_resolve_indeterminate": lambda r: mat_task(r),
    "_requeue": lambda r: mat_task(r),
    "_defer_unconfirmed_stop": lambda r: r,
    "_complete_acknowledged_stop": lambda r: mat_task(r),
    "_served_elsewhere": lambda r: r,
    "_resume_exact": lambda r: r,
    "_report_pending_action": lambda r: None,
}


def mk_self(name):
    def f(*a, **kw):
        args = SELF_ARGS[name](a, kw)
        r = seam("self." + name, args)
        return SELF_MAT[name](r)
    return f


# ── per-case setup ───────────────────────────────────────────────────────────
def setup_case(ns, func, inp):
    CUR["func"] = func
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["io"] = inp.get("io") or {}
    CUR["pos"] = {}
    CUR["raises"] = []
    CUR["transports"] = {}
    stt = inp.get("state") or {}
    cfg = {**CFG_DEFAULTS, **(stt.get("cfg") or {})}
    cb = {**CB_DEFAULTS, **(stt.get("cb") or {})}
    placement = stt.get("placement", "shared")
    rpc_id = stt.get("rpc_id", "rpc")
    CUR["cfg"] = cfg
    CUR["cb"] = cb
    CUR["placement"] = placement
    CUR["rpc_id"] = rpc_id

    sd = SimpleNamespace()
    sd.store = object()
    sd.placement = placement
    sd.process_generation = cfg["process_generation"]
    for k in ("lease_ttl_seconds", "poll_interval_seconds", "active_poll_interval_seconds",
              "turn_timeout_seconds", "indeterminate_defer_seconds",
              "unavailable_retry_min_seconds", "unavailable_retry_max_seconds"):
        setattr(sd, k, float(cfg[k]))
    sd.max_concurrent_rooms = 4
    sd.clock = lambda: seam("clock", {})
    sd.rpc = transport_double({"id": rpc_id}) if cb["rpc"] else None
    sd.turn_lock = lambda profile: TurnLockDouble(seam("turn_lock", {"profile": profile}))
    sd.transport_resolver = None
    sd.remote_member = None
    sd.publish_terminal = None
    sd.pending_action = None
    sd.retire_task_progress = None
    sd.confirm_deferred_stop = (lambda room_id, task: seam(
        "confirm_deferred_stop", {"room_id": room_id, "task": proj_task(task)})) if cb["confirm_deferred_stop"] else None
    sd.task_progress = (lambda room_id, task_id, gen, info: seam(
        "task_progress", {"room_id": room_id, "task_id": task_id,
                          "execution_generation": gen, "info": info})) if cb["task_progress"] else None
    sd.prepare_room = None
    sd.maintain = None

    sd._leases = {rid: mat_lease(l) for rid, l in (stt.get("leases") or {}).items()}
    sd._blocked_rooms = set(stt.get("blocked") or [])
    sd._ambiguous_rooms = dict(stt.get("ambiguous") or {})
    sd._unavailable_route_retries = {
        (r["room_id"], r["member_id"]): {"delay": r["delay"], "next_attempt_at": r["next_attempt_at"]}
        for r in (stt.get("route_retries") or [])}
    sd._recovered_leases = {tuple(k) for k in (stt.get("recovered") or [])}
    sd._inspected_indeterminate_attempts = {tuple(k) for k in (stt.get("inspected") or [])}
    sd._last_error = stt.get("last_error")
    rooms = [mat_binding(b) for b in (stt.get("rooms") or [])]
    sd._rooms_provider = lambda: tuple(rooms)
    sd._room_threads = {rid: object() for rid in (stt.get("room_threads") or [])}
    sd._rooms_needing_reschedule = set(stt.get("reschedule") or [])
    sd._stop = EventDouble(stt.get("stop_set", False))
    sd._wake = EventDouble(stt.get("wake_set", False))
    sd._status_lock = NoOpLock()
    sd._current_tasks = {}
    sd._thread = None
    sd._cycles = 0
    sd._room_schedule_cursor = 0
    # real targets + wakeup helper
    for nm in METHOD_TARGETS + HELPER_METHODS:
        setattr(sd, nm, types.MethodType(ns[nm], sd))
    for nm in STATIC_METHODS:
        setattr(sd, nm, ns[nm])
    # claimed siblings -> seam stubs (plain functions; SimpleNamespace does not bind)
    for nm in SEAMED_SIBLINGS:
        setattr(sd, nm, mk_self(nm))
    CUR["self"] = sd


def norm(x):
    if isinstance(x, TransportDouble):
        return {"id": x.id}
    if dataclasses.is_dataclass(x) and not isinstance(x, type):
        return {f: norm(getattr(x, f)) for f in x.__dataclass_fields__}
    if isinstance(x, SimpleNamespace):
        return {k: norm(v) for k, v in vars(x).items()}
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [norm(v) for v in x]
    return x


def norm_state():
    sd = CUR["self"]
    return {
        "cfg": CUR["cfg"], "cb": CUR["cb"], "placement": CUR["placement"],
        "rpc_id": CUR["rpc_id"],
        "rooms": [norm(b) for b in sd._rooms_provider()],
        "leases": {rid: norm(l) for rid, l in sd._leases.items()},
        "blocked": sorted(sd._blocked_rooms),
        "ambiguous": dict(sd._ambiguous_rooms),
        "route_retries": [
            {"room_id": k[0], "member_id": k[1],
             "delay": v["delay"], "next_attempt_at": v["next_attempt_at"]}
            for k, v in sd._unavailable_route_retries.items()],
        "recovered": sorted([list(k) for k in sd._recovered_leases]),
        "inspected": sorted([list(k) for k in sd._inspected_indeterminate_attempts]),
        "last_error": sd._last_error,
        "room_threads": sorted(sd._room_threads.keys()),
        "reschedule": sorted(sd._rooms_needing_reschedule),
        "wake_set": sd._wake.is_set(), "stop_set": sd._stop.is_set(),
    }


# ── per-func drive (fixture args -> live call) ───────────────────────────────
def drive(ns, func, inp):
    sd = CUR["self"]
    g = inp.get
    if func == "_discard_unknown_stop":
        return ns[func](transport_double(g("transport")), g("profile"), g("session_id"),
                        mat_task(g("task")), g("info"))
    f = getattr(sd, func)
    if func in ("_stop_outcome", "_interrupt_stopping_task"):
        return f(mat_binding(g("binding")), mat_task(g("task")))
    if func == "_interrupt_stale_admission":
        return f(transport_double(g("transport")), mat_binding(g("binding")),
                 g("profile"), g("session_id"), g("info"))
    if func in ("_stop_or_settle", "_finish_stop", "_settle_stopping_completion",
                "_retry_deferred", "_expire_attempt_deadline"):
        return f(mat_binding(g("binding")), mat_task(g("task")), mat_lease(g("lease")))
    if func == "_retry_stopping_tasks":
        return f(mat_binding(g("binding")), mat_lease(g("lease")))
    if func == "cancel":
        return f(mat_ident(g("identity")), cancel_id=g("cancel_id"),
                 acknowledge=g("acknowledge", True))
    if func == "retry_indeterminate":
        return f(mat_ident(g("identity")), execution_generation=g("execution_generation"))
    if func == "deferred_stop_confirmed":
        return f(g("room_id"), mat_task(g("task")))
    if func == "_inspect_session":
        return f(transport_double(g("transport")), mat_task(g("task")), g("session_id"),
                 read_history=g("read_history", False))
    if func == "_inspect_recovery_session":
        return f(mat_binding(g("binding")), mat_task(g("task")))
    if func == "_inspect_local_recovery_session":
        return f(mat_task(g("task")))
    if func == "_inspect_abandoned_attempts":
        return f(mat_binding(g("binding")))
    if func == "_harvest_previous_attempt":
        return f(mat_binding(g("binding")), mat_task(g("task")), mat_receipt(g("receipt")))
    raise ValueError("unknown func: %s" % func)


def run_case(ns, func, inp):
    setup_case(ns, func, inp)
    try:
        ret = drive(ns, func, inp)
        want_ret = norm(ret)
    except BaseException as e:
        want_ret = None
        for r in CUR["raises"]:
            if e is r.exc:
                want_ret = r.spec
                break
        if want_ret is None:
            want_ret = {"__raise__": type(e).__name__, "message": str(e)}
    return {"ret": want_ret, "trace": CUR["trace"], "state": norm_state()}


def canon(x):
    if isinstance(x, dict):
        return ("obj", sorted(((k, canon(v)) for k, v in x.items()), key=lambda kv: kv[0]))
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", x)


def main():
    ns = extract()
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        want = run_case(ns, func, inp)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE hosted_room_driver_flow2: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())