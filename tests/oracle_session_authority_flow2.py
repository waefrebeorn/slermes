#!/usr/bin/env python3
"""Oracle for the session-authority FLOW remainder port
(src/gateway/session_authority_flow2.c).

Drives the LIVE gateway/session_authority.py decision chains with SCRIPTED
seams: every store read/write, restore call, event-stream publish, runtime
worker, owner-identity check, media lease and routing collaborator is a
recording stub fed the same scripts the C harness feeds its universal seam.
Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"} in
"trace") plus the final return ("ret") and the mutated self/world state
("state").

The live defs are AST-extracted from gateway/session_authority.py (module
constants seeded; the SessionAuthority ClassDef exec whole; the LiveSession
`detached` property extracted alone). Claimed siblings ported in
session_authority_flow.c / gateway_inbound_flow.c (admission_row, _receipt,
_handle, authorize, logical_owner, _require_admission_open, _current_owner,
gate_refusal, _runs_own_agent, _publish_pending, _schedule, _settle_cancelled,
_commit_cancel, _admitted, _schedule_admitted, _release_refused_capture,
physical_target) are SEAMED here, never re-ported. Ported targets compose
through the real extracted methods. Support dataclasses/exceptions are the REAL
ones AST-extracted from session_contract / persistence.core.errors /
persistence.authority.admissions / gateway.config / vocabulary. Every exec
namespace is a registered sys.modules module (py3.14 dataclasses _is_type needs
cls.__module__ resolvable). The heavy quarry import chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE session_authority_flow2: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc), ...}
  (AuthorityRefusal adds reason/field/detail).
- Platform members travel as .value strings; agents/transports as id strings
  wrapped in attribute doubles.
- logger calls are silent on both sides; authority_persistence is a silent
  handle lookup; asyncio.to_thread / tracked_write / track_mutation / shield
  run their callable synchronously (tracked_write then runs `then`/`after`);
  locks are no-ops.
- sets (native_waiters, cancel_obligations) normalize to SORTED id arrays;
  waiters as {aid: {"done": bool}}; event_stream.execution is a state dict.
- admit_* seams return dicts; store.admissions.admit returns [row, created];
  check_native_route returns [source, route]; prepare_result returns
  [outcome, result] (CapturedResult(*prepared)). Integral floats excluded.
"""
import ast
import contextlib
import enum
import inspect
import json
import os
import re
import sys
import textwrap
import types
import asyncio as _asyncio
from dataclasses import dataclass, field, asdict, is_dataclass
from pathlib import Path
from typing import Any, Optional, Mapping
from enum import Enum, StrEnum
import __future__ as _fut

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SA_SRC = os.path.join(Q, "gateway", "session_authority.py")
CONTRACT_SRC = os.path.join(Q, "gateway", "session_contract.py")
ERRORS_SRC = os.path.join(Q, "persistence", "core", "errors.py")
ADMISSIONS_SRC = os.path.join(Q, "persistence", "authority", "admissions.py")
CONFIG_SRC = os.path.join(Q, "gateway", "config.py")
VOCAB_SRC = os.path.join(Q, "persistence", "authority", "vocabulary.py")

_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


def seed_constants(src, ns, path):
    for node in ast.parse(src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, path)
            except Exception:
                pass


def class_nodes(src, names):
    return [n for n in ast.parse(src).body
            if isinstance(n, ast.ClassDef) and n.name in names]


def func_nodes(src):
    return [n for n in ast.parse(src).body
            if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef))]


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


