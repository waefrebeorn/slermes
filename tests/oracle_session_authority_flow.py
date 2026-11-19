#!/usr/bin/env python3
"""Oracle for the session-authority FLOW port
(src/gateway/session_authority_flow.c).

Drives the LIVE gateway/session_authority.py decision chains with SCRIPTED
seams: every store read/write, restore call, event-stream publish, controls
verb, waiter settlement, runtime-worker call and owner-identity check is a
recording stub fed the same scripts the C harness feeds its universal seam.
Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"} in
"trace") plus the final return ("ret") and the mutated self/world state
("state").

The live defs are AST-extracted from gateway/session_authority.py (module
constants seeded, Assign AND AnnAssign, plain-Name targets only; the
SessionAuthority ClassDef exec whole). Claimed siblings ported in
gateway_inbound_flow.c (admission_row, _receipt, _handle, authorize,
logical_owner) are SEAMED here, never re-ported. Support dataclasses/exceptions
are the REAL ones AST-extracted from session_contract / persistence.core.errors
/ persistence.authority.admissions / gateway.config / vocabulary. Every exec
namespace is a registered sys.modules module (py3.14 dataclasses _is_type needs
cls.__module__ resolvable). The heavy quarry import chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE session_authority_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc), ...}
  (AuthorityRefusal adds reason/field/detail).
- Platform members travel as .value strings; agents/transports as id strings
  wrapped in attribute doubles; _AGENT_PENDING_SENTINEL as "__sentinel__".
- logger calls are silent on both sides; authority_persistence is a silent
  handle lookup; asyncio.to_thread runs its callable synchronously;
  asyncio.create_task is a seam returning a task id (the scheduled _drain never
  runs, done callbacks never fire); locks are no-ops.
- sets (native_waiters, cancel_obligations) normalize to SORTED id arrays;
  waiters as {aid: {"done": bool}}; event_stream.execution is a state dict.
- generation type-fence fixtures use int / bool / str (float excluded: libjson
  cannot distinguish 1 from 1.0).
"""
import ast
import contextlib
import enum
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
    mod = new_module("sau_oracle_support")
    ns = mod.__dict__
    ns.update({"dataclass": dataclass, "field": field, "asdict": asdict,
               "Enum": Enum, "StrEnum": StrEnum, "Any": Any, "Optional": Optional,
               "Mapping": Mapping, "contextlib": contextlib})
    csrc = open(CONTRACT_SRC, encoding="utf-8").read()
    cmod = new_module("sau_oracle_contract")
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
                "ClaimedAdmission", "Platform", "STEERING", "_one_line", "STORE_FAILURES",
                "CorruptStoreError", "StoreReplaced", "CANONICAL_GATEWAY_PROTOCOL"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_session_authority_flow: support missing: {missing}")
        sys.exit(2)
    return ns


SUP = extract_support()


# ── asyncio double (create_task installed per case) ──
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
    mod = new_module("sau_oracle_sa")
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
    ns["LiveSession"] = type("LiveSession", (), {})
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
    required = ["admission_row", "SessionAuthority", "_log_drain_failure",
                "backfill_legacy_session_owners", "backfill_compression_created_sources"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_session_authority_flow: session_authority missing: {missing}")
        sys.exit(2)
    mixin = ns["SessionAuthority"]
    for meth in PORTED:
        if meth in MODULE_FUNCS:
            if meth not in ns:
                print(f"FATAL oracle_session_authority_flow: module missing {meth}")
                sys.exit(2)
        elif not hasattr(mixin, meth):
            print(f"FATAL oracle_session_authority_flow: SessionAuthority missing {meth}")
            sys.exit(2)
    return ns


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
                          ControlsDouble, EventDouble, DrainMarker)):
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


# ── per-case script backend (mirrors the C sau_call exactly) ──
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
    if isinstance(x, DrainMarker):
        return {"ref": canon(x.ref)}
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

    def cancelled(self):
        return self._ctx.call("task.cancelled", {"task": self.id})

    def exception(self):
        return self._ctx.call("task.exception", {"task": self.id})


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

    def register(self, session_id, route, generation, data, *, timeout_s=None):
        return self._ctx.call("controls.register",
                              {"session_id": session_id, "route": route,
                               "generation": generation, "data": data, "timeout_s": timeout_s})

    def register_clarify(self, session_id, generation, entry, *, timeout_s=None):
        return self._ctx.call("controls.register_clarify",
                              {"session_id": session_id, "generation": generation,
                               "entry": entry, "timeout_s": timeout_s})

    def respond(self, session_id, generation, prompt_id, response, *, kind="approval"):
        return self._ctx.call("controls.respond",
                              {"session_id": session_id, "generation": generation,
                               "prompt_id": prompt_id, "response": response, "kind": kind})


