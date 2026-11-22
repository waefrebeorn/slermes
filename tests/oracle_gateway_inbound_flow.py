#!/usr/bin/env python3
"""Oracle for the session-authority + inbound FLOW port
(src/gateway/gateway_inbound_flow.c).

Drives the LIVE gateway/session_authority.py and gateway/run_inbound.py
decision chains with SCRIPTED seams: every store read, restore call, owner
identity check, pairing-store edit, adapter send, command resolution and i18n
call is a recording stub fed the same scripts the C harness feeds its
universal seam. Compares the DECISION SEQUENCE (ordered seam events
{"name","args","ret"} in "trace") plus the final return ("ret") and the
mutated self/world state ("state").

The live defs are AST-extracted from BOTH target modules (module constants
seeded, Assign AND AnnAssign, plain-Name targets only; the SessionAuthority
and GatewayInboundMixin ClassDefs exec whole with stub mixin bases). Support
dataclasses/exceptions are the REAL ones, AST-extracted from their modules:
session_contract (SessionActor/SessionRef/SessionHandle/AdmissionReceipt),
persistence.core.errors (AuthorityRefusal + _one_line), persistence.authority.
admissions (AdmissionRecord/ClaimedAdmission/GateRefusal + the StrEnums),
gateway.config (Platform, DEFAULT_UNAUTHORIZED_DM_DECLINE_MESSAGE),
gateway.platforms.event (MessageType). Every exec namespace is a registered
sys.modules module (py3.14 dataclasses _is_type needs cls.__module__ resolvable).
The heavy quarry import chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE gateway_inbound_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc), ...}
  (AuthorityRefusal adds reason/field/detail; OSError adds errno when set).
- Platform/MessageType members travel as .value strings; agents/adapters/
  pairing stores as id strings wrapped in attribute doubles when non-empty.
- logger calls are silent on both sides; authority_persistence(self) is a
  silent handle lookup; executing_admission.get() reads state silently;
  asyncio.to_thread runs its callable synchronously.
- self._peek_session_state records the state node copy and returns a backed
  view so update_prompt_pending writes persist into out.state on both sides.
- Scripted restore seams may carry "live": both stubs install that live
  session into sessions[session_id] (the live restore's state effect).
- Fixture corpus folds ASCII only (str.lower/strip) and avoids non-ASCII
  _one_line inputs; UTF-8 len/slice IS exercised (update-prompt labels).
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
os.environ.setdefault("HOME", str(_REPO.parent.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SA_SRC = os.path.join(Q, "gateway", "session_authority.py")
RI_SRC = os.path.join(Q, "gateway", "run_inbound.py")
CONTRACT_SRC = os.path.join(Q, "gateway", "session_contract.py")
ERRORS_SRC = os.path.join(Q, "persistence", "core", "errors.py")
ADMISSIONS_SRC = os.path.join(Q, "persistence", "authority", "admissions.py")
CONFIG_SRC = os.path.join(Q, "gateway", "config.py")
EVENT_SRC = os.path.join(Q, "gateway", "platforms", "event.py")
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


# ── support namespace: real contract dataclasses, errors, records, enums ──
def extract_support():
    mod = new_module("gif_oracle_support")
    ns = mod.__dict__
    ns.update({"dataclass": dataclass, "field": field, "asdict": asdict,
               "Enum": Enum, "StrEnum": StrEnum, "Any": Any, "Optional": Optional,
               "Mapping": Mapping, "contextlib": contextlib})
    # session_contract: lightweight (dataclasses + typing only) — exec whole
    csrc = open(CONTRACT_SRC, encoding="utf-8").read()
    cmod = new_module("gif_oracle_contract")
    cmod.__dict__.update({"dataclass": dataclass, "field": field})
    exec(compile(csrc, CONTRACT_SRC, "exec", flags=_FLAGS, dont_inherit=True), cmod.__dict__)
    for name in ("SessionActor", "SessionRef", "SessionHandle", "AdmissionReceipt",
                 "PendingAdmission", "SubscriptionSnapshot", "Submission",
                 "CANONICAL_GATEWAY_PROTOCOL"):
        ns[name] = getattr(cmod, name)
    # errors: the real AuthorityRefusal family + _one_line
    esrc = open(ERRORS_SRC, encoding="utf-8").read()
    for node in class_nodes(esrc, ["PersistenceError", "TransientStoreError",
                                   "PermanentStoreError", "StoreUnavailable",
                                   "StoreReplaced", "CorruptStoreError",
                                   "AuthorityRefusal"]):
        exec_node(esrc, node, ns, ERRORS_SRC)
    for node in func_nodes(esrc):
        if node.name == "_one_line":
            exec_node(esrc, node, ns, ERRORS_SRC)
    seed_constants(esrc, ns, ERRORS_SRC)  # STORE_FAILURES et al
    # admissions: the StrEnums + record dataclasses + GateRefusal
    asrc = open(ADMISSIONS_SRC, encoding="utf-8").read()
    for node in class_nodes(asrc, ["AdmissionIntent", "AdmissionStatus",
                                   "AdmissionOutcome", "AdmissionRecord",
                                   "ClaimedAdmission", "GateRefusal"]):
        exec_node(asrc, node, ns, ADMISSIONS_SRC)
    # Platform enum + the decline default (config.py is heavy: extract only)
    gsrc = open(CONFIG_SRC, encoding="utf-8").read()
    for node in class_nodes(gsrc, ["Platform"]):
        exec_node(gsrc, node, ns, CONFIG_SRC)
    seed_constants(gsrc, ns, CONFIG_SRC)
    # MessageType enum
    evsrc = open(EVENT_SRC, encoding="utf-8").read()
    for node in class_nodes(evsrc, ["MessageType"]):
        exec_node(evsrc, node, ns, EVENT_SRC)
    # STEERING
    vsrc = open(VOCAB_SRC, encoding="utf-8").read()
    seed_constants(vsrc, ns, VOCAB_SRC)
    required = ["SessionActor", "SessionRef", "SessionHandle", "AdmissionReceipt",
                "AuthorityRefusal", "GateRefusal", "AdmissionRecord",
                "ClaimedAdmission", "AdmissionIntent", "AdmissionStatus",
                "Platform", "MessageType", "STEERING",
                "DEFAULT_UNAUTHORIZED_DM_DECLINE_MESSAGE", "_one_line"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_gateway_inbound_flow: support missing: {missing}")
        sys.exit(2)
    return ns


SUP = extract_support()


# ── session_authority namespace ──
def extract_sa():
    mod = new_module("gif_oracle_sa")
    ns = mod.__dict__
    import asyncio
    import sqlite3
    import threading
    import uuid
    import logging
    from contextvars import ContextVar
    from functools import partial
    ns.update({"asyncio": asyncio, "contextlib": contextlib, "ContextVar": ContextVar,
               "dataclass": dataclass, "field": field, "asdict": asdict,
               "partial": partial, "sqlite3": sqlite3, "threading": threading,
               "SimpleNamespace": types.SimpleNamespace, "uuid": uuid,
               "logging": logging, "json": json})
    for name in ("CANONICAL_GATEWAY_PROTOCOL", "AdmissionReceipt", "PendingAdmission",
                 "SessionActor", "SessionHandle", "SessionRef", "Submission",
                 "SubscriptionSnapshot", "GateRefusal", "STEERING", "STORE_FAILURES",
                 "AuthorityRefusal", "CorruptStoreError", "StoreReplaced"):
        ns[name] = SUP[name]
    # collaborators replaced by per-case recording stubs
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
        print(f"FATAL oracle_gateway_inbound_flow: session_authority missing: {missing}")
        sys.exit(2)
    return ns


NS_SA = extract_sa()


# ── run_inbound namespace ──
def extract_ri():
    mod = new_module("gif_oracle_ri")
    ns = mod.__dict__
    import concurrent.futures
    import logging
    import time
    from contextlib import suppress

    async def _to_thread(fn, *a, **k):
        return fn(*a, **k)

    ns.update({"logging": logging, "TYPE_CHECKING": False,
               "asyncio": types.SimpleNamespace(to_thread=_to_thread),
               "concurrent": concurrent.futures, "dataclasses": __import__("dataclasses"),
               "json": json, "os": os, "re": re, "time": time,
               "suppress": suppress, "Path": Path,
               "Any": Any, "Dict": dict, "List": list, "Optional": Optional,
               "Tuple": tuple, "Platform": SUP["Platform"],
               "MessageType": SUP["MessageType"]})
    ns["logger"] = _SilentLogger()
    ns["t"] = lambda key, **k: key
    ns["reply_pointer"] = lambda *a, **k: ""
    ns["EphemeralReply"] = type("EphemeralReply", (), {})
    ns["MessageEvent"] = type("MessageEvent", (), {})
    ns["clean_attachment_filename"] = lambda name: name or ""
    ns["approval_input_words"] = lambda input_key: []
    ns["rehome_inbound_media"] = lambda event: None
    for base in ("GatewayInboundHooksMixin", "GatewayInboundTranscriptionMixin",
                 "GatewayPluginInjectionMixin", "GatewayInboundMoaMixin"):
        ns[base] = type(base, (), {})
    ns["cancel_preagent_setup"] = lambda *a, **k: None
    ns["pending_setup_busy_notice"] = lambda *a, **k: None
    ns["UnauthorizedOwnerNotifier"] = type("UnauthorizedOwnerNotifier", (), {})
    ns["pairing_code_reply"] = lambda *a, **k: ""
    ns["pairing_profile_arg"] = lambda *a, **k: ""
    ns["pairing_rate_limited_reply"] = lambda *a, **k: ""
    ns["unauthorized_owner_hint"] = lambda *a, **k: ""
    ns["SessionSource"] = type("SessionSource", (), {})
    ns["build_session_context"] = lambda *a, **k: {}
    ns["label_shared_sender"] = lambda source, text, **k: text
    ns["TurnLeaseTimeoutError"] = type("TurnLeaseTimeoutError", (Exception,), {})
    for name in ("note_busy_admitting", "note_busy_outcome", "settle_busy_arrival",
                 "stamp_busy_arrival"):
        ns[name] = lambda *a, **k: None
    src = open(RI_SRC, encoding="utf-8").read()
    seed_constants(src, ns, RI_SRC)
    for node in class_nodes(src, ["GatewayInboundMixin"]):
        exec_node(src, node, ns, RI_SRC)
    # These two live at module level in run_inbound.py, not on the mixin.
    for node in func_nodes(src):
        if node.name in ("discord_triggering_note", "strip_discord_triggering_note"):
            exec_node(src, node, ns, RI_SRC)
    seed_constants(src, ns, RI_SRC)
    ns["logger"] = _SilentLogger()
    required = ["GatewayInboundMixin", "discord_triggering_note",
                "strip_discord_triggering_note"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_gateway_inbound_flow: run_inbound missing: {missing}")
        sys.exit(2)
    mixin = ns["GatewayInboundMixin"]
    for meth in ("_hm_estop_turn_allowed", "_hm_estop_gate", "_hm_update_prompt_reply",
                 "_hm_slash_confirm_reply", "_hm_busy_slash_or_photo", "_hm_admit_event",
                 "_hm_send_unauthorized_decline", "_slash_confirm_text_choices"):
        if not hasattr(mixin, meth):
            print(f"FATAL oracle_gateway_inbound_flow: mixin missing {meth}")
            sys.exit(2)
    return ns


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


NS_RI = extract_ri()
# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
        if is_dataclass(x) and not isinstance(x, type):
            return conv(asdict(x))
        if isinstance(x, enum.Enum):
            return x.value
        if isinstance(x, StateNS):
            return conv(object.__getattribute__(x, "_d"))
        if isinstance(x, EventDouble):
            return norm_event(x)
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, frozenset):
            return sorted((conv(i) for i in x), key=lambda z: json.dumps(z, sort_keys=True))
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception materialization / projection ──
EXC_CLASSES = {
    "OSError": OSError, "TimeoutError": TimeoutError, "PermissionError": PermissionError,
    "FileNotFoundError": FileNotFoundError, "FileExistsError": FileExistsError,
    "BlockingIOError": BlockingIOError, "ValueError": ValueError, "TypeError": TypeError,
    "RuntimeError": RuntimeError, "KeyError": KeyError, "AttributeError": AttributeError,
    "ImportError": ImportError, "Exception": Exception, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "AuthorityRefusal": SUP["AuthorityRefusal"],
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
    en = spec.get("errno")
    if en is not None and issubclass(cls, OSError):
        return cls(en, msg)
    return cls(msg)


def proj_exc(exc):
    d = {"__raise__": type(exc).__name__, "message": str(exc)}
    if isinstance(exc, SUP["AuthorityRefusal"]):
        d["reason"] = exc.reason
        d["field"] = exc.field
        d["detail"] = exc.detail
    if isinstance(exc, OSError) and exc.errno is not None:
        d["errno"] = exc.errno
    return d


# ── per-case script backend (mirrors the C gif_call exactly) ──
_MISSING = object()
RESTORE_SEAMS = {"restore_local_session", "restore_routed_session",
                 "mount_history", "restore_api_session"}


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []
        self.live_sink = None  # (sessions_dict, make_live(spec, sid))

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
        if (isinstance(used, dict) and used.get("live") and name in RESTORE_SEAMS
                and self.live_sink is not None):
            sessions, make_live = self.live_sink
            sid = args.get("session_id")
            if sid is not None:
                sessions[sid] = make_live(used["live"], sid)
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
    """A dict-backed attribute view: reads wrap nested dicts, writes persist."""
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


class EventDouble:
    def __init__(self, d, ctx):
        dd = dict(d)
        if "source" in dd:
            dd["source"] = make_source(dd["source"])
        if dd.get("message_type"):
            dd["message_type"] = SUP["MessageType"](dd["message_type"])
        object.__setattr__(self, "_d", dd)
        object.__setattr__(self, "_ctx", ctx)

    def __getattr__(self, k):
        d = object.__getattribute__(self, "_d")
        if k in d:
            return d[k]
        raise AttributeError(k)

    def get_command(self):
        return object.__getattribute__(self, "_ctx").call("event.get_command", {})


def norm_event(ev):
    d = dict(object.__getattribute__(ev, "_d"))
    if "source" in d:
        d["source"] = norm_source(d["source"])
    mt = d.get("message_type")
    if isinstance(mt, enum.Enum):
        d["message_type"] = mt.value
    return d


class AgentDouble:
    def __init__(self, spec, ctx):
        self.id = spec.get("id")
        self._ctx = ctx
        if spec.get("has_clear"):
            self.clear_interrupt = lambda: ctx.call("agent.clear_interrupt", {"agent": self.id})

    def steer(self, text):
        return self._ctx.call("agent.steer", {"agent": self.id, "text": text})


class PairingStoreDouble:
    def __init__(self, id_, ctx):
        self.id = id_
        self._ctx = ctx

    def has_recent_decline(self, platform, user_id):
        return self._ctx.call("pairing_store.has_recent_decline",
                              {"platform": platform, "user_id": user_id})

    def record_decline(self, platform, user_id):
        return self._ctx.call("pairing_store.record_decline",
                              {"platform": platform, "user_id": user_id})


class AdapterDouble:
    def __init__(self, id_, ctx):
        self.id = id_
        self._ctx = ctx

    async def send(self, chat_id, text, **kw):
        return self._ctx.call("adapter.send", {"chat_id": chat_id, "text": text})


def make_record(d):
    if d is None:
        return None
    if "transcript_mark" in d:
        return SUP["ClaimedAdmission"](**d)
    return SUP["AdmissionRecord"](**d)


def make_live(spec, ctx, sid):
    es = types.SimpleNamespace(
        lock=NoopCM(), execution={},
        note_progress=lambda generation, event_type, payload: ctx.call(
            "event_stream.note_progress",
            {"session_id": sid, "generation": generation,
             "event_type": event_type, "payload": payload}),
        publish=lambda session_id, payload, event_type=None: ctx.call(
            "event_stream.publish",
            {"session_id": session_id, "payload": payload, "event_type": event_type}),
    )
    return types.SimpleNamespace(route=spec.get("route"),
                                 source=make_source(spec.get("source")),
                                 settling=dict(spec.get("settling") or {}),
                                 event_stream=es, session_id=sid)


# ── sys.modules stubs for in-function imports (both levels registered) ──
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
# ── session_authority drivers ──
def setup_authority(args, state, ctx):
    runner = types.SimpleNamespace(
        _draining=(state.get("runner") or {}).get("_draining", False),
        session_store=types.SimpleNamespace(
            lookup_by_session_key=rec(
                ctx, "runner.session_store.lookup_by_session_key",
                args_fn=lambda route: {"route": route},
                build=lambda r: None if r is None else types.SimpleNamespace(**r))))
    auth = NS_SA["SessionAuthority"](runner, profile_id=state.get("profile_id", "p1"),
                                     instance_id=state.get("instance_id", "i1"),
                                     epoch=state.get("epoch", 1), persistence=None)
    auth.retiring = state.get("retiring", False)
    auth.sessions = {sid: make_live(spec, ctx, sid)
                     for sid, spec in (state.get("sessions") or {}).items()}
    auth.adopted = {sid: (v[0], AgentDouble({"id": v[1]}, ctx) if v[1] is not None else None)
                    for sid, v in (state.get("adopted") or {}).items()}
    auth.delivered_stops = dict(state.get("delivered_stops") or {})
    auth.pending_stops = dict(state.get("pending_stops") or {})
    auth.pending_steers = {sid: (v[0], list(v[1]))
                           for sid, v in (state.get("pending_steers") or {}).items()}
    auth.executing = dict(state.get("executing") or {})
    auth.rotated_owners = dict(state.get("rotated_owners") or {})
    ctx.live_sink = (auth.sessions, lambda spec, sid: make_live(spec, ctx, sid))

    store = types.SimpleNamespace(
        sessions=types.SimpleNamespace(get=rec(
            ctx, "store.sessions.get",
            args_fn=lambda session_id: {"session_id": session_id},
            build=lambda r: None if r is None else types.SimpleNamespace(**r))),
        bindings=types.SimpleNamespace(binding_marks=rec(
            ctx, "store.bindings.binding_marks",
            args_fn=lambda session_id: {"session_id": session_id},
            build=lambda r: types.SimpleNamespace(**r))),
        admissions=types.SimpleNamespace(list=rec(
            ctx, "store.admissions.list",
            args_fn=lambda session_id: {"session_id": session_id},
            build=lambda rows: [make_record(d) for d in (rows if isinstance(rows, list) else [rows])])),
        lineage=types.SimpleNamespace(
            lineage=rec(ctx, "lineage.lineage",
                        args_fn=lambda session_id: {"session_id": session_id}),
            tip=rec(ctx, "lineage.tip",
                    args_fn=lambda session_id: {"session_id": session_id})))
    NS_SA["authority_persistence"] = lambda a: store

    def agent_arg(agent):
        return {"agent": agent.id if isinstance(agent, AgentDouble) else agent}

    stub_modules({
        "gateway.session_authorities": {
            "require_serving": rec(ctx, "require_serving", args_fn=lambda a: {})},
        "gateway.drain_control": {
            "restart_wait_open": rec(ctx, "restart_wait_open", args_fn=lambda r: {})},
        "gateway.session_local_recovery": {
            "restore_local_session": rec(ctx, "restore_local_session",
                                         args_fn=lambda a, sid: {"session_id": sid})},
        "gateway.session_route_recovery": {
            "restore_routed_session": rec(ctx, "restore_routed_session",
                                          args_fn=lambda a, sid: {"session_id": sid})},
        "gateway.session_history_mount": {
            "mount_history": rec(ctx, "mount_history",
                                 args_fn=lambda a, actor, sid: {"actor": canon(actor),
                                                                "session_id": sid}),
            "is_history_mount": rec(ctx, "is_history_mount",
                                    args_fn=lambda live: {"session_id": live.session_id}),
            "require_history_access": rec(ctx, "require_history_access",
                                          args_fn=lambda a, actor, sid, cap: {
                                              "actor": canon(actor), "session_id": sid,
                                              "capability": cap})},
        "gateway.session_api": {
            "restore_api_session": rec(ctx, "restore_api_session",
                                       args_fn=lambda a, sid: {"session_id": sid})},
        "gateway.owner_identity": {
            "same_owner": rec(ctx, "same_owner", args_fn=lambda a, b: {"a": a, "b": b})},
        "gateway.session_retirement": {
            "touch_restored": rec(ctx, "touch_restored",
                                  args_fn=lambda a, sid: {"session_id": sid})},
        "agent.interrupt_compat": {
            "request_hard_interrupt": rec(ctx, "request_hard_interrupt",
                                          args_fn=agent_arg)},
        "gateway.session_api_turn": {
            "publish_api_event": rec(ctx, "publish_api_event",
                                     args_fn=lambda a, sid, et, payload: {
                                         "session_id": sid, "event_type": et,
                                         "payload": payload})},
        "gateway.config": {"Platform": SUP["Platform"]},
    })
    return auth


def project_authority_state(state, auth):
    final = canon(state)
    final["retiring"] = auth.retiring
    final["sessions"] = {sid: {"route": l.route, "source": norm_source(l.source),
                               "settling": l.settling}
                         for sid, l in auth.sessions.items()}
    final["adopted"] = {sid: [g, (a.id if isinstance(a, AgentDouble) else a)]
                        for sid, (g, a) in auth.adopted.items()}
    final["delivered_stops"] = dict(auth.delivered_stops)
    final["pending_stops"] = dict(auth.pending_stops)
    final["pending_steers"] = {sid: [g, list(t)]
                               for sid, (g, t) in auth.pending_steers.items()}
    final["executing"] = dict(auth.executing)
    final["rotated_owners"] = dict(auth.rotated_owners)
    return final


def invoke_authority(func, args, auth, ctx):
    if func == "admission_row":
        return NS_SA["admission_row"](make_record(args.get("record")))
    if func == "_require_admission_open":
        return auth._require_admission_open(queue_only=args.get("queue_only", False))
    if func == "gate_refusal":
        check = (lambda: ctx.call("check", {})) if args.get("check") else None
        return auth.gate_refusal(check, queue_only=args.get("queue_only", False))
    if func == "logical_owner":
        return auth.logical_owner(args.get("session_id"))
    if func == "physical_target":
        return auth.physical_target(SUP["SessionRef"](**args["ref"]))
    if func == "_current_owner":
        return auth._current_owner(args.get("session_id"))
    if func == "_receipt":
        return auth._receipt(args["row"])
    if func == "_handle":
        return auth._handle(SUP["SessionRef"](**args["ref"]))
    if func == "authorize":
        a = args["actor"]
        actor = SUP["SessionActor"](subject=a["subject"], profile_id=a["profile_id"],
                                    capabilities=frozenset(a["capabilities"]),
                                    transport_id=a["transport_id"])
        return auth.authorize(actor, SUP["SessionRef"](**args["ref"]), args["capability"])
    if func == "adopt_agent":
        spec = args.get("agent")
        agent = AgentDouble(spec, ctx) if spec is not None else None
        return auth.adopt_agent(args["session_id"], args["generation"], agent)
    if func == "publish_execution":
        return auth.publish_execution(args["session_id"], args["generation"],
                                      args["event_type"], args["payload"])
    raise RuntimeError(f"unknown authority func {func}")


SA_FUNCS = {"admission_row", "_require_admission_open", "gate_refusal", "logical_owner",
            "physical_target", "_current_owner", "_receipt", "_handle", "authorize",
            "adopt_agent", "publish_execution"}
# ── run_inbound drivers ──
def setup_inbound(args, state, ctx):
    self = object.__new__(NS_RI["GatewayInboundMixin"])
    self.config = StateNS(state["config"]) if state.get("config") is not None else None
    self._startup_restore_in_progress = state.get("_startup_restore_in_progress", False)
    self._draining = state.get("_draining", False)

    NS_RI["t"] = lambda key, **kw: ctx.call("t", {"key": key, **kw})
    NS_RI["approval_input_words"] = rec(ctx, "approval_input_words",
                                        args_fn=lambda input_key: {"input_key": input_key})
    NS_RI["logger"] = _SilentLogger()

    def ev_arg(event):
        return {"event": norm_event(event)}

    def src_arg(source):
        return {"source": norm_source(source)}

    def peek(key):
        node = (state.get("session_states") or {}).get(key)
        ctx.trace.append({"name": "self._peek_session_state", "args": {"key": key},
                          "ret": canon(node) if node is not None else None})
        return StateNS(node) if node is not None else None

    self._session_key_for_source = lambda source: ctx.call(
        "self._session_key_for_source", src_arg(source))
    self._peek_session_state = peek
    self._hm_update_reply_refused = lambda event: ctx.call(
        "self._hm_update_reply_refused", ev_arg(event))
    self._is_session_running = lambda key: ctx.call("self._is_session_running", {"key": key})
    self._hm_write_update_response = lambda response_text: ctx.call(
        "self._hm_write_update_response", {"response_text": response_text})
    self._refused_slash_confirm = lambda event, session_key, confirm_id: ctx.call(
        "self._refused_slash_confirm",
        {"event": norm_event(event), "session_key": session_key, "confirm_id": confirm_id})
    self._handle_status_command = arec(ctx, "self._handle_status_command", args_fn=ev_arg)
    self._handle_context_command = arec(ctx, "self._handle_context_command", args_fn=ev_arg)
    self._check_slash_access = lambda source, command: ctx.call(
        "self._check_slash_access", {"source": norm_source(source), "command": command})
    self._hm_drop_held_input = lambda event: ctx.call("self._hm_drop_held_input", ev_arg(event))
    self._hm_stop_owner_turn = arec(ctx, "self._hm_stop_owner_turn",
                                    args_fn=lambda key: {"key": key})
    self._dispatch_busy_slash_command = arec(
        ctx, "self._dispatch_busy_slash_command",
        args_fn=lambda event, cmd_def, key, source: {
            "event": norm_event(event), "cmd_def": vars(cmd_def),
            "key": key, "source": norm_source(source)})
    self._hm_merge_pending_for_source = lambda source, key, event, merge_text=False: ctx.call(
        "self._hm_merge_pending_for_source",
        {"source": norm_source(source), "key": key, "event": norm_event(event)})
    self._canonicalize = lambda source: ctx.call("self._canonicalize", src_arg(source))
    self._intake_adapter_for = lambda source: ctx.call(
        "self._intake_adapter_for", src_arg(source))
    self._queue_startup_restore_event = lambda event: ctx.call(
        "self._queue_startup_restore_event", ev_arg(event))
    self._scale_to_zero_note_real_inbound = lambda: ctx.call(
        "self._scale_to_zero_note_real_inbound", {})
    self._hm_pre_gateway_dispatch_hook = arec(
        ctx, "self._hm_pre_gateway_dispatch_hook",
        args_fn=lambda event, source: {"event": norm_event(event),
                                       "source": norm_source(source)},
        build=lambda raw: None if raw is None else EventDouble(raw, ctx))
    self._is_user_authorized_for_source = lambda source: ctx.call(
        "self._is_user_authorized_for_source", src_arg(source))
    self._get_unauthorized_dm_behavior = lambda platform, profile=None: ctx.call(
        "self._get_unauthorized_dm_behavior",
        {"platform": platform.value if isinstance(platform, enum.Enum) else platform,
         "profile": profile})
    self._hm_offer_pairing_code = arec(ctx, "self._hm_offer_pairing_code", args_fn=src_arg)
    # _hm_send_unauthorized_decline is the real extracted LIVE method; the C
    # port implements its body (pairing-store dedupe, stamp before send).
    self._hm_report_ignored_dm = arec(ctx, "self._hm_report_ignored_dm", args_fn=src_arg)
    self._admit_bot_message_for_source = lambda source: ctx.call(
        "self._admit_bot_message_for_source", src_arg(source))
    self._admit_bot_chain = lambda event: ctx.call("self._admit_bot_chain", ev_arg(event))
    self._pairing_store_for = lambda source: (
        lambda raw: PairingStoreDouble(raw, ctx) if raw else raw)(
            ctx.call("self._pairing_store_for", src_arg(source)))
    self._delivery_adapter_for = lambda source: (
        lambda raw: AdapterDouble(raw, ctx) if raw else raw)(
            ctx.call("self._delivery_adapter_for", src_arg(source)))

    stub_modules({
        "hermes_cli.commands": {
            "resolve_command": rec(ctx, "resolve_command",
                                   args_fn=lambda cmd: {"cmd": cmd},
                                   build=lambda r: None if r is None
                                   else types.SimpleNamespace(**r))},
        "agent.estop": {
            "paused_reply": rec(ctx, "agent.estop.paused_reply", args_fn=lambda: {})},
        "tools.slash_confirm": {
            "get_pending": rec(ctx, "slash_confirm.get_pending",
                               args_fn=lambda key: {"key": key}),
            "resolve": arec(ctx, "slash_confirm.resolve",
                            args_fn=lambda key, confirm_id, choice: {
                                "key": key, "confirm_id": confirm_id, "choice": choice}),
            "clear_if_stale": rec(ctx, "slash_confirm.clear_if_stale",
                                  args_fn=lambda key: {"key": key})},
        "tools.approval": {
            "has_blocking_approval": rec(ctx, "approval.has_blocking_approval",
                                         args_fn=lambda key: {"key": key})},
        "gateway.session_ingress": {
            "executing_admission": types.SimpleNamespace(
                get=lambda: state.get("executing_admission"))},
        "gateway.session_context": {
            "reset_session_vars": rec(ctx, "reset_session_vars", args_fn=lambda: {})},
        "gateway.parked_route_refusal": {
            "drop_rejected_route": rec(ctx, "drop_rejected_route",
                                       args_fn=lambda adapter, source: {
                                           "adapter": adapter,
                                           "source": norm_source(source)})},
        "gateway.run": {
            "_is_slack_ignored_channel": rec(
                ctx, "_is_slack_ignored_channel",
                args_fn=lambda config, chat_id, adapter: {
                    "config": canon(config) if config is not None else None,
                    "chat_id": chat_id, "adapter": adapter})},
        "gateway.config": {
            "Platform": SUP["Platform"],
            "DEFAULT_UNAUTHORIZED_DM_DECLINE_MESSAGE":
                SUP["DEFAULT_UNAUTHORIZED_DM_DECLINE_MESSAGE"]},
    })
    return self


def invoke_inbound(func, args, self, ctx):
    if func == "discord_triggering_note":
        return NS_RI["discord_triggering_note"](args.get("message_id"))
    if func == "strip_discord_triggering_note":
        ev = EventDouble(args.get("event") or {}, ctx)
        return NS_RI["strip_discord_triggering_note"](ev, args.get("message_text"))
    if func == "_hm_estop_turn_allowed":
        ev = EventDouble(args.get("event") or {}, ctx)
        return self._hm_estop_turn_allowed(ev, make_source(args.get("source")))
    if func == "_hm_estop_gate":
        ev = EventDouble(args.get("event") or {}, ctx)
        return self._hm_estop_gate(ev, make_source(args.get("source")),
                                   args.get("is_internal", False))
    if func == "_hm_update_prompt_reply":
        ev = EventDouble(args.get("event") or {}, ctx)
        return self._hm_update_prompt_reply(ev, args["key"])
    if func == "_hm_slash_confirm_reply":
        ev = EventDouble(args.get("event") or {}, ctx)
        return _asyncio.run(self._hm_slash_confirm_reply(ev, args["key"]))
    if func == "_hm_busy_slash_or_photo":
        ev = EventDouble(args.get("event") or {}, ctx)
        return _asyncio.run(self._hm_busy_slash_or_photo(
            ev, make_source(args.get("source")), args["key"]))
    if func == "_hm_admit_event":
        ev = EventDouble(args.get("event") or {}, ctx)
        result = _asyncio.run(self._hm_admit_event(ev))
        if result is None:
            return None
        event, source, is_internal = result
        return [norm_event(event), norm_source(source), is_internal]
    if func == "_hm_send_unauthorized_decline":
        return _asyncio.run(self._hm_send_unauthorized_decline(make_source(args.get("source"))))
    raise RuntimeError(f"unknown inbound func {func}")


RI_FUNCS = {"discord_triggering_note", "strip_discord_triggering_note",
            "_hm_estop_turn_allowed", "_hm_estop_gate", "_hm_update_prompt_reply",
            "_hm_slash_confirm_reply", "_hm_busy_slash_or_photo", "_hm_admit_event",
            "_hm_send_unauthorized_decline"}
RI_ASYNC = {"_hm_slash_confirm_reply", "_hm_busy_slash_or_photo", "_hm_admit_event",
            "_hm_send_unauthorized_decline"}


# ── per-case drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    state = canon(inp.get("state") or {})
    ctx = Ctx(inp.get("io"))
    if func in SA_FUNCS:
        auth = setup_authority(args, state, ctx)
        try:
            ret = invoke_authority(func, args, auth, ctx)
        except BaseException as exc:  # SystemExit included
            ret = proj_exc(exc)
        final = project_authority_state(state, auth)
    elif func in RI_FUNCS:
        self = setup_inbound(args, state, ctx)
        try:
            ret = invoke_inbound(func, args, self, ctx)
        except BaseException as exc:
            ret = proj_exc(exc)
        final = state
    else:
        raise RuntimeError(f"unknown func {func}")
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(final)}
# ── three-way seam-name cross-check (C port ↔ oracle stubs ↔ fixture io) ──
SEAM_NAMES = [
    # session_authority seams
    "require_serving", "restart_wait_open", "store.sessions.get",
    "store.bindings.binding_marks", "store.admissions.list",
    "restore_local_session", "restore_routed_session", "mount_history",
    "restore_api_session", "is_history_mount", "require_history_access",
    "same_owner", "touch_restored", "lineage.lineage", "lineage.tip",
    "runner.session_store.lookup_by_session_key", "check",
    "agent.clear_interrupt", "agent.steer", "request_hard_interrupt",
    "event_stream.note_progress", "event_stream.publish", "publish_api_event",
    # run_inbound seams
    "event.get_command", "resolve_command", "self._session_key_for_source",
    "self._peek_session_state", "self._hm_update_reply_refused",
    "self._is_session_running", "slash_confirm.get_pending",
    "approval.has_blocking_approval", "agent.estop.paused_reply",
    "self._hm_write_update_response", "t", "approval_input_words",
    "self._refused_slash_confirm", "slash_confirm.resolve",
    "slash_confirm.clear_if_stale", "self._handle_status_command",
    "self._handle_context_command", "self._check_slash_access",
    "self._hm_drop_held_input", "self._hm_stop_owner_turn",
    "self._dispatch_busy_slash_command", "self._hm_merge_pending_for_source",
    "reset_session_vars", "self._canonicalize", "self._intake_adapter_for",
    "drop_rejected_route", "_is_slack_ignored_channel",
    "self._queue_startup_restore_event", "self._scale_to_zero_note_real_inbound",
    "self._hm_pre_gateway_dispatch_hook", "self._is_user_authorized_for_source",
    "self._get_unauthorized_dm_behavior", "self._hm_offer_pairing_code",
    "self._hm_report_ignored_dm",
    "self._admit_bot_message_for_source", "self._admit_bot_chain",
    "self._pairing_store_for", "pairing_store.has_recent_decline",
    "pairing_store.record_decline", "self._delivery_adapter_for", "adapter.send",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "gateway_inbound_flow.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_gateway_inbound_flow: seams absent from C port: {missing_c}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'(?:rec|arec)\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'ctx\.call\(\s*"([^"]+)"', osrc))
    used |= {"self._peek_session_state"}
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_gateway_inbound_flow: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "gateway_inbound_flow.jsonl"
    if fix.exists():
        fix_keys = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
        bad = fix_keys - registry - {"self._hm_send_unauthorized_decline"}
        if bad:
            print(f"FATAL oracle_gateway_inbound_flow: fixture io keys without a seam: {sorted(bad)}")
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
    print(f"ORACLE gateway_inbound_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


if __name__ == "__main__":
    main()