# ── support namespace: real contract dataclasses, errors, records, enums ──
def extract_support():
    mod = new_module("sau2_oracle_support")
    ns = mod.__dict__
    ns.update({"dataclass": dataclass, "field": field, "asdict": asdict,
               "Enum": Enum, "StrEnum": StrEnum, "Any": Any, "Optional": Optional,
               "Mapping": Mapping, "contextlib": contextlib})
    csrc = open(CONTRACT_SRC, encoding="utf-8").read()
    cmod = new_module("sau2_oracle_contract")
    cmod.__dict__.update({"dataclass": dataclass, "field": field})
    exec(compile(csrc, CONTRACT_SRC, "exec", flags=_FLAGS, dont_inherit=True), cmod.__dict__)
    for name in ("SessionActor", "SessionRef", "SessionHandle", "AdmissionReceipt",
                 "PendingAdmission", "SubscriptionSnapshot", "Submission",
                 "CANONICAL_GATEWAY_PROTOCOL"):
        ns[name] = getattr(cmod, name)
    esrc = open(ERRORS_SRC, encoding="utf-8").read()
    for node in class_nodes(esrc, ["PersistenceError", "TransientStoreError",
                                   "PermanentStoreError", "StoreUnavailable",
                                   "StoreReplaced", "CorruptStoreError",
                                   "AuthorityRefusal"]):
        exec_node(esrc, node, ns, ERRORS_SRC)
    for node in func_nodes(esrc):
        if node.name == "_one_line":
            exec_node(esrc, node, ns, ERRORS_SRC)
    seed_constants(esrc, ns, ERRORS_SRC)
    asrc = open(ADMISSIONS_SRC, encoding="utf-8").read()
    for node in class_nodes(asrc, ["AdmissionIntent", "AdmissionStatus",
                                   "AdmissionOutcome", "AdmissionRecord",
                                   "ClaimedAdmission", "GateRefusal", "CapturedResult"]):
        exec_node(asrc, node, ns, ADMISSIONS_SRC)
    gsrc = open(CONFIG_SRC, encoding="utf-8").read()
    for node in class_nodes(gsrc, ["Platform"]):
        exec_node(gsrc, node, ns, CONFIG_SRC)
    vsrc = open(VOCAB_SRC, encoding="utf-8").read()
    seed_constants(vsrc, ns, VOCAB_SRC)
    required = ["SessionActor", "SessionRef", "SessionHandle", "AdmissionReceipt",
                "PendingAdmission", "AuthorityRefusal", "GateRefusal", "AdmissionRecord",
                "ClaimedAdmission", "CapturedResult", "Platform", "STEERING", "_one_line",
                "STORE_FAILURES", "CorruptStoreError", "StoreReplaced",
                "CANONICAL_GATEWAY_PROTOCOL"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_session_authority_flow2: support missing: {missing}")
        sys.exit(2)
    return ns


SUP = extract_support()


# ── asyncio double ──
class _NoopLock:
    async def __aenter__(self):
        return self

    async def __aexit__(self, *a):
        return False

    def locked(self):
        return False


async def _to_thread(fn, *a, **k):
    return fn(*a, **k)


async def _shield(x):
    if hasattr(x, "__await__"):
        return await x
    return x


class _EventDouble:
    def __init__(self):
        self._set = False

    def is_set(self):
        return self._set

    def set(self):
        self._set = True

    async def wait(self):
        return True


SA_ASYNCIO = types.SimpleNamespace(
    create_task=None, to_thread=_to_thread, shield=_shield,
    Lock=_NoopLock, Event=_EventDouble, Future=_asyncio.Future,
    CancelledError=_asyncio.CancelledError, Task=_asyncio.Task,
    wait_for=None, sleep=None,
)


# ── session_authority namespace ──
def extract_sa():
    mod = new_module("sau2_oracle_sa")
    ns = mod.__dict__
    import sqlite3
    import threading
    import uuid
    from contextvars import ContextVar
    from functools import partial
    ns.update({"asyncio": SA_ASYNCIO, "contextlib": contextlib, "ContextVar": ContextVar,
               "dataclass": dataclass, "field": field, "asdict": asdict,
               "partial": partial, "sqlite3": sqlite3, "threading": threading,
               "SimpleNamespace": types.SimpleNamespace, "uuid": uuid,
               "json": json, "Any": Any, "Optional": Optional})
    for name in ("CANONICAL_GATEWAY_PROTOCOL", "AdmissionReceipt", "PendingAdmission",
                 "SessionActor", "SessionHandle", "SessionRef", "Submission",
                 "SubscriptionSnapshot", "GateRefusal", "STEERING", "STORE_FAILURES",
                 "AuthorityRefusal", "CorruptStoreError", "StoreReplaced"):
        ns[name] = SUP[name]
    ns["SessionEvents"] = type("SessionEvents", (), {})
    ns["PendingControls"] = type("PendingControls", (), {})
    ns["authority_persistence"] = lambda a: None
    ns["drop_producer_turn"] = lambda *a, **k: None
    ns["mark_producer_admission"] = lambda *a, **k: None
    ns["busy_row_settled"] = lambda *a, **k: None
    src = open(SA_SRC, encoding="utf-8").read()
    seed_constants(src, ns, SA_SRC)
    for node in func_nodes(src):
        exec_node(src, node, ns, SA_SRC)
    for node in class_nodes(src, ["SessionAuthority"]):
        exec_node(src, node, ns, SA_SRC)
    seed_constants(src, ns, SA_SRC)
    required = ["admission_row", "SessionAuthority"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_session_authority_flow2: session_authority missing: {missing}")
        sys.exit(2)
    mixin = ns["SessionAuthority"]
    for meth in PORTED:
        if meth in MODULE_FUNCS:
            if meth not in ns:
                print(f"FATAL oracle_session_authority_flow2: module missing {meth}")
                sys.exit(2)
        elif meth == "detached":
            continue
        elif not hasattr(mixin, meth):
            print(f"FATAL oracle_session_authority_flow2: SessionAuthority missing {meth}")
            sys.exit(2)
    return ns


def extract_detached_prop():
    src = open(SA_SRC, encoding="utf-8").read()
    lines = src.splitlines()
    for node in ast.parse(src).body:
        if isinstance(node, ast.ClassDef) and node.name == "LiveSession":
            for m in node.body:
                if isinstance(m, ast.FunctionDef) and m.name == "detached":
                    text = textwrap.dedent("\n".join(lines[m.lineno - 1:m.end_lineno]))
                    ns = {"__name__": "sau2_oracle_sa"}
                    exec(compile(text, SA_SRC, "exec", flags=_FLAGS, dont_inherit=True), ns)
                    return property(ns["detached"])
    print("FATAL oracle_session_authority_flow2: LiveSession.detached not found")
    sys.exit(2)


# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
        if is_dataclass(x) and not isinstance(x, type):
            return conv(asdict(x))
        if isinstance(x, enum.Enum):
            return x.value
        if isinstance(x, StateNS):
            return conv(object.__getattribute__(x, "_d"))
        if isinstance(x, (AgentDouble, TransportDouble, TaskDouble, WaiterDouble,
                          ControlsDouble, EventDouble)):
            return conv(norm_double(x))
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, (frozenset, set)):
            return sorted((conv(i) for i in x), key=lambda z: json.dumps(z, sort_keys=True))
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception materialization / projection ──
EXC_CLASSES = {
    "OSError": OSError, "TimeoutError": TimeoutError, "PermissionError": PermissionError,
    "FileNotFoundError": FileNotFoundError, "ValueError": ValueError, "TypeError": TypeError,
    "RuntimeError": RuntimeError, "KeyError": KeyError, "AttributeError": AttributeError,
    "ImportError": ImportError, "Exception": Exception, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
    "AuthorityRefusal": SUP["AuthorityRefusal"],
    "StoreUnavailable": SUP["StoreUnavailable"], "StoreReplaced": SUP["StoreReplaced"],
    "CorruptStoreError": SUP["CorruptStoreError"],
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    if kind == "AuthorityRefusal":
        return SUP["AuthorityRefusal"](spec.get("reason", ""),
                                       field=spec.get("field"), detail=spec.get("detail"))
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    msg = spec.get("message", "")
    return cls(msg)


def proj_exc(exc):
    d = {"__raise__": type(exc).__name__, "message": str(exc)}
    if isinstance(exc, SUP["AuthorityRefusal"]):
        d["reason"] = exc.reason
        d["field"] = exc.field
        d["detail"] = exc.detail
    return d


# ── per-case script backend (mirrors the C sau2_call exactly) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": args, "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            self._record_raise(name, args, RuntimeError(f"no script for {name}"))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                self._record_raise(name, args, RuntimeError(f"script exhausted for {name}"))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                self._record_raise(name, args, make_exc(used["raise"]))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": args, "ret": canon(ret)})
        return ret


def rec(ctx, name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


def arec(ctx, name, args_fn=None, build=None):
    async def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


# ── attribute doubles ──
class StateNS:
    def __init__(self, d):
        object.__setattr__(self, "_d", d)

    def __getattr__(self, k):
        d = object.__getattribute__(self, "_d")
        if k not in d:
            raise AttributeError(k)
        v = d[k]
        return StateNS(v) if isinstance(v, dict) else v

    def __setattr__(self, k, v):
        object.__getattribute__(self, "_d")[k] = v


class NoopCM:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


def make_source(d):
    if d is None:
        return None
    d = dict(d)
    if d.get("platform"):
        d["platform"] = SUP["Platform"](d["platform"])
    return types.SimpleNamespace(**d)


def norm_source(s):
    if s is None:
        return None
    d = dict(vars(s))
    p = d.get("platform")
    d["platform"] = p.value if isinstance(p, enum.Enum) else p
    return d


def make_actor(d):
    return SUP["SessionActor"](subject=d["subject"], profile_id=d["profile_id"],
                               capabilities=frozenset(d.get("capabilities") or []),
                               transport_id=d["transport_id"])


def norm_double(x):
    if isinstance(x, AgentDouble):
        return x.id
    if isinstance(x, TransportDouble):
        return x.id
    if isinstance(x, TaskDouble):
        return {"id": x.id, "done": x._done}
    if isinstance(x, WaiterDouble):
        return {"done": x._done}
    if isinstance(x, ControlsDouble):
        return {}
    if isinstance(x, EventDouble):
        return dict(object.__getattribute__(x, "_d"))
    return {}


class AgentDouble:
    def __init__(self, spec, ctx):
        self.id = spec.get("id")
        self._ctx = ctx


_TRANSPORTS = {}


def intern_transport(id_):
    if id_ not in _TRANSPORTS:
        _TRANSPORTS[id_] = TransportDouble(id_)
    return _TRANSPORTS[id_]


class TransportDouble:
    def __init__(self, id_):
        self.id = id_


class TaskDouble:
    def __init__(self, id_, done=False, ctx=None):
        self.id = id_
        self._done = done
        self._ctx = ctx

    def done(self):
        return self._done

    def add_done_callback(self, cb):
        return None


class WaiterDouble:
    def __init__(self, done, ctx, aid):
        self._done = done
        self._ctx = ctx
        self._aid = aid

    def done(self):
        return self._done

    def set_exception(self, exc):
        reason = exc.reason if isinstance(exc, SUP["AuthorityRefusal"]) else str(exc)
        return self._ctx.call("waiter.set_exception",
                              {"admission_id": self._aid, "reason": reason})

    def set_result(self, result):
        return self._ctx.call("waiter.set_result",
                              {"admission_id": self._aid, "result": result})


class ControlsDouble:
    def __init__(self, ctx, sid):
        self._ctx = ctx
        self._sid = sid


class EventDouble:
    def __init__(self, d, ctx):
        object.__setattr__(self, "_d", d)
        object.__setattr__(self, "_ctx", ctx)

    def __getattr__(self, k):
        d = object.__getattribute__(self, "_d")
        if k in d:
            return d[k]
        raise AttributeError(k)

    def __setattr__(self, k, v):
        object.__getattribute__(self, "_d")[k] = v


CUR_CTX = None
DETACHED_PROP = extract_detached_prop()


class LiveDouble:
    """Stand-in for LiveSession: carries the real `detached` property so
    name_lane's `live.detached` fires the seam at the right trace position."""
    detached = DETACHED_PROP

    def __init__(self, source=None, route=None, **kw):
        self.source = source
        self.route = route
        self.task = None
        self.subscribers = {}
        self.settling = {}
        self.rescan = False
        self.pause_notified = False
        self.background_wait = None
        self.detaching = 0
        self.mutation_lock = _NoopLock()
        self.claim_gate = _NoopLock()
        self.event_stream = types.SimpleNamespace(
            lock=NoopCM(), execution={}, publish=lambda *a, **k: None)
        self.controls = ControlsDouble(CUR_CTX, None)
        self.__dict__.update(kw)


def build_receipt(d):
    if d is None:
        return None
    return SUP["AdmissionReceipt"](
        admission_id=d["admission_id"], ref=SUP["SessionRef"](**d["ref"]),
        sequence=d["sequence"], status=d["status"], outcome=d["outcome"],
        authority_epoch=d["authority_epoch"], execution_generation=d["execution_generation"])


def build_handle(d):
    if d is None:
        return None
    return SUP["SessionHandle"](
        ref=SUP["SessionRef"](**d["ref"]), instance_id=d["instance_id"],
        authority_epoch=d["authority_epoch"], revision=d["revision"],
        execution_generation=d["execution_generation"], execution_state=d["execution_state"])


def build_record(r):
    return None if r is None else types.SimpleNamespace(**r)


def make_live(spec, ctx, sid):
    spec = spec or {}
    live = LiveDouble()
    live.route = spec.get("route")
    live.source = make_source(spec.get("source"))
    live.settling = dict(spec.get("settling") or {})
    live.subscribers = {sub: make_actor(a) for sub, a in (spec.get("subscribers") or {}).items()}
    task_spec = spec.get("task")
    live.task = TaskDouble(task_spec["id"], task_spec.get("done", False), ctx) if task_spec else None
    live.rescan = spec.get("rescan", False)
    live.background_wait = spec.get("background_wait")
    live.pause_notified = spec.get("pause_notified", False)
    es_spec = spec.get("event_stream") or {}
    live.event_stream = types.SimpleNamespace(
        lock=NoopCM(), execution=dict(es_spec.get("execution") or {}),
        publish=lambda session_id, payload, event_type=None: ctx.call(
            "event_stream.publish",
            {"session_id": session_id, "payload": payload, "event_type": event_type}))
    live.controls = ControlsDouble(ctx, sid)
    live.session_id = sid
    live._token = spec.get("token", sid)
    return live


# ── sys.modules stubs for in-function imports (every level registered) ──
def stub_modules(mapping):
    for dotted, attrs in mapping.items():
        parts = dotted.split(".")
        for i in range(1, len(parts) + 1):
            name = ".".join(parts[:i])
            if name not in sys.modules:
                m = types.ModuleType(name)
                m.__path__ = []
                sys.modules[name] = m
            if i > 1:
                setattr(sys.modules[".".join(parts[:i - 1])], parts[i - 1], sys.modules[name])
        mod = sys.modules[dotted]
        for k, v in attrs.items():
            setattr(mod, k, v)


async def _tracked_write(self, fn, then=None, ordered=False, after=None):
    res = fn()
    if inspect.isawaitable(res):
        res = await res
    if then is not None:
        res = then(res)
        if inspect.isawaitable(res):
            res = await res
    if after is not None:
        r = after(res)
        if inspect.isawaitable(r):
            await r
    return res


def _track_mutation(self, x):
    return x


def setup_authority(args, state, ctx):
    rstate = state.get("runner") or {}
    runner = types.SimpleNamespace(_draining=rstate.get("_draining", False))
    if rstate.get("_resident_agent_for"):
        runner._resident_agent_for = rec(ctx, "runner._resident_agent_for",
                                         args_fn=lambda key: {"key": key})
    runner._cached_agent_for = rec(ctx, "runner._cached_agent_for",
                                   args_fn=lambda key: {"key": key})
    runner.session_store = types.SimpleNamespace(
        get_or_create_session=rec(ctx, "runner.session_store.get_or_create_session",
                                  args_fn=lambda source: {"source": canon(source)},
                                  build=build_record))

    auth = NS_SA["SessionAuthority"](runner, profile_id=state["profile_id"],
                                     instance_id=state["instance_id"],
                                     epoch=state["epoch"], persistence=None)
    auth.retiring = state.get("retiring", False)
    auth.sessions = {sid: make_live(spec, ctx, sid)
                     for sid, spec in (state.get("sessions") or {}).items()}
    auth.waiters = {aid: WaiterDouble(spec.get("done", False), ctx, aid)
                    for aid, spec in (state.get("waiters") or {}).items()}
    auth.native_waiters = set(state.get("native_waiters") or [])
    auth.cancel_obligations = set(state.get("cancel_obligations") or [])
    auth.pending_steers = {sid: (v[0], list(v[1]))
                           for sid, v in (state.get("pending_steers") or {}).items()}
    auth.adopted = {sid: (v[0], AgentDouble({"id": v[1]}, ctx) if v[1] is not None else None)
                    for sid, v in (state.get("adopted") or {}).items()}
    auth.executing = dict(state.get("executing") or {})
    auth._classic_evict_at = dict(state.get("_classic_evict_at") or {})
    auth._route_epochs = dict(state.get("_route_epochs") or {})
    auth.events = {tid: intern_transport(tid) for tid in (state.get("events") or {})}
    auth.pending_results = dict(state.get("pending_results") or {})
    auth.pending_stops = dict(state.get("pending_stops") or {})
    auth.delivered_stops = dict(state.get("delivered_stops") or {})

    # claimed siblings -> recording seams (never re-ported)
    auth.authorize = rec(ctx, "self.authorize",
                         args_fn=lambda actor, ref, cap: {"actor": canon(actor),
                                                          "ref": canon(ref), "capability": cap})
    auth._handle = rec(ctx, "self._handle", args_fn=lambda ref: {"ref": canon(ref)},
                       build=build_handle)
    auth._receipt = rec(ctx, "self._receipt", args_fn=lambda row: {"row": canon(row)},
                        build=build_receipt)
    auth.logical_owner = rec(ctx, "self.logical_owner", args_fn=lambda sid: {"session_id": sid})
    auth._require_admission_open = rec(ctx, "self._require_admission_open",
                                       args_fn=lambda *, queue_only=False: {"queue_only": queue_only})
    auth._current_owner = rec(ctx, "self._current_owner", args_fn=lambda sid: {"session_id": sid})
    auth.gate_refusal = rec(ctx, "self.gate_refusal",
                            args_fn=lambda check=None, *, queue_only=False: {
                                "check": True if check is not None else None,
                                "queue_only": queue_only})
    auth._runs_own_agent = rec(ctx, "self._runs_own_agent", args_fn=lambda ref: {"ref": canon(ref)})
    auth._publish_pending = rec(ctx, "self._publish_pending", args_fn=lambda ref: {"ref": canon(ref)})
    auth._schedule = rec(ctx, "self._schedule", args_fn=lambda ref: {"ref": canon(ref)})
    auth._settle_cancelled = rec(ctx, "self._settle_cancelled",
                                 args_fn=lambda ref, aid, outcome="cancelled": {
                                     "ref": canon(ref), "admission_id": aid, "outcome": outcome})
    auth._commit_cancel = rec(ctx, "self._commit_cancel",
                              args_fn=lambda ref, aid, before: {
                                  "ref": canon(ref), "admission_id": aid, "before": canon(before)})
    auth._admitted = rec(ctx, "self._admitted",
                         args_fn=lambda ref, event, row: {"ref": canon(ref),
                                                          "event": canon(event), "row": canon(row)})
    auth._schedule_admitted = rec(ctx, "self._schedule_admitted",
                                  args_fn=lambda ref, _receipt=None: {"ref": canon(ref)})
    auth._release_refused_capture = rec(ctx, "self._release_refused_capture",
                                        args_fn=lambda request, payload: {
                                            "request": canon(request), "payload": canon(payload)})
    auth.physical_target = rec(ctx, "self.physical_target", args_fn=lambda ref: {"ref": canon(ref)})
    NS_SA["admission_row"] = rec(ctx, "admission_row", args_fn=lambda record: {"record": canon(record)})
    NS_SA["LiveSession"] = LiveDouble

    store = types.SimpleNamespace(
        sessions=types.SimpleNamespace(
            get=rec(ctx, "store.sessions.get", args_fn=lambda session_id: {"session_id": session_id},
                    build=build_record),
            create=rec(ctx, "store.sessions.create",
                       args_fn=lambda ns: {"session_id": ns.session_id, "source": ns.source})),
        admissions=types.SimpleNamespace(
            list=rec(ctx, "store.admissions.list",
                     args_fn=lambda *, session_id, pending_only=True: {
                         "session_id": session_id, "pending_only": pending_only},
                     build=lambda rows: [types.SimpleNamespace(**d) for d in rows]),
            get=rec(ctx, "store.admissions.get", args_fn=lambda *, admission_id: {"admission_id": admission_id},
                    build=build_record),
            cancel=rec(ctx, "store.admissions.cancel",
                       args_fn=lambda *, epoch, admission_id: {"epoch": epoch, "admission_id": admission_id},
                       build=build_record),
            admit=rec(ctx, "store.admissions.admit",
                      args_fn=lambda **kw: {k: canon(v) for k, v in kw.items()}),
            resolve_unknown=rec(ctx, "store.admissions.resolve_unknown",
                                args_fn=lambda *, epoch, admission_id, generation, captured, notice: {
                                    "epoch": epoch, "admission_id": admission_id,
                                    "generation": generation, "captured": canon(captured),
                                    "notice": canon(notice)},
                                build=build_record)),
        scope=StateNS(state.get("store_scope") or {}),
    )
    NS_SA["authority_persistence"] = lambda a: store

    stub_modules({
        "gateway.session_detached": {
            "detach": rec(ctx, "detach", args_fn=lambda a, other, other_sid: {
                "session_id": other_sid, "route": other.route}),
            "forget_detached": rec(ctx, "forget_detached", args_fn=lambda a, route: {"route": route}),
            "detached_route_of": rec(ctx, "detached_route_of", args_fn=lambda source: {"source": canon(source)}),
            "running_key": rec(ctx, "running_key",
                               args_fn=lambda runner, route, sid: {"route": route, "session_id": sid})},
        "persistence.conversation.sessions": {
            "NewSession": lambda **kw: types.SimpleNamespace(**kw)},
        "gateway.session_ingress_media": {
            "drop_capture_lease": rec(ctx, "drop_capture_lease", args_fn=lambda: {}),
            "reconcile_native_retry": rec(ctx, "reconcile_native_retry",
                                          args_fn=lambda store, *, principal_id, session_id,
                                          request_id, payload, raw: {
                                              "principal_id": principal_id, "session_id": session_id,
                                              "request_id": request_id, "payload": canon(payload),
                                              "raw": canon(raw)}),
            "release_unheld_media": rec(ctx, "release_unheld_media",
                                        args_fn=lambda store, fresh, *, retain_history: {
                                            "fresh": canon(fresh), "retain_history": retain_history}),
            "release_admission_media": rec(ctx, "release_admission_media",
                                           args_fn=lambda store, aid: {"admission_id": aid}),
            "capture_lease": rec(ctx, "capture_lease", args_fn=lambda: {}, build=lambda r: NoopCM()),
            "admit_attachments": rec(ctx, "admit_attachments",
                                     args_fn=lambda attachments, *, admitted: {
                                         "attachments": canon(attachments), "admitted": True})},
        "gateway.owner_identity": {
            "same_owner": rec(ctx, "same_owner", args_fn=lambda a, b: {"a": a, "b": b})},
        "gateway.session_results": {
            "prepare_result": rec(ctx, "prepare_result",
                                  args_fn=lambda row, response, outcome, result=None: {
                                      "row": canon(row), "final_response": response,
                                      "status": outcome, "captured": canon(result)})},
        "gateway.session_retirement": {
            "restore_retired": rec(ctx, "restore_retired", args_fn=lambda a, sid: {"session_id": sid})},
        "gateway.session_ingress": {
            "deliver_resolved": arec(ctx, "deliver_resolved",
                                     args_fn=lambda a, aid, *, owed: {"admission_id": aid, "owed": owed})},
        "gateway.session_settlement_recovery": {
            "_publish_completion": rec(ctx, "_publish_completion",
                                       args_fn=lambda a, live, ref, row, row2, response, captured: {
                                           "ref": canon(ref), "row": canon(row),
                                           "final_response": response, "captured": canon(captured)}),
            "release_media_after_delivery": rec(ctx, "release_media_after_delivery",
                                                args_fn=lambda a, row: {"row": canon(row)}),
            "hand_collection_to_drain": rec(ctx, "hand_collection_to_drain",
                                            args_fn=lambda a, aid: {"admission_id": aid})},
        "agent.interrupt_compat": {
            "request_hard_interrupt": rec(ctx, "request_hard_interrupt",
                                          args_fn=lambda agent: {"agent": canon(agent)})},
        "gateway.run_plugin_injection": {
            "wake_background_wait": rec(ctx, "wake_background_wait",
                                        args_fn=lambda a, ref, *, person_input=False: {
                                            "ref": canon(ref), "person_input": person_input})},
        "gateway.session_envelope": {
            "check_native_route": arec(ctx, "check_native_route",
                                       args_fn=lambda runner, payload, target, available_source, adapter: {
                                           "payload": canon(payload), "target": target,
                                           "available_source": canon(available_source), "adapter": adapter},
                                       build=lambda r: (make_source(r[0]), r[1]) if r is not None else None)},
        "gateway.session_automation": {
            "check_local_automation": rec(ctx, "check_local_automation",
                                          args_fn=lambda a, ref, row, *, route=None: {
                                              "ref": canon(ref), "row": canon(row), "route": route}),
            "admit_automation": arec(ctx, "admit_automation",
                                     args_fn=lambda a, adapter, event, identity, replaces, fold: {
                                         "adapter": adapter, "event": canon(event),
                                         "identity": canon(identity), "replaces": replaces, "fold": fold})},
        "gateway.session_operator": {
            "check_local_input": rec(ctx, "check_local_input",
                                     args_fn=lambda a, ref, row, *, source=None: {
                                         "ref": canon(ref), "row": canon(row), "source": canon(source)})},
        "gateway.session_finite": {
            "admit_finite": rec(ctx, "admit_finite", args_fn=lambda payload: {"payload": canon(payload)})},
        "gateway.session_surface": {
            "admit_surface": rec(ctx, "admit_surface", args_fn=lambda payload: {"payload": canon(payload)})},
        "gateway.session_display": {
            "admit_display": rec(ctx, "admit_display", args_fn=lambda payload: {"payload": canon(payload)})},
        "gateway.session_cli_turn": {
            "admit_cli_turn": rec(ctx, "admit_cli_turn", args_fn=lambda payload: {"payload": canon(payload)})},
        "gateway.session_runtime_workers": {
            "tracked_write": _tracked_write, "track_mutation": _track_mutation,
            "persist_work_count": lambda *a, **k: None},
        "gateway.config": {"Platform": SUP["Platform"]},
        "persistence.authority.admissions": {"CapturedResult": SUP["CapturedResult"]},
    })
    return auth


def project_live(live, inspec, sid):
    d = canon(inspec or {})
    d["route"] = live.route
    d["source"] = norm_source(live.source)
    d["settling"] = live.settling
    d["subscribers"] = {sub: canon(a) for sub, a in live.subscribers.items()}
    d["task"] = canon(live.task) if live.task is not None else None
    d["rescan"] = live.rescan
    d["background_wait"] = live.background_wait
    d["pause_notified"] = live.pause_notified
    d["token"] = getattr(live, "_token", sid)
    es = d.get("event_stream")
    if not isinstance(es, dict):
        es = {}
    es["execution"] = live.event_stream.execution
    d["event_stream"] = es
    return d


def project_authority_state(state, auth):
    final = canon(state)
    final["retiring"] = auth.retiring
    insessions = state.get("sessions") or {}
    final["sessions"] = {sid: project_live(l, insessions.get(sid), sid)
                         for sid, l in auth.sessions.items()}
    final["waiters"] = {aid: {"done": w._done} for aid, w in auth.waiters.items()}
    final["native_waiters"] = sorted(auth.native_waiters)
    final["cancel_obligations"] = sorted(auth.cancel_obligations)
    final["pending_steers"] = {sid: [g, list(t)] for sid, (g, t) in auth.pending_steers.items()}
    final["adopted"] = {sid: [g, (a.id if isinstance(a, AgentDouble) else a)]
                        for sid, (g, a) in auth.adopted.items()}
    final["executing"] = dict(auth.executing)
    final["_classic_evict_at"] = dict(auth._classic_evict_at)
    final["_route_epochs"] = dict(auth._route_epochs)
    final["pending_results"] = dict(auth.pending_results)
    final["pending_stops"] = dict(auth.pending_stops)
    final["delivered_stops"] = dict(auth.delivered_stops)
    return final


PORTED = ["detached", "_reserve_route", "name_lane", "_publish_route", "register",
          "agent", "_admit_native_write", "receipt", "cancel_queued", "cancel_or_claimed",
          "resolve_unknown", "resolve_unknown_as_owner", "_resolved_unknown", "interrupt",
          "_refuse_head", "recover_native_sessions", "admit_automation", "submit", "_submit"]
SA_ASYNC_FUNCS = {"receipt", "cancel_queued", "cancel_or_claimed", "resolve_unknown",
                  "resolve_unknown_as_owner", "_resolved_unknown", "interrupt", "_refuse_head",
                  "recover_native_sessions", "admit_automation", "submit", "_submit"}
MODULE_FUNCS = set()


def make_request(d):
    return types.SimpleNamespace(ref=SUP["SessionRef"](**d["ref"]), intent=d["intent"],
                                 payload=d["payload"], request_id=d["request_id"])


def invoke_authority(func, args, auth, ctx):
    ref = lambda: SUP["SessionRef"](**args["ref"])
    actor = lambda: make_actor(args["actor"])
    if func == "detached":
        return auth.sessions[args["session_id"]].detached
    if func == "_reserve_route":
        return auth._reserve_route(make_source(args["source"]))
    if func == "name_lane":
        return auth.name_lane(args["route"], args["session_id"], make_source(args["source"]))
    if func == "_publish_route":
        return auth._publish_route(make_source(args["source"]), args["session_id"], args["route"])
    if func == "register":
        return auth.register(make_source(args["source"]))
    if func == "agent":
        return auth.agent(ref())
    if func == "_admit_native_write":
        rewrite = types.SimpleNamespace(**args["rewrite"]) if args.get("rewrite") is not None else None
        fence = (lambda _conn=None: None) if args.get("fence") else None
        return auth._admit_native_write(
            principal_id=args["principal_id"], session_id=args["session_id"],
            request_id=args["request_id"], payload=args["payload"], fence=fence,
            rewrite=rewrite, raw=args.get("raw"))
    if func == "receipt":
        return _asyncio.run(auth.receipt(actor(), ref(), args["admission_id"]))
    if func == "cancel_queued":
        return _asyncio.run(auth.cancel_queued(actor(), ref(), args["admission_id"]))
    if func == "cancel_or_claimed":
        return _asyncio.run(auth.cancel_or_claimed(actor(), ref(), args["admission_id"]))
    if func == "resolve_unknown":
        return _asyncio.run(auth.resolve_unknown(actor(), ref(), args["admission_id"],
                                                 args["generation"]))
    if func == "resolve_unknown_as_owner":
        return _asyncio.run(auth.resolve_unknown_as_owner(ref(), args["admission_id"],
                                                          args["generation"],
                                                          notice=args.get("notice")))
    if func == "_resolved_unknown":
        prepared = args.get("prepared")
        prepared = tuple(prepared) if prepared is not None else None
        return _asyncio.run(auth._resolved_unknown(ref(), args["admission_id"], prepared,
                                                   args.get("captured"),
                                                   build_record(args.get("record"))))
    if func == "interrupt":
        return _asyncio.run(auth.interrupt(actor(), ref(), args["generation"]))
    if func == "_refuse_head":
        return _asyncio.run(auth._refuse_head(ref(), args["admission_id"], args["reason"]))
    if func == "recover_native_sessions":
        bindings = [(sid, make_source(avsrc), adapter)
                    for sid, avsrc, adapter in args["bindings"]]
        return _asyncio.run(auth.recover_native_sessions(bindings))
    if func == "admit_automation":
        return _asyncio.run(auth.admit_automation(args["adapter"], args.get("event"),
                                                  args.get("identity"),
                                                  replaces=args.get("replaces"),
                                                  fold=args.get("fold")))
    if func == "submit":
        return _asyncio.run(auth.submit(actor(), make_request(args["request"]),
                                        queue_while_restarting=args.get("queue_while_restarting", False),
                                        _kanban_card=args.get("_kanban_card"),
                                        replaces=args.get("replaces"),
                                        supersedes=args.get("supersedes"),
                                        api_target=args.get("api_target"),
                                        hosted=args.get("hosted")))
    if func == "_submit":
        return _asyncio.run(auth._submit(actor(), make_request(args["request"]),
                                         queue_only=args.get("queue_only", False),
                                         _kanban_card=args.get("_kanban_card"),
                                         replaces=args.get("replaces"),
                                         supersedes=args.get("supersedes"),
                                         api_target=args.get("api_target"),
                                         hosted=args.get("hosted")))
    raise RuntimeError(f"unknown authority func {func}")


def drive(func, inp):
    global CUR_CTX
    args = inp.get("args") or {}
    state = canon(inp.get("state") or {})
    ctx = Ctx(inp.get("io"))
    CUR_CTX = ctx
    _TRANSPORTS.clear()
    auth = setup_authority(args, state, ctx)
    try:
        ret = invoke_authority(func, args, auth, ctx)
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    final = project_authority_state(state, auth)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(final)}


NS_SA = extract_sa()

# In-function `import logging` resolves through sys.modules; keep it silent.
_logstub = types.ModuleType("logging")
_logstub.getLogger = lambda name=None: _SilentLogger()
sys.modules["logging"] = _logstub

# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
SEAM_NAMES = [
    "admission_row", "self._receipt", "self._handle", "self.authorize", "self.logical_owner",
    "self._require_admission_open", "self._current_owner", "self.gate_refusal",
    "self._runs_own_agent", "self._publish_pending", "self._schedule", "self._settle_cancelled",
    "self._commit_cancel", "self._admitted", "self._schedule_admitted",
    "self._release_refused_capture", "self.physical_target",
    "runner.session_store.get_or_create_session", "store.sessions.get", "store.sessions.create",
    "store.admissions.list", "store.admissions.get", "store.admissions.cancel",
    "store.admissions.admit", "store.admissions.resolve_unknown",
    "detach", "forget_detached", "detached_route_of", "running_key",
    "runner._resident_agent_for", "runner._cached_agent_for",
    "reconcile_native_retry", "drop_capture_lease", "release_unheld_media",
    "release_admission_media", "capture_lease", "admit_attachments",
    "same_owner", "prepare_result", "restore_retired", "deliver_resolved",
    "_publish_completion", "release_media_after_delivery", "hand_collection_to_drain",
    "request_hard_interrupt", "wake_background_wait", "check_native_route",
    "check_local_automation", "check_local_input", "admit_automation",
    "admit_finite", "admit_surface", "admit_display", "admit_cli_turn",
    "waiter.set_exception", "waiter.set_result", "event_stream.publish",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "session_authority_flow2.c", encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    # C-used seams must all be registered (catches a typo'd / unregistered seam).
    c_used = set(re.findall(r'sau2_call\(\s*c,\s*"([^"]+)"', csrc))
    missing_c = sorted(c_used - registry)
    if missing_c:
        print(f"FATAL oracle_session_authority_flow2: C seams outside registry: {missing_c}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'(?:rec|arec)\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'ctx\.call\(\s*"([^"]+)"', osrc))
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_session_authority_flow2: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "session_authority_flow2.jsonl"
    if fix.exists():
        fix_keys = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
        bad = fix_keys - registry
        if bad:
            print(f"FATAL oracle_session_authority_flow2: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)


def first_trace_diff(want, got):
    wt, gt = want.get("trace") or [], got.get("trace") or []
    for i in range(max(len(wt), len(gt))):
        w = wt[i] if i < len(wt) else None
        g = gt[i] if i < len(gt) else None
        if w != g:
            return i, w, g
    return None, None, None


def report_mismatch(func, want, got, shown):
    if shown >= 12:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:400]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:400]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:400]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:400]}")
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        keys = [k for k in set(ws) | set(gs) if ws.get(k) != gs.get(k)]
        print(f"  state diff keys: {sorted(keys)[:10]}")
        for k in sorted(keys)[:3]:
            print(f"    {k} want={json.dumps(ws.get(k), ensure_ascii=False)[:200]}")
            print(f"    {k} got ={json.dumps(gs.get(k), ensure_ascii=False)[:200]}")


def main():
    cross_check()
    n = m = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except Exception as exc:
            m += 1
            print(f"MISMATCH <unparsable harness line>: {exc}")
            continue
        n += 1
        func = rec.get("func", "")
        inp = rec.get("in") or {}
        got = rec.get("out")
        if not isinstance(got, dict) or "error" in got:
            m += 1
            print(f"MISMATCH {func}: harness emitted no result: {got}")
            continue
        try:
            want = drive(func, inp)
        except BaseException as exc:
            m += 1
            print(f"MISMATCH {func}: oracle drive raised {type(exc).__name__}: {exc}")
            continue
        g = canon(got)
        if g != want:
            m += 1
            report_mismatch(func, want, g, shown)
            shown += 1
    print(f"ORACLE session_authority_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


if __name__ == "__main__":
    main()