class EventDouble:
    def __init__(self, d, ctx):
        object.__setattr__(self, "_d", d)
        object.__setattr__(self, "_ctx", ctx)

    def __getattr__(self, k):
        d = object.__getattribute__(self, "_d")
        if k in d:
            v = d[k]
            if k == "_on_commit" and v:
                return lambda: object.__getattribute__(self, "_ctx").call("event._on_commit", {})
            return v
        raise AttributeError(k)

    def __setattr__(self, k, v):
        object.__getattribute__(self, "_d")[k] = v


class DrainMarker:
    def __init__(self, ref):
        self.ref = ref


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


def make_live(spec, ctx, sid):
    spec = spec or {}
    es_spec = spec.get("event_stream") or {}
    es = types.SimpleNamespace(
        lock=NoopCM(), execution=dict(es_spec.get("execution") or {}),
        publish=lambda session_id, payload, event_type=None: ctx.call(
            "event_stream.publish",
            {"session_id": session_id, "payload": payload, "event_type": event_type}),
    )
    subs = {sub: make_actor(a) for sub, a in (spec.get("subscribers") or {}).items()}
    task_spec = spec.get("task")
    task = TaskDouble(task_spec["id"], task_spec.get("done", False), ctx) if task_spec else None
    live = types.SimpleNamespace(
        route=spec.get("route"), source=make_source(spec.get("source")),
        settling=dict(spec.get("settling") or {}), subscribers=subs, task=task,
        rescan=spec.get("rescan", False), background_wait=spec.get("background_wait"),
        pause_notified=spec.get("pause_notified", False), event_stream=es,
        controls=ControlsDouble(ctx, sid), session_id=sid,
    )
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


SENTINEL = object()


