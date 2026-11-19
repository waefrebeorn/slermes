#!/usr/bin/env python3
"""Oracle for the tui_gateway/hosted_room_driver.py FLOW cores + leaves
(src/gateway/hosted_room_driver_flow.c).

AST-extracts the 42 target functions (12 module leaves + 30 HostedRoomRuntime
methods) plus the module constants and the _TerminalReceipt/_RecoveryInspection
dataclasses, seeds faithful doubles for the DriverStateError exception family,
and replaces every collaborator (the claimed gateway.hosted_room_driver state
siblings, the InternalSessionRPC transport methods, the injected callbacks,
the clock) with recording stubs returning the SAME scripted results the C
harness feeds its universal seam. Ported siblings run INLINE (real extracted
defs bound onto a SimpleNamespace self double); only unported/claimed
collaborators are seamed.

State is injected as a JSON mirror (cfg / cb presence flags / leases /
blocked / ambiguous / route_retries / recovered / inspected / last_error /
room_threads / reschedule / wake+stop flags); the oracle materializes the
runtime double, lets live mutate it, then norms the final state back to JSON.
Compares ordered seam trace + return + final state. BaseException is caught
per case; scripted raises round-trip bare (identity-matched to the Raise
instance the script handed out). Transport identity (`transport is self.rpc`)
is modeled by a per-id object registry, mirrored by id comparison in C.

Input: stdin JSON lines from t_port_hosted_room_driver_flow.
Summary: ORACLE hosted_room_driver_flow: N cases, M mismatches. Fail-closed on 0.
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

# Scrubbed-env runner: derive home from __file__ (repo = parent.parent, home = parent).
_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "hrd_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "tui_gateway", "hosted_room_driver.py")

LEAF_TARGETS = [
    "_session_kw", "_fences", "room_session_title", "_member_id", "_session_id",
    "_truncate_utf8", "_bounded_terminal_result", "_receipt_from",
    "discarded_receipt", "_find_terminal_receipt", "_info_active",
    "_info_is_active_for",
]
METHOD_TARGETS = [
    "_remote", "_publish", "_set_blocked", "_record_error", "_record_task_error",
    "_binding_for_room", "_transport_for", "_tasks", "_defer_unavailable_route",
    "_past_execution_deadline", "_cache_lease", "_drop_lease", "_mark_ambiguous",
    "_renew_lease_if_needed", "_ensure_lease", "_release_idle_leases",
    "_fenced", "_complete_cancel", "_requeue", "_resolve_indeterminate",
    "_resolve_or_create", "_resume_exact", "_open_session",
    "_terminal_from_history", "_peer_stop_acknowledged", "_report_pending_action",
    "_served_elsewhere", "_defer_unconfirmed_stop", "_complete_acknowledged_stop",
    "_settle_failure_if_current",
]
TARGETS = LEAF_TARGETS + METHOD_TARGETS
STATIC_METHODS = {"_terminal_from_history"}
HELPER_METHODS = ["wakeup"]   # bound real, not a claimed target (C: inline state step)
STATE_FUNCS = [
    "get_task", "list_tasks", "complete_task_cancel", "settle_task",
    "acquire_lease", "renew_lease", "release_lease", "cancel_task",
    "begin_task_cancel", "settle_stopping_task", "resolve_indeterminate_task",
    "resolve_indeterminate_cancellation", "requeue_indeterminate_task",
    "requeue_deferred_task", "requeue_not_admitted_task", "reopen_deferred_task",
    "defer_unconfirmed_stop", "defer_indeterminate_task", "start_task",
    "mark_attempt_indeterminate", "recover_room",
]

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
        # str(KeyError(k)) == repr(k); the spec message IS repr(k) -> recover k.
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
        self._discard_present = discard
        for nm in ("resolve_exact", "create", "resume", "history", "info",
                   "interrupt", "discard"):
            setattr(self, nm, self._mk(nm))

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


# ── projections (seam args) and materialization (fixture -> live doubles) ────
def proj_identity(x):
    return dict(vars(x)) if isinstance(x, SimpleNamespace) else x


def proj_binding(x):
    return dict(vars(x)) if isinstance(x, SimpleNamespace) else x


def proj_lease(x):
    return dict(vars(x)) if isinstance(x, SimpleNamespace) else x


def proj_task(x):
    if not isinstance(x, dict):
        return x
    return {k: (proj_identity(v) if k == "identity" else v) for k, v in x.items()}


def proj_attempt(x):
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


def mat_attempt(d):
    if not isinstance(d, dict):
        return d
    d = copy.deepcopy(d)
    if isinstance(d.get("identity"), dict):
        d["identity"] = SimpleNamespace(**d["identity"])
    if isinstance(d.get("lease"), dict):
        d["lease"] = mat_lease(d["lease"])
    return SimpleNamespace(**d)
# ── extraction ───────────────────────────────────────────────────────────────
def extract():
    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("hosted_room_driver_flow_extracted")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    ns.update({
        "json": json, "contextlib": contextlib, "copy": copy,
        "asdict": dataclasses.asdict, "dataclass": dataclasses.dataclass,
        "field": dataclasses.field, "suppress": contextlib.suppress,
        "cast": typing.cast, "Any": typing.Any, "Callable": typing.Callable,
        "TerminalStatus": str,
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
    # module constants + receipt dataclasses in SOURCE order (_NO_INSPECTION
    # needs _RecoveryInspection exec'd first)
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
                if isinstance(m, ast.FunctionDef) and m.name in METHOD_TARGETS + HELPER_METHODS:
                    m2 = copy.copy(m)
                    m2.decorator_list = []   # strip @staticmethod wrappers
                    exec(compile(ast.Module(body=[m2], type_ignores=[]), SRC, "exec", flags=flags), ns)
    for name in TARGETS + HELPER_METHODS:
        assert name in ns, "extraction missed %s" % name
    for name in ("_STOP_ACK_STATUSES", "ROOM_SESSION_SOURCE", "MAX_TERMINAL_TEXT_BYTES",
                 "_TERMINAL_TRUNCATION_NOTICE", "_TerminalReceipt", "_NO_INSPECTION"):
        assert name in ns, "constant missed %s" % name
    # the claimed state siblings: one recording double for the whole module
    ns["state"] = SimpleNamespace(**{nm: mk_state(nm) for nm in STATE_FUNCS})
    return ns


# ── state sibling recorders (claimed gateway.hosted_room_driver functions) ───
def mk_state(name):
    def f(store, target=None, lease=None, **kw):
        if name == "list_tasks":
            args = {"placement": kw.get("placement"), "room_id": kw.get("room_id"),
                    "status": kw.get("status")}
        elif name == "get_task":
            args = {"identity": proj_identity(target), "placement": kw.get("placement")}
        elif name == "complete_task_cancel":
            args = {"identity": proj_identity(target), "placement": kw.get("placement"),
                    "cancel_id": kw.get("cancel_id"),
                    "expected_cancel_generation": kw.get("expected_cancel_generation")}
        elif name == "settle_task":
            args = {"attempt": proj_attempt(target), "placement": kw.get("placement"),
                    "settlement_id": kw.get("settlement_id"), "status": kw.get("status"),
                    "result": kw.get("result")}
        elif name == "acquire_lease":
            args = {"placement": kw.get("placement"), "room_id": kw.get("room_id"),
                    "gateway_id": kw.get("gateway_id"),
                    "authority_epoch": kw.get("authority_epoch"),
                    "process_generation": kw.get("process_generation"),
                    "ttl_seconds": kw.get("ttl_seconds")}
        elif name in ("renew_lease", "release_lease"):
            args = {"lease": proj_lease(target), "placement": kw.get("placement")}
            if name == "renew_lease":
                args["ttl_seconds"] = kw.get("ttl_seconds")
        else:
            # fenced ops: op(store, identity, lease, placement=, fences+extra, clock=)
            args = {"identity": proj_identity(target), "lease": proj_lease(lease),
                    "placement": kw.get("placement")}
            for k, v in kw.items():
                if k in ("placement", "clock"):
                    continue
                args[k] = v
        r = seam("state." + name, args)
        if name in ("acquire_lease", "renew_lease") and isinstance(r, dict):
            return mat_lease(r)
        if name == "list_tasks" and isinstance(r, list):
            return [mat_task(t) for t in r]
        return r
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

    def tresolver(binding, task):
        r = seam("transport_resolver", {"binding": proj_binding(binding), "task": proj_task(task)})
        return transport_double(r) if isinstance(r, dict) else None
    sd.transport_resolver = tresolver if cb["transport_resolver"] else None
    sd.remote_member = (lambda room_id, member_id: seam(
        "remote_member", {"room_id": room_id, "member_id": member_id})) if cb["remote_member"] else None
    sd.publish_terminal = (lambda binding, task: seam(
        "publish_terminal", {"binding": proj_binding(binding), "task": proj_task(task)})) if cb["publish_terminal"] else None
    sd.pending_action = (lambda room_id, member_id, action: seam(
        "pending_action", {"room_id": room_id, "member_id": member_id, "action": action})) if cb["pending_action"] else None
    sd.task_progress = (lambda room_id, task_id, gen, info: seam(
        "task_progress", {"room_id": room_id, "task_id": task_id,
                          "execution_generation": gen, "info": info})) if cb["task_progress"] else None
    sd.retire_task_progress = (lambda room_id, task_id, gen: seam(
        "retire_task_progress", {"room_id": room_id, "task_id": task_id,
                                 "execution_generation": gen})) if cb["retire_task_progress"] else None
    sd.confirm_deferred_stop = None
    sd.prepare_room = None
    sd.maintain = None
    sd.turn_lock = None

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
    for nm in METHOD_TARGETS + HELPER_METHODS:
        if nm in STATIC_METHODS:
            setattr(sd, nm, ns[nm])
        else:
            setattr(sd, nm, types.MethodType(ns[nm], sd))
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
    if func in LEAF_TARGETS:
        f = ns[func]
        if func == "_session_kw":
            return f(g("profile"), g("session_id"))
        if func == "_fences":
            return f(mat_task(g("task")))
        if func == "room_session_title":
            return f(g("room_id"))
        if func == "_member_id":
            return f(mat_task(g("task")))
        if func == "_session_id":
            return f(g("session"))
        if func == "_truncate_utf8":
            return f(g("value"), max_bytes=g("max_bytes"))
        if func == "_bounded_terminal_result":
            return f(g("receipt"))
        if func == "_receipt_from":
            return f(g("receipt"), mat_ident(g("identity")), g("execution_generation"))
        if func == "discarded_receipt":
            return f(g("result"), mat_task(g("task")))
        if func == "_find_terminal_receipt":
            return f(g("history"), mat_ident(g("identity")), g("execution_generation"))
        if func == "_info_active":
            return f(g("info"))
        if func == "_info_is_active_for":
            return f(g("info"), mat_ident(g("identity")), require_exact=g("require_exact", False))
    f = getattr(sd, func)
    if func == "_remote":
        return f(mat_task(g("task")))
    if func == "_publish":
        return f(mat_binding(g("binding")), mat_task(g("task")))
    if func == "_set_blocked":
        return f(g("room_id"), g("blocked"))
    if func == "_record_error":
        return f(g("message"))
    if func == "_record_task_error":
        return f(mat_attempt(g("attempt")), g("message"))
    if func == "_binding_for_room":
        return f(g("room_id"))
    if func == "_transport_for":
        return f(mat_binding(g("binding")), mat_task(g("task")))
    if func == "_tasks":
        return f(mat_binding(g("binding")), g("status"))
    if func == "_defer_unavailable_route":
        return f(mat_task(g("task")))
    if func == "_past_execution_deadline":
        return f(mat_task(g("task")))
    if func == "_cache_lease":
        return f(mat_lease(g("lease")))
    if func == "_drop_lease":
        return f(g("room_id"))
    if func == "_mark_ambiguous":
        return f(mat_binding(g("binding")), mat_attempt(g("attempt")))
    if func == "_renew_lease_if_needed":
        return f(mat_lease(g("lease")), force=g("force", False))
    if func == "_ensure_lease":
        return f(mat_binding(g("binding")))
    if func == "_release_idle_leases":
        return f()
    if func == "_fenced":
        op = getattr(ns["state"], g("op"))
        return f(op, mat_binding(g("binding")), mat_task(g("task")), mat_lease(g("lease")),
                 publish=g("publish", True), **(g("extra") or {}))
    if func == "_complete_cancel":
        return f(mat_task(g("task")), cancel_id=g("cancel_id"))
    if func == "_requeue":
        rq = getattr(ns["state"], g("requeue"))
        return f(rq, mat_task(g("task")), mat_lease(g("lease")), g("room_id"))
    if func == "_resolve_indeterminate":
        terminal = ns["_TerminalReceipt"](**copy.deepcopy(g("terminal")))
        return f(mat_binding(g("binding")), mat_task(g("task")), mat_lease(g("lease")),
                 terminal, publish=g("publish", True))
    if func == "_resolve_or_create":
        return f(transport_double(g("transport")), g("profile"), g("room_id"),
                 create=g("create", True))
    if func == "_resume_exact":
        return f(transport_double(g("transport")), g("room_id"), g("profile"))
    if func == "_open_session":
        return f(mat_binding(g("binding")), mat_task(g("task")), peer_only=g("peer_only", False))
    if func == "_terminal_from_history":
        return f(transport_double(g("transport")), g("profile"), g("session_id"),
                 mat_task(g("task")))
    if func == "_peer_stop_acknowledged":
        return f(mat_binding(g("binding")), mat_task(g("task")))
    if func == "_report_pending_action":
        return f(mat_task(g("task")), session_id=g("session_id"), info=g("info"))
    if func == "_served_elsewhere":
        return f(mat_binding(g("binding")), mat_task(g("task")))
    if func == "_defer_unconfirmed_stop":
        return f(mat_binding(g("binding")), mat_task(g("task")), mat_lease(g("lease")))
    if func == "_complete_acknowledged_stop":
        return f(mat_binding(g("binding")), mat_task(g("task")), mat_lease(g("lease")))
    if func == "_settle_failure_if_current":
        return f(mat_attempt(g("attempt")), make_exc(g("exc")))
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
    print("ORACLE hosted_room_driver_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