def setup_authority(args, state, ctx):
    rstate = state.get("runner") or {}
    runner = types.SimpleNamespace(_draining=rstate.get("_draining", False))
    if rstate.get("_is_session_running"):
        runner._is_session_running = rec(ctx, "runner._is_session_running",
                                         args_fn=lambda route: {"route": route})
    runner._hm_evict_idle_stale_agent = rec(ctx, "runner._hm_evict_idle_stale_agent",
                                            args_fn=lambda route: {"route": route})
    runner._hm_evict_reaped_agent = rec(ctx, "runner._hm_evict_reaped_agent",
                                        args_fn=lambda route: {"route": route})
    ra = rstate.get("_running_agents") or {}
    runner._running_agents = {k: (SENTINEL if v == "__sentinel__" else AgentDouble({"id": v}, ctx))
                              for k, v in ra.items()}
    runner.session_store = types.SimpleNamespace()

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

    # claimed siblings -> recording seams (never re-ported)
    auth.authorize = rec(ctx, "self.authorize",
                         args_fn=lambda actor, ref, cap: {"actor": canon(actor),
                                                          "ref": canon(ref), "capability": cap})
    auth._handle = rec(ctx, "self._handle", args_fn=lambda ref: {"ref": canon(ref)},
                       build=build_handle)
    auth._receipt = rec(ctx, "self._receipt", args_fn=lambda row: {"row": canon(row)},
                        build=build_receipt)
    auth.logical_owner = rec(ctx, "self.logical_owner",
                             args_fn=lambda sid: {"session_id": sid})
    NS_SA["admission_row"] = rec(ctx, "admission_row",
                                 args_fn=lambda record: {"record": canon(record)})
    # module-level collaborators (silent stubs at extract time) -> recorders
    NS_SA["mark_producer_admission"] = rec(ctx, "mark_producer_admission",
                                           args_fn=lambda a, event, row: {"event": canon(event),
                                                                          "row": canon(row)})
    NS_SA["drop_producer_turn"] = rec(ctx, "drop_producer_turn",
                                      args_fn=lambda a, admission_id: {"admission_id": admission_id})
    NS_SA["busy_row_settled"] = rec(ctx, "busy_row_settled",
                                    args_fn=lambda admission_id, outcome: {"admission_id": admission_id,
                                                                           "outcome": outcome})
    # non-ported collaborators
    auth.agent = rec(ctx, "self.agent", args_fn=lambda ref: {"ref": canon(ref)},
                     build=lambda r: AgentDouble({"id": r}, ctx) if r is not None else None)
    auth._drain = lambda ref: DrainMarker(ref)

    def _create_task(coro):
        raw = ctx.call("asyncio.create_task", {"session_id": coro.ref.session_id})
        return TaskDouble(raw, done=False, ctx=ctx)
    SA_ASYNCIO.create_task = _create_task

    store = types.SimpleNamespace(
        sessions=types.SimpleNamespace(
            get=rec(ctx, "store.sessions.get", args_fn=lambda session_id: {"session_id": session_id},
                    build=lambda r: None if r is None else types.SimpleNamespace(**r)),
            store_owner=rec(ctx, "store.sessions.store_owner", args_fn=lambda: {}),
            backfill_session_owners=rec(ctx, "store.sessions.backfill_session_owners",
                                        args_fn=lambda stamp: {"stamp": stamp})),
        admissions=types.SimpleNamespace(
            list=rec(ctx, "store.admissions.list", args_fn=lambda session_id: {"session_id": session_id},
                     build=lambda rows: [types.SimpleNamespace(**d) for d in rows]),
            cancel=rec(ctx, "store.admissions.cancel",
                       args_fn=lambda epoch, admission_id: {"epoch": epoch, "admission_id": admission_id}),
            record_retired_media=rec(ctx, "store.admissions.record_retired_media",
                                     args_fn=lambda refs: {"refs": refs})),
        lineage=types.SimpleNamespace(
            stamp_legacy_compression_chains=rec(ctx, "store.lineage.stamp_legacy_compression_chains",
                                                args_fn=lambda: {})),
        scope=StateNS(state.get("store_scope") or {}),
    )
    NS_SA["authority_persistence"] = lambda a: store
    NS_SA["time"] = types.SimpleNamespace(monotonic=rec(ctx, "time.monotonic", args_fn=lambda: {}))
    # `import time` inside _classic_turn_holds resolves through sys.modules.
    _timestub = types.ModuleType("time")
    _timestub.monotonic = rec(ctx, "time.monotonic", args_fn=lambda: {})
    sys.modules["time"] = _timestub

    stub_modules({
        "gateway.session_retirement": {
            "restore_retired": rec(ctx, "restore_retired", args_fn=lambda a, sid: {"session_id": sid})},
        "gateway.session_queued_controls": {
            "pending_rows": rec(ctx, "pending_rows", args_fn=lambda a, ref: {"ref": canon(ref)}),
            "queued_controls": rec(ctx, "queued_controls", args_fn=lambda a, row: {"row": canon(row)})},
        "gateway.session_bot_mailbox": {
            "wake_bot_receipts": rec(ctx, "wake_bot_receipts", args_fn=lambda a, sid: {"session_id": sid})},
        "gateway.run_plugin_injection": {
            "wake_background_wait": rec(ctx, "wake_background_wait",
                                        args_fn=lambda a, ref, *, person_input=False: {
                                            "ref": canon(ref), "person_input": person_input})},
        "gateway.session_runtime_workers": {
            "persist_work_count": lambda *a, **k: None,
            "tracked_write": lambda *a, **k: None, "track_mutation": lambda *a, **k: None},
        "gateway.session_ingress_media": {
            "drop_capture_lease": rec(ctx, "drop_capture_lease", args_fn=lambda: {})},
        "session_retention": {
            "collect_retired_media": rec(ctx, "collect_retired_media",
                                         args_fn=lambda store, profile_dir: {"profile_dir": profile_dir})},
        "session_rules.media": {
            "admission_media_references": rec(ctx, "admission_media_references",
                                              args_fn=lambda p: {"payload": p})},
        "gateway.session_detached": {
            "running_key": rec(ctx, "running_key",
                               args_fn=lambda runner, route, sid: {"route": route, "session_id": sid})},
        "gateway.run": {"_AGENT_PENDING_SENTINEL": SENTINEL},
        "gateway.session_busy_controls": {
            "adopts_owner_agent": rec(ctx, "adopts_owner_agent", args_fn=lambda a, ref: {"ref": canon(ref)})},
        "gateway.session_automation": {
            "admission_display": rec(ctx, "admission_display", args_fn=lambda payload: {"payload": payload})},
        "gateway.config": {"Platform": SUP["Platform"]},
        "persistence_assembly": {
            "store_location": rec(ctx, "store_location", args_fn=lambda scope: {"scope": canon(scope)})},
    })
    return auth


def project_live(live, inspec):
    d = canon(inspec or {})
    d["route"] = live.route
    d["source"] = norm_source(live.source)
    d["settling"] = live.settling
    d["subscribers"] = {sub: canon(a) for sub, a in live.subscribers.items()}
    d["task"] = canon(live.task) if live.task is not None else None
    d["rescan"] = live.rescan
    d["background_wait"] = live.background_wait
    d["pause_notified"] = live.pause_notified
    d["token"] = live._token
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
    final["sessions"] = {sid: project_live(l, insessions.get(sid))
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
    return final


PORTED = ["route_epoch", "fence_route", "_log_drain_failure", "_run_deferred_reset",
          "backfill_legacy_session_owners", "backfill_compression_created_sources",
          "check_approval_generation", "_drain_stopping", "_pending_rows",
          "_retire_overflowed", "unless_deleted", "resolve", "_pending_receipt",
          "_publish_pending", "_schedule", "wake_after_worker", "_pause", "_admitted",
          "_schedule_admitted", "_release_refused_capture", "_commit_cancel",
          "_settle_cancelled", "_classic_turn_holds", "_runs_own_agent", "steer_agent",
          "register_approval", "register_clarify", "respond", "_stamp_claim"]
SA_ASYNC_FUNCS = {"resolve", "respond", "_commit_cancel", "_run_deferred_reset"}
MODULE_FUNCS = {"backfill_legacy_session_owners", "backfill_compression_created_sources",
                "_log_drain_failure"}


def invoke_authority(func, args, auth, ctx):
    ref = lambda: SUP["SessionRef"](**args["ref"])
    actor = lambda: make_actor(args["actor"])
    if func == "route_epoch":
        return auth.route_epoch(args["route"])
    if func == "fence_route":
        return auth.fence_route(args["route"])
    if func == "_log_drain_failure":
        task = TaskDouble(args["task"], ctx=ctx)
        return NS_SA["_log_drain_failure"](task)
    if func == "_run_deferred_reset":
        reset = arec(ctx, "reset", args_fn=lambda: {}) if args.get("reset") else None
        return _asyncio.run(auth._run_deferred_reset(args["route"], reset))
    if func == "backfill_legacy_session_owners":
        return NS_SA["backfill_legacy_session_owners"](NS_SA["authority_persistence"](auth))
    if func == "backfill_compression_created_sources":
        return NS_SA["backfill_compression_created_sources"](NS_SA["authority_persistence"](auth))
    if func == "check_approval_generation":
        return auth.check_approval_generation(args["session_id"], args["generation"])
    if func == "_drain_stopping":
        sid = args["ref"]["session_id"]
        live_token = args.get("live_token", sid)
        cur = auth.sessions.get(sid)
        # `self.sessions.get(sid) is not live`: pass the ACTUAL entry when the
        # captured live is it (token match), else a distinct object.
        if cur is not None and cur._token == live_token:
            live = cur
        else:
            live = types.SimpleNamespace(_token=live_token)
        return auth._drain_stopping(ref(), live)
    if func == "_pending_rows":
        return auth._pending_rows(ref())
    if func == "_retire_overflowed":
        return auth._retire_overflowed(args["session_id"], intern_transport(args["transport"]))
    if func == "unless_deleted":
        read = rec(ctx, "read", args_fn=lambda: {})
        return auth.unless_deleted(ref(), read)
    if func == "resolve":
        return _asyncio.run(auth.resolve(actor(), ref()))
    if func == "_pending_receipt":
        return auth._pending_receipt(args["row"])
    if func == "_publish_pending":
        return auth._publish_pending(ref())
    if func == "_schedule":
        return auth._schedule(ref())
    if func == "wake_after_worker":
        return auth.wake_after_worker(args["session_id"])
    if func == "_pause":
        return auth._pause(ref(), args["reason"])
    if func == "_admitted":
        event = EventDouble(state_event, ctx) if args.get("event") else None
        return auth._admitted(ref(), event, args["row"])
    if func == "_schedule_admitted":
        return auth._schedule_admitted(ref())
    if func == "_release_refused_capture":
        request = types.SimpleNamespace(request_id=args["request"]["request_id"])
        return auth._release_refused_capture(request, args["payload"])
    if func == "_commit_cancel":
        before = build_receipt(args["before"])
        return _asyncio.run(auth._commit_cancel(ref(), args["admission_id"], before))
    if func == "_settle_cancelled":
        return auth._settle_cancelled(ref(), args["admission_id"],
                                      outcome=args.get("outcome", "cancelled"))
    if func == "_classic_turn_holds":
        return auth._classic_turn_holds(args["route"])
    if func == "_runs_own_agent":
        return auth._runs_own_agent(ref())
    if func == "steer_agent":
        return auth.steer_agent(ref(), args["generation"], args.get("text"))
    if func == "register_approval":
        return auth.register_approval(args["session_id"], args["generation"], args["route"],
                                      args["data"], timeout_s=args.get("timeout_s"))
    if func == "register_clarify":
        return auth.register_clarify(args["session_id"], args["generation"], args["entry"],
                                     timeout_s=args.get("timeout_s"))
    if func == "respond":
        return _asyncio.run(auth.respond(actor(), ref(), args["generation"], args["prompt_id"],
                                         args["response"], kind=args.get("kind", "approval")))
    if func == "_stamp_claim":
        live = auth.sessions.get(args["ref"]["session_id"])
        return auth._stamp_claim(ref(), live, args.get("row"))
    raise RuntimeError(f"unknown authority func {func}")


state_event = None


def drive(func, inp):
    global state_event
    args = inp.get("args") or {}
    state = canon(inp.get("state") or {})
    ctx = Ctx(inp.get("io"))
    _TRANSPORTS.clear()
    state_event = state.get("event") if args.get("event") else None
    auth = setup_authority(args, state, ctx)
    try:
        ret = invoke_authority(func, args, auth, ctx)
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    final = project_authority_state(state, auth)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(final)}


NS_SA = extract_sa()

# In-function `import logging` (drain failure, backfills, refused capture,
# deferred reset) resolves through sys.modules; keep it silent on both sides.
# The logger ARGUMENTS (task.exception(), store_location(...)) still evaluate.
_logstub = types.ModuleType("logging")
_logstub.getLogger = lambda name=None: _SilentLogger()
sys.modules["logging"] = _logstub

# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
SEAM_NAMES = [
    "admission_row", "self._receipt", "self._handle", "self.authorize", "self.logical_owner",
    "self.agent", "store.admissions.list", "store.admissions.cancel", "store.sessions.get",
    "store.sessions.store_owner", "store.sessions.backfill_session_owners",
    "store.lineage.stamp_legacy_compression_chains", "store.admissions.record_retired_media",
    "store_location", "restore_retired", "pending_rows", "queued_controls", "wake_bot_receipts",
    "wake_background_wait", "asyncio.create_task", "mark_producer_admission", "event._on_commit",
    "drop_producer_turn", "busy_row_settled", "drop_capture_lease", "admission_media_references",
    "collect_retired_media", "running_key", "adopts_owner_agent", "admission_display", "reset",
    "read", "runner._is_session_running", "runner._hm_evict_idle_stale_agent",
    "runner._hm_evict_reaped_agent", "time.monotonic", "event_stream.publish",
    "controls.register", "controls.register_clarify", "controls.respond",
    "waiter.set_exception", "waiter.set_result", "task.cancelled", "task.exception",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "session_authority_flow.c", encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_session_authority_flow: seams absent from C port: {missing_c}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'(?:rec|arec)\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'ctx\.call\(\s*"([^"]+)"', osrc))
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_session_authority_flow: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "session_authority_flow.jsonl"
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
            print(f"FATAL oracle_session_authority_flow: fixture io keys without a seam: {sorted(bad)}")
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
    print(f"ORACLE session_authority_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


if __name__ == "__main__":
    main()