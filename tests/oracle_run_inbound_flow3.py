#!/usr/bin/env python3
"""Oracle for the UNCLAIMED remainder FLOW of gateway/run_inbound.py
(src/gateway/run_inbound_flow3.c, third wave after gateway_inbound_flow /
run_inbound_flow claimed the ingress/busy/clarify-release cores and
gateway_inbound_flow2 claimed command dispatch / admission).

Drives the LIVE GatewayInboundMixin methods with SCRIPTED seams: every
pairing-store / adapter / authority / activity / config / model-metadata /
vision / filesystem / i18n / clock call is a recording stub fed the same
scripts the C harness feeds its universal seam. Compares the DECISION
SEQUENCE (ordered seam events {"name","args","ret"} in "trace") plus the
final return ("ret"), the mutated world state ("state") and the possibly
mutated INPUT event ("event").

The live defs are AST-extracted from gateway/run_inbound.py (the whole
GatewayInboundMixin ClassDef execs with stub mixin bases; module constants
seeded, Assign AND AnnAssign, plain-Name targets only). Support types are
the REAL ones, AST-extracted from their modules: gateway.config.Platform,
gateway.platforms.event.MessageType, persistence.core.errors
(AuthorityRefusal family + _one_line), gateway.session_contract
(SessionRef / SessionActor frozen dataclasses). Every exec namespace is a
registered sys.modules module (py3.14 dataclasses _is_type needs
cls.__module__ resolvable). The heavy quarry import chain is never imported.

Claimed siblings (flow1/flow2 ports) and unported collaborators are SEAMED
as "self.<name>" instance recorders (or module-level recorder overrides);
the 18 ported methods stay LIVE on the class and compose through normal
attribute lookup (_hm_clarify_reply -> _prepare_clarify_reply_text; the two
note prependers -> _inbound_attachment_display_name).

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE run_inbound_flow3: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc), ...}
  (AuthorityRefusal adds reason/field/detail; OSError adds errno when set).
- Platform/MessageType members travel as .value strings; events/sources as
  attribute dicts (EventDouble / SimpleNamespace materialization).
- Stores/notifiers travel as id strings; adapters as {"id","retire"} dicts
  (per-case class carries retire_clarify_card only when retire is true);
  agents as the sentinel token or {"id","activity"} dicts (per-case class
  carries get_activity_summary only when activity is true).
- Authorities as {"id","profile_id","sessions","_ordered_writes"} dicts;
  rows/handles/pending/send-results as SimpleNamespace-wrapped dicts.
- SessionRef/SessionActor construction silent (real frozen dataclasses);
  EphemeralReply is a str subclass (travels as plain text).
- logger calls silent; asyncio.to_thread synchronous and untraced.
- _session_state get-or-create / _peek_session_state peek STATE accessors
  (traced, borrowed views; StateNS bare-key AttributeError on misses).
- scoped_runtime_main sync CM (ctor + __enter__/__exit__ seams, truthy exit
  suppresses); ordered async CM (__aenter__/__aexit__ seams).
- Fixture corpus ASCII; integral numbers at str() sites; no NUL strings
  (SKIPPED responses outside the C domain — behaviorally == falsy response).
"""
import ast
import asyncio as _asyncio
import contextlib
import enum
import json
import os
import re
import sys
import textwrap
import types
from dataclasses import asdict, is_dataclass
from pathlib import Path
from typing import Any, Optional
from contextlib import suppress
import __future__ as _fut

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
RI_SRC = os.path.join(Q, "gateway", "run_inbound.py")
CONFIG_SRC = os.path.join(Q, "gateway", "config.py")
EVENT_SRC = os.path.join(Q, "gateway", "platforms", "event.py")
ERRORS_SRC = os.path.join(Q, "persistence", "core", "errors.py")
CONTRACT_SRC = os.path.join(Q, "gateway", "session_contract.py")

_FLAGS = _fut.annotations.compiler_flag
SENTINEL_TOKEN = "__agent_pending_sentinel__"
SENTINEL = object()   # _AGENT_PENDING_SENTINEL identity marker
SKIPPED = "\x00skipped"


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


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


# ── support namespace: real enums + error families + contract dataclasses ──
def extract_support():
    mod = new_module("ri3_oracle_support")
    ns = mod.__dict__
    import dataclasses
    import typing
    ns.update({"enum": enum, "Enum": enum.Enum, "StrEnum": enum.StrEnum,
               "contextlib": contextlib, "Any": Any, "Optional": Optional,
               "dataclass": dataclasses.dataclass, "field": dataclasses.field,
               "Literal": typing.Literal, "Mapping": typing.Mapping,
               "Protocol": typing.Protocol})
    gsrc = open(CONFIG_SRC, encoding="utf-8").read()
    for node in class_nodes(gsrc, ["Platform"]):
        exec_node(gsrc, node, ns, CONFIG_SRC)
    evsrc = open(EVENT_SRC, encoding="utf-8").read()
    for node in class_nodes(evsrc, ["MessageType"]):
        exec_node(evsrc, node, ns, EVENT_SRC)
    esrc = open(ERRORS_SRC, encoding="utf-8").read()
    for node in class_nodes(esrc, ["PersistenceError", "TransientStoreError",
                                   "PermanentStoreError", "StoreUnavailable",
                                   "StoreReplaced", "CorruptStoreError",
                                   "AuthorityRefusal"]):
        exec_node(esrc, node, ns, ERRORS_SRC)
    for node in ast.parse(esrc).body:
        if isinstance(node, ast.FunctionDef) and node.name == "_one_line":
            exec_node(esrc, node, ns, ERRORS_SRC)
    seed_constants(esrc, ns, ERRORS_SRC)
    csrc = open(CONTRACT_SRC, encoding="utf-8").read()
    for node in class_nodes(csrc, ["SessionActor", "SessionRef"]):
        exec_node(csrc, node, ns, CONTRACT_SRC)
    required = ["Platform", "MessageType", "AuthorityRefusal", "StoreUnavailable",
                "_one_line", "SessionRef", "SessionActor"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_run_inbound_flow3: support missing: {missing}")
        sys.exit(2)
    return ns


SUP = extract_support()


# ── run_inbound namespace ──
def extract_ri():
    mod = new_module("ri3_oracle_ri")
    ns = mod.__dict__
    import concurrent.futures
    import logging
    import time as _time

    async def _to_thread(fn, *a, **k):
        return fn(*a, **k)

    class EphemeralReply(str):
        def __new__(cls, text, ttl_seconds=None):
            inst = super().__new__(cls, text)
            inst.ttl_seconds = ttl_seconds
            return inst

    ns.update({"logging": logging, "TYPE_CHECKING": False,
               "asyncio": types.SimpleNamespace(to_thread=_to_thread),
               "concurrent": concurrent.futures,
               "dataclasses": __import__("dataclasses"),
               "json": json, "os": os, "re": re, "time": _time,
               "suppress": suppress, "Path": Path,
               "Any": Any, "Dict": dict, "List": list, "Optional": Optional,
               "Tuple": tuple, "Platform": SUP["Platform"],
               "MessageType": SUP["MessageType"],
               "EphemeralReply": EphemeralReply})
    ns["logger"] = _SilentLogger()
    ns["t"] = lambda key, **k: key
    ns["reply_pointer"] = lambda *a, **k: ""
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
    ns["TurnLeaseTimeoutError"] = type("TurnLeaseTimeoutError", (TimeoutError,), {})
    for name in ("note_busy_admitting", "note_busy_outcome", "settle_busy_arrival",
                 "stamp_busy_arrival"):
        ns[name] = lambda *a, **k: None
    src = open(RI_SRC, encoding="utf-8").read()
    seed_constants(src, ns, RI_SRC)
    for node in class_nodes(src, ["GatewayInboundMixin"]):
        exec_node(src, node, ns, RI_SRC)
    seed_constants(src, ns, RI_SRC)
    ns["logger"] = _SilentLogger()   # re-set AFTER the Assign seeding loop
    mixin = ns.get("GatewayInboundMixin")
    if mixin is None:
        print("FATAL oracle_run_inbound_flow3: GatewayInboundMixin missing")
        sys.exit(2)
    missing = [m for m in FUNCS if not hasattr(mixin, m)]
    if missing:
        print(f"FATAL oracle_run_inbound_flow3: mixin missing {missing}")
        sys.exit(2)
    return ns


FUNCS = [
    "_hm_offer_pairing_code", "_hm_report_ignored_dm", "_hm_write_update_response",
    "_hm_clarify_reply", "_hm_evict_idle_stale_agent", "_hm_stop_owner_turn",
    "_hm_handle_running_session_message", "_prefix_inbound_sender_context",
    "_classify_inbound_media", "_enrich_inbound_images", "_echo_stt_transcripts",
    "_inbound_attachment_display_name", "_prepend_inbound_media_file_notes",
    "_prepend_inbound_document_notes", "_prepend_inbound_reply_context",
    "_inbound_model_context_length", "_expand_inbound_context_references",
    "_prepare_clarify_reply_text",
]
FUNC_SET = set(FUNCS)

NS_RI = extract_ri()


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

    def __setattr__(self, k, v):
        object.__getattribute__(self, "_d")[k] = v

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


class StoreDouble:
    """Pairing/session store token wrapper; methods fire pairing_store.* /
    session_store.* seams."""
    def __init__(self, tok, ctx):
        self.tok = tok
        self._ctx = ctx

    def _is_rate_limited(self, platform, user_id):
        return self._ctx.call("pairing_store._is_rate_limited",
                              {"platform": platform, "user_id": user_id})

    def generate_code(self, platform, user_id, user_name):
        return self._ctx.call("pairing_store.generate_code",
                              {"platform": platform, "user_id": user_id,
                               "user_name": user_name})

    def _record_rate_limit(self, platform, user_id):
        return self._ctx.call("pairing_store._record_rate_limit",
                              {"platform": platform, "user_id": user_id})

    def peek_session_id(self, key):
        return self._ctx.call("session_store.peek_session_id", {"key": key})


class NotifierDouble:
    def __init__(self, tok, ctx):
        self.tok = tok
        self._ctx = ctx

    def first_time(self, platform, user_id):
        return self._ctx.call("notifier.first_time",
                              {"platform": platform, "user_id": user_id})

    async def notify(self, runner, source, hint):
        return self._ctx.call("notifier.notify",
                              {"source": norm_source(source), "hint": hint})


def make_adapter(raw, ctx):
    if raw is None:
        return None
    d = dict(raw)

    async def send(self, chat_id, text, metadata=None):
        r = self._ctx.call("adapter.send",
                           {"chat_id": chat_id, "text": text, "metadata": metadata})
        return types.SimpleNamespace(**r) if isinstance(r, dict) else r

    def resume_typing_for_chat(self, chat_id):
        return self._ctx.call("adapter.resume_typing_for_chat",
                              {"chat_id": chat_id})

    async def retire_clarify_card(self, clarify_id, text):
        return self._ctx.call("adapter.retire_clarify_card",
                              {"clarify_id": clarify_id, "text": text})

    ns = {"send": send, "resume_typing_for_chat": resume_typing_for_chat}
    if d.get("retire"):
        ns["retire_clarify_card"] = retire_clarify_card
    cls = type("AdapterDouble", (), ns)
    inst = cls()
    inst._raw = d
    inst._ctx = ctx
    return inst


def make_agent(raw, ctx):
    if raw is None:
        return None
    if raw == SENTINEL_TOKEN:
        return SENTINEL
    d = dict(raw)

    def get_activity_summary(self):
        return self._ctx.call("agent.get_activity_summary", {})

    ns = {}
    if d.get("activity"):
        ns["get_activity_summary"] = get_activity_summary
    cls = type("AgentDouble", (), ns)
    inst = cls()
    inst._raw = d
    inst._ctx = ctx
    return inst


class AwakeDouble:
    def __init__(self, raw, ctx):
        self._raw = raw
        self._ctx = ctx

    def measure(self, idle):
        return self._ctx.call("awake_idle.measure", {"idle": idle})


class OrderedCM:
    def __init__(self, tok, ctx):
        self.tok = tok
        self._ctx = ctx

    async def __aenter__(self):
        return self._ctx.call("ordered.__aenter__", {})

    async def __aexit__(self, *a):
        return self._ctx.call("ordered.__aexit__", {})


class ScopedCM:
    def __init__(self, ctx):
        self._ctx = ctx

    def __enter__(self):
        return self._ctx.call("scoped_runtime_main.__enter__", {})

    def __exit__(self, et, ev, tb):
        return self._ctx.call("scoped_runtime_main.__exit__",
                              {"exc": proj_exc(ev) if ev is not None else None})


class AuthorityDouble:
    """Authority record: data fields plus seamed control methods. _raw keeps
    the fixture projection (with the _ordered_writes TOKEN) for canon."""
    def __init__(self, raw, ctx):
        d = dict(raw)
        self._raw = d
        self._ctx = ctx
        self.id = d.get("id")
        self.profile_id = d.get("profile_id")
        self.sessions = d.get("sessions")
        ow = d.get("_ordered_writes")
        self._ordered_writes = OrderedCM(ow, ctx) if ow is not None else None
        self.logical_owner = rec(ctx, "authority.logical_owner",
                                 args_fn=lambda sid: {"sid": sid})
        self.fence_route = rec(ctx, "authority.fence_route",
                               args_fn=lambda key: {"key": key})
        self.cancel_queued = arec(
            ctx, "authority.cancel_queued",
            args_fn=lambda actor, ref, admission_id: {
                "actor": canon(actor), "ref": canon(ref), "admission_id": admission_id})
        self._handle = rec(ctx, "authority._handle",
                           args_fn=lambda ref: {"ref": canon(ref)}, build=handle_build)
        self.interrupt = arec(
            ctx, "authority.interrupt",
            args_fn=lambda actor, ref, generation: {
                "actor": canon(actor), "ref": canon(ref), "generation": generation})


class PersistenceStoreDouble:
    """authority_persistence handle: token + admissions.list seam."""
    def __init__(self, tok, ctx):
        self.tok = tok
        self._ctx = ctx
        self.admissions = types.SimpleNamespace(
            list=lambda session_id: rows_build(
                ctx.call("store.admissions.list", {"session_id": session_id})))


def make_authority(raw, ctx):
    if raw is None:
        return None
    return AuthorityDouble(raw, ctx)


# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
        if x is SENTINEL:
            return SENTINEL_TOKEN
        if is_dataclass(x) and not isinstance(x, type):
            return conv(asdict(x))
        if isinstance(x, enum.Enum):
            return x.value
        if isinstance(x, StateNS):
            return conv(object.__getattribute__(x, "_d"))
        if isinstance(x, EventDouble):
            return norm_event(x)
        if isinstance(x, (StoreDouble, NotifierDouble, OrderedCM,
                          PersistenceStoreDouble)):
            return x.tok
        if isinstance(x, AuthorityDouble):
            return conv(x._raw)
        if isinstance(x, AwakeDouble):
            return conv(x._raw)
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
        if isinstance(x, BaseException):
            return str(x)
        if hasattr(x, "_raw") and hasattr(x, "_ctx"):
            # per-case Adapter/Agent classes (duck probe)
            return conv(x._raw)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception materialization / projection ──
EXC_CLASSES = {
    "OSError": OSError, "TimeoutError": TimeoutError, "PermissionError": PermissionError,
    "FileNotFoundError": FileNotFoundError, "FileExistsError": FileExistsError,
    "BlockingIOError": BlockingIOError, "ConnectionError": ConnectionError,
    "ConnectionResetError": ConnectionResetError, "BrokenPipeError": BrokenPipeError,
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "IndexError": IndexError, "AttributeError": AttributeError,
    "NameError": NameError, "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "StopIteration": StopIteration, "AssertionError": AssertionError,
    "Exception": Exception, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
    "AuthorityRefusal": SUP["AuthorityRefusal"],
    "StoreUnavailable": SUP["StoreUnavailable"],
    "StoreReplaced": SUP["StoreReplaced"],
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


# ── per-case script backend (mirrors the C ri3_call exactly) ──
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


def ev_arg(event):
    return {"event": norm_event(event)}


def src_arg(source):
    return {"source": norm_source(source)}


def pending_build(raw):
    return None if raw is None else types.SimpleNamespace(**raw)


def rows_build(raw):
    if raw is None:
        return None
    return [types.SimpleNamespace(**r) if isinstance(r, dict) else r for r in raw]


def handle_build(raw):
    return None if raw is None else types.SimpleNamespace(**raw)


# ── per-case setup ──
def setup(args, state, ctx):
    mixin = NS_RI["GatewayInboundMixin"]
    SelfCls = type("RI3Self", (mixin,), {})
    self = object.__new__(SelfCls)

    # state-backed self attributes
    cfg = state.get("config")
    self.config = StateNS(cfg) if isinstance(cfg, dict) else cfg
    self._draining = state.get("_draining", False)
    ss = state.get("session_store")
    self.session_store = StoreDouble(ss, ctx) if ss is not None else None

    def _notifier_get(self):
        v = state.get("_unauthorized_owner_notifier")
        return None if v is None else NotifierDouble(v, ctx)

    def _notifier_set(self, v):
        state["_unauthorized_owner_notifier"] = v.tok if isinstance(v, NotifierDouble) else v

    SelfCls._unauthorized_owner_notifier = property(_notifier_get, _notifier_set)

    # materialize session-state agents / awake meters into attribute doubles
    for node in (state.get("session_states") or {}).values():
        if not isinstance(node, dict):
            continue
        turn = node.get("turn")
        if not isinstance(turn, dict):
            continue
        if "agent" in turn:
            turn["agent"] = make_agent(turn["agent"], ctx)
        tctx = turn.get("ctx")
        if isinstance(tctx, dict) and isinstance(tctx.get("awake_idle"), dict):
            tctx["awake_idle"] = AwakeDouble(tctx["awake_idle"], ctx)

    NS_RI["t"] = lambda key, **kw: ctx.call("t", {"key": key, **kw})
    NS_RI["logger"] = _SilentLogger()
    NS_RI["time"] = types.SimpleNamespace(time=rec(ctx, "time.time", args_fn=lambda: {}))

    # Path filesystem ops are seams (live write_text/replace/unlink would
    # otherwise hit the real disk; as_posix keeps the POSIX fixture paths).
    def _path_write_text(self, data, encoding=None, errors=None, newline=None):
        return ctx.call("path.write_text", {"path": self.as_posix(), "text": data})

    def _path_replace(self, target):
        dst = target.as_posix() if isinstance(target, Path) else str(target)
        return ctx.call("path.replace", {"src": self.as_posix(), "dst": dst})

    def _path_unlink(self, missing_ok=False):
        return ctx.call("path.unlink", {"path": self.as_posix(), "missing_ok": missing_ok})

    Path.write_text = _path_write_text
    Path.replace = _path_replace
    Path.unlink = _path_unlink
    NS_RI["label_shared_sender"] = rec(
        ctx, "label_shared_sender",
        args_fn=lambda source, text, group_sessions_per_user=True,
        thread_sessions_per_user=False: {
            "source": norm_source(source), "text": text,
            "group_sessions_per_user": group_sessions_per_user,
            "thread_sessions_per_user": thread_sessions_per_user})
    NS_RI["clean_attachment_filename"] = rec(
        ctx, "clean_attachment_filename", args_fn=lambda name: {"name": name})
    NS_RI["reply_pointer"] = rec(
        ctx, "reply_pointer",
        args_fn=lambda text, own_message=False, replied_to=None: {
            "text": str(text), "own_message": own_message,
            "replied_to": canon(replied_to)})
    NS_RI["discord_triggering_note"] = rec(
        ctx, "discord_triggering_note", args_fn=lambda message_id: {"message_id": message_id})
    NS_RI["pairing_code_reply"] = rec(
        ctx, "pairing_code_reply",
        args_fn=lambda platform, code, profile: {
            "platform": platform, "code": code, "profile": profile})
    NS_RI["pairing_profile_arg"] = rec(
        ctx, "pairing_profile_arg", args_fn=lambda store: {"store": canon(store)})
    NS_RI["pairing_rate_limited_reply"] = rec(ctx, "pairing_rate_limited_reply",
                                              args_fn=lambda: {})
    NS_RI["unauthorized_owner_hint"] = rec(
        ctx, "unauthorized_owner_hint",
        args_fn=lambda platform, user_id, user_name, hermes_home=None: {
            "platform": platform, "user_id": user_id, "user_name": user_name,
            "hermes_home": hermes_home})
    NS_RI["UnauthorizedOwnerNotifier"] = rec(
        ctx, "UnauthorizedOwnerNotifier", args_fn=lambda: {},
        build=lambda raw: None if raw is None else NotifierDouble(raw, ctx))
    NS_RI["cancel_preagent_setup"] = rec(
        ctx, "cancel_preagent_setup", args_fn=lambda runner, key: {"key": key})
    NS_RI["pending_setup_busy_notice"] = rec(
        ctx, "pending_setup_busy_notice",
        args_fn=lambda runner, key, source: {"key": key, "source": norm_source(source)})

    def session_state(key):
        states = state.setdefault("session_states", {})
        node = states.get(key)
        if node is None:
            node = {}
            states[key] = node
        ctx.trace.append({"name": "self._session_state", "args": {"key": key},
                          "ret": canon(node)})
        return StateNS(node)

    def peek_session_state(key):
        states = state.get("session_states")
        node = states.get(key) if isinstance(states, dict) else None
        if node is not None and not isinstance(node, dict):
            node = None
        ctx.trace.append({"name": "self._peek_session_state", "args": {"key": key},
                          "ret": canon(node) if node is not None else None})
        return StateNS(node) if node is not None else None

    self._session_state = session_state
    self._peek_session_state = peek_session_state

    # claimed siblings + unported collaborators: instance recorders shadow
    # the live class methods (the 18 ported methods stay live and compose).
    self._pairing_store_for = rec(ctx, "self._pairing_store_for", args_fn=src_arg,
                                  build=lambda raw: None if raw is None else StoreDouble(raw, ctx))
    self._delivery_adapter_for = rec(ctx, "self._delivery_adapter_for", args_fn=src_arg,
                                     build=lambda raw: make_adapter(raw, ctx))
    self._refused_clarify = rec(ctx, "self._refused_clarify",
                                args_fn=lambda event, clarify_id: {
                                    "event": norm_event(event), "clarify_id": clarify_id})
    self._pending_event_audio_paths = rec(ctx, "self._pending_event_audio_paths",
                                          args_fn=ev_arg)
    self._current_session_run_generation = rec(
        ctx, "self._current_session_run_generation", args_fn=lambda key: {"key": key})
    self._release_pending_clarify = arec(
        ctx, "self._release_pending_clarify",
        args_fn=lambda pending, source: {"pending": canon(pending),
                                         "source": norm_source(source)})
    self._hm_evict_running_agent = rec(
        ctx, "self._hm_evict_running_agent",
        args_fn=lambda key, reason: {"key": key, "reason": reason})
    self._queue_or_replace_pending_event = rec(
        ctx, "self._queue_or_replace_pending_event",
        args_fn=lambda key, event: {"key": key, "event": norm_event(event)})
    self._hm_busy_slash_or_photo = arec(
        ctx, "self._hm_busy_slash_or_photo",
        args_fn=lambda event, source, key: {
            "event": norm_event(event), "source": norm_source(source), "key": key})
    self._busy_skill_turn = arec(
        ctx, "self._busy_skill_turn",
        args_fn=lambda event, source, key: {
            "event": norm_event(event), "source": norm_source(source), "key": key})
    self._clarify_takeover = rec(ctx, "self._clarify_takeover",
                                 args_fn=lambda event, key: {
                                     "event": norm_event(event), "key": key})
    self._effective_busy_input_mode = rec(ctx, "self._effective_busy_input_mode",
                                          args_fn=src_arg)
    self._hm_busy_telegram_grace_queue = rec(
        ctx, "self._hm_busy_telegram_grace_queue",
        args_fn=lambda event, source, key, mode: {
            "event": norm_event(event), "source": norm_source(source),
            "key": key, "mode": mode})
    self._release_running_agent_state = rec(
        ctx, "self._release_running_agent_state", args_fn=lambda key: {"key": key})
    self._hm_merge_pending_for_source = rec(
        ctx, "self._hm_merge_pending_for_source",
        args_fn=lambda source, key, event, merge_text=False: {
            "source": norm_source(source), "key": key,
            "event": norm_event(event), "merge_text": merge_text})
    self._queue_during_drain_enabled = rec(
        ctx, "self._queue_during_drain_enabled", args_fn=lambda mode: {"mode": mode})
    self._status_action_gerund = rec(ctx, "self._status_action_gerund", args_fn=lambda: {})
    self._hm_busy_steer = rec(ctx, "self._hm_busy_steer",
                              args_fn=lambda event, agent, key: {
                                  "event": norm_event(event), "agent": canon(agent),
                                  "key": key})
    self._agent_has_active_subagents = rec(
        ctx, "self._agent_has_active_subagents", args_fn=lambda agent: {"agent": canon(agent)})
    self._session_has_compression_in_flight = arec(
        ctx, "self._session_has_compression_in_flight", args_fn=lambda key: {"key": key})
    self._hm_busy_interrupt = arec(
        ctx, "self._hm_busy_interrupt",
        args_fn=lambda event, source, agent, key: {
            "event": norm_event(event), "source": norm_source(source),
            "agent": canon(agent), "key": key})
    self._resolve_session_agent_runtime = rec(
        ctx, "self._resolve_session_agent_runtime",
        args_fn=lambda source, session_key, user_config=None: {
            "source": norm_source(source), "session_key": session_key,
            "user_config": canon(user_config)})
    self._transcribe_pending_audio_event_once = arec(
        ctx, "self._transcribe_pending_audio_event_once",
        args_fn=lambda event, arg: {"event": norm_event(event), "arg": arg})
    self._decide_image_input_mode = rec(
        ctx, "self._decide_image_input_mode",
        args_fn=lambda source, session_key: {
            "source": norm_source(source), "session_key": session_key})
    self._enrich_message_with_vision = arec(
        ctx, "self._enrich_message_with_vision",
        args_fn=lambda text, image_paths: {"text": text, "image_paths": image_paths})

    stub_modules({
        "gateway.run": {
            "_AGENT_PENDING_SENTINEL": SENTINEL,
            "_float_env": rec(ctx, "_float_env",
                              args_fn=lambda name, default: {"name": name, "default": default}),
            "_hermes_home": Path(state.get("hermes_home", "")),
            "_load_gateway_config": rec(ctx, "_load_gateway_config", args_fn=lambda: {}),
            "_event_media_is_image": rec(ctx, "event_media_is_image",
                                         args_fn=lambda event, i: {"event": norm_event(event), "i": i}),
            "_event_media_is_audio": rec(ctx, "event_media_is_audio",
                                         args_fn=lambda event, i: {"event": norm_event(event), "i": i}),
            "_event_media_is_video": rec(ctx, "event_media_is_video",
                                         args_fn=lambda event, i: {"event": norm_event(event), "i": i}),
            "_event_media_is_stt_input": rec(ctx, "event_media_is_stt_input",
                                             args_fn=lambda event, i: {"event": norm_event(event), "i": i}),
            "_build_document_context_note": rec(
                ctx, "build_document_context_note",
                args_fn=lambda display_name, agent_path, mtype, content_inlined=False: {
                    "display_name": display_name, "agent_path": agent_path,
                    "mtype": mtype, "content_inlined": content_inlined}),
        },
        "hermes_constants": {
            "display_hermes_home": rec(ctx, "display_hermes_home", args_fn=lambda: {}),
        },
        "tools.clarify_gateway": {
            "get_pending_for_session": rec(
                ctx, "clarify.get_pending_for_session",
                args_fn=lambda key, include_choice_prompts=False: {
                    "key": key, "include_choice_prompts": include_choice_prompts},
                build=pending_build),
            "attempt_text_response": rec(
                ctx, "clarify.attempt_text_response",
                args_fn=lambda clarify_id, text: {"clarify_id": clarify_id, "text": text}),
            "TEXT_RESOLVED": "resolved",
            "TEXT_REJECTED_PROSE": "rejected_prose",
            "TEXT_REJECTED_SELECTION": "rejected_selection",
            "SKIPPED": SKIPPED,
        },
        "gateway.session_stall": {
            "resolve_session_idle_seconds_from_activity": rec(
                ctx, "resolve_session_idle_seconds_from_activity",
                args_fn=lambda activity, now: {"activity": canon(activity), "now": now}),
        },
        "gateway.session_authorities": {
            "active_authority": rec(ctx, "active_authority", args_fn=lambda runner: {},
                                    build=lambda raw: make_authority(raw, ctx)),
        },
        "gateway.session_contract": {
            "SessionRef": SUP["SessionRef"], "SessionActor": SUP["SessionActor"],
        },
        "gateway.session_persistence": {
            "authority_persistence": rec(
                ctx, "authority_persistence",
                args_fn=lambda authority: {"authority": canon(authority)},
                build=lambda raw: None if raw is None else PersistenceStoreDouble(raw, ctx)),
        },
        "persistence.core.errors": {
            "AuthorityRefusal": SUP["AuthorityRefusal"],
            "StoreUnavailable": SUP["StoreUnavailable"],
        },
        "gateway.bot_peer_notices": {
            "automatic_notices_allowed": rec(ctx, "automatic_notices_allowed",
                                             args_fn=src_arg),
        },
        "tools.credential_files": {
            "to_agent_visible_cache_path": rec(ctx, "to_agent_visible_cache_path",
                                               args_fn=lambda path: {"path": path}),
        },
        "mimetypes": {
            "guess_type": rec(ctx, "mimetypes.guess_type", args_fn=lambda path: {"path": path}),
        },
        "agent.auxiliary_client": {
            "scoped_runtime_main": rec(ctx, "scoped_runtime_main",
                                       args_fn=lambda runtime: {"runtime": canon(runtime)},
                                       build=lambda raw: ScopedCM(ctx)),
        },
        "gateway.session": {
            "_discord_tools_loaded": rec(ctx, "_discord_tools_loaded", args_fn=lambda: {}),
        },
        "gateway.session_context_refs": {
            "inbound_references": arec(
                ctx, "inbound_references",
                args_fn=lambda runner, source, session_key, text: {
                    "source": norm_source(source), "session_key": session_key, "text": text}),
        },
        "agent.model_metadata": {
            "get_model_context_length_async": arec(
                ctx, "get_model_context_length_async",
                args_fn=lambda model, base_url="", api_key="", config_context_length=None,
                provider="", custom_providers=None: {
                    "model": model, "base_url": base_url, "api_key": api_key,
                    "config_context_length": config_context_length,
                    "provider": provider, "custom_providers": canon(custom_providers)}),
        },
        "hermes_cli.config": {
            "get_compatible_custom_providers": rec(
                ctx, "get_compatible_custom_providers",
                args_fn=lambda cfg: {"cfg": canon(cfg)}),
            "get_custom_provider_context_length": rec(
                ctx, "get_custom_provider_context_length",
                args_fn=lambda model, base_url, custom_providers: {
                    "model": model, "base_url": base_url,
                    "custom_providers": canon(custom_providers)}),
        },
        "hermes_cli.route_identity": {
            "should_clear_context_pin_async": arec(
                ctx, "should_clear_context_pin_async",
                args_fn=lambda model, configured_model, config_base_url, base_url,
                config_provider, provider: {
                    "model": model, "configured_model": configured_model,
                    "config_base_url": config_base_url, "base_url": base_url,
                    "config_provider": config_provider, "provider": provider}),
        },
    })
    ev = EventDouble(args.get("event") or {}, ctx) if args.get("event") is not None else None
    return self, ev


# ── per-func invocation ──
def invoke(func, args, self, ctx, ev):
    src = make_source(args.get("source"))
    key = args.get("key", "")
    skey = args.get("session_key", "")
    text = args.get("message_text", "")
    run = _asyncio.run
    if func == "_hm_offer_pairing_code":
        return run(self._hm_offer_pairing_code(src))
    if func == "_hm_report_ignored_dm":
        return run(self._hm_report_ignored_dm(src))
    if func == "_hm_write_update_response":
        return self._hm_write_update_response(args.get("response_text", ""))
    if func == "_hm_clarify_reply":
        return run(self._hm_clarify_reply(ev, src, key))
    if func == "_hm_evict_idle_stale_agent":
        return self._hm_evict_idle_stale_agent(key)
    if func == "_hm_stop_owner_turn":
        return run(self._hm_stop_owner_turn(key))
    if func == "_hm_handle_running_session_message":
        return run(self._hm_handle_running_session_message(ev, src, key))
    if func == "_prefix_inbound_sender_context":
        return self._prefix_inbound_sender_context(ev, src, text)
    if func == "_classify_inbound_media":
        return self._classify_inbound_media(ev, args.get("pending_stt_prepared", False))
    if func == "_enrich_inbound_images":
        return run(self._enrich_inbound_images(src, skey, text, args.get("image_paths", [])))
    if func == "_echo_stt_transcripts":
        adapter = make_adapter(args.get("adapter"), ctx)
        return run(self._echo_stt_transcripts(
            adapter, src, args.get("transcripts", []),
            metadata=args.get("metadata"), log_context=args.get("log_context", "Transcript")))
    if func == "_inbound_attachment_display_name":
        return self._inbound_attachment_display_name(args.get("path", ""),
                                                     args.get("original_name", ""))
    if func == "_prepend_inbound_media_file_notes":
        return self._prepend_inbound_media_file_notes(
            text, args.get("audio_file_paths", []), args.get("video_paths", []),
            args.get("original_names"))
    if func == "_prepend_inbound_document_notes":
        return self._prepend_inbound_document_notes(ev, text)
    if func == "_prepend_inbound_reply_context":
        return self._prepend_inbound_reply_context(ev, src, text)
    if func == "_inbound_model_context_length":
        return run(self._inbound_model_context_length(src, skey))
    if func == "_expand_inbound_context_references":
        return run(self._expand_inbound_context_references(src, skey, text))
    if func == "_prepare_clarify_reply_text":
        return run(self._prepare_clarify_reply_text(ev))
    raise RuntimeError(f"unknown func {func}")


# ── per-case drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    state = canon(inp.get("state") or {})
    ctx = Ctx(inp.get("io"))
    self, ev = setup(args, state, ctx)
    try:
        ret = invoke(func, args, self, ctx, ev)
    except BaseException as exc:   # SystemExit included
        ret = proj_exc(exc)
    event_out = norm_event(ev) if ev is not None else None
    return {"ret": canon(ret), "trace": canon(ctx.trace),
            "state": canon(state), "event": canon(event_out)}


# ── three-way seam-name cross-check (C port ↔ oracle stubs ↔ fixture io) ──
SEAM_NAMES = [
    "event.get_command",
    "self._pairing_store_for", "pairing_store._is_rate_limited",
    "pairing_store.generate_code", "pairing_store._record_rate_limit",
    "self._delivery_adapter_for", "pairing_profile_arg", "pairing_code_reply",
    "pairing_rate_limited_reply", "adapter.send",
    "display_hermes_home", "unauthorized_owner_hint", "UnauthorizedOwnerNotifier",
    "notifier.first_time", "notifier.notify",
    "path.write_text", "path.replace", "path.unlink",
    "clarify.get_pending_for_session", "self._refused_clarify",
    "self._pending_event_audio_paths", "clarify.attempt_text_response",
    "adapter.resume_typing_for_chat", "t", "adapter.retire_clarify_card",
    "self._current_session_run_generation", "self._release_pending_clarify",
    "self._transcribe_pending_audio_event_once",
    "_float_env", "self._peek_session_state", "time.time",
    "agent.get_activity_summary", "resolve_session_idle_seconds_from_activity",
    "awake_idle.measure", "self._hm_evict_running_agent",
    "active_authority", "session_store.peek_session_id", "authority.logical_owner",
    "authority.fence_route", "authority_persistence", "store.admissions.list",
    "ordered.__aenter__", "ordered.__aexit__", "authority.cancel_queued",
    "authority._handle", "authority.interrupt",
    "self._queue_or_replace_pending_event", "self._hm_busy_slash_or_photo",
    "self._busy_skill_turn", "self._clarify_takeover",
    "self._effective_busy_input_mode", "self._hm_busy_telegram_grace_queue",
    "cancel_preagent_setup", "self._release_running_agent_state",
    "self._hm_merge_pending_for_source", "pending_setup_busy_notice",
    "self._queue_during_drain_enabled", "self._status_action_gerund",
    "self._hm_busy_steer", "self._agent_has_active_subagents",
    "self._session_has_compression_in_flight", "self._hm_busy_interrupt",
    "label_shared_sender", "event_media_is_image", "event_media_is_audio",
    "event_media_is_stt_input", "event_media_is_video",
    "self._decide_image_input_mode", "self._session_state",
    "self._resolve_session_agent_runtime", "scoped_runtime_main",
    "scoped_runtime_main.__enter__", "self._enrich_message_with_vision",
    "scoped_runtime_main.__exit__", "automatic_notices_allowed",
    "clean_attachment_filename", "to_agent_visible_cache_path",
    "mimetypes.guess_type", "build_document_context_note",
    "reply_pointer", "_discord_tools_loaded", "discord_triggering_note",
    "_load_gateway_config", "get_compatible_custom_providers",
    "should_clear_context_pin_async", "get_custom_provider_context_length",
    "get_model_context_length_async", "inbound_references",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "run_inbound_flow3.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_run_inbound_flow3: seams absent from C port: {missing_c}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'(?:rec|arec)\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'ctx\.call\(\s*"([^"]+)"', osrc))
    used |= {"self._session_state", "self._peek_session_state"}
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_run_inbound_flow3: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "run_inbound_flow3.jsonl"
    if fix.exists():
        fix_keys = set()
        fix_funcs = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func"))
        bad = fix_keys - registry
        if bad:
            print(f"FATAL oracle_run_inbound_flow3: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        badf = fix_funcs - FUNC_SET
        if badf:
            print(f"FATAL oracle_run_inbound_flow3: fixture funcs outside port set: {sorted(badf)}")
            sys.exit(2)
        missingf = FUNC_SET - fix_funcs
        if missingf:
            print(f"FATAL oracle_run_inbound_flow3: ported funcs without fixtures: {sorted(missingf)}")
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
    for key in ("state", "event"):
        ws, gs = want.get(key), got.get(key)
        if ws != gs:
            if isinstance(ws, dict) and isinstance(gs, dict):
                keys = [k for k in set(ws) | set(gs) if ws.get(k) != gs.get(k)]
                print(f"  {key} diff keys: {sorted(keys)[:10]}")
                for k in sorted(keys)[:3]:
                    print(f"    {k} want={json.dumps(ws.get(k), ensure_ascii=False)[:200]}")
                    print(f"    {k} got ={json.dumps(gs.get(k), ensure_ascii=False)[:200]}")
            else:
                print(f"  {key} want={json.dumps(ws, ensure_ascii=False)[:300]}")
                print(f"  {key} got ={json.dumps(gs, ensure_ascii=False)[:300]}")


def main():
    cross_check()
    n = m = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec_ = json.loads(line)
        except Exception as exc:
            m += 1
            print(f"MISMATCH <unparsable harness line>: {exc}")
            continue
        n += 1
        func = rec_.get("func", "")
        inp = rec_.get("in") or {}
        got = rec_.get("out")
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
    print(f"ORACLE run_inbound_flow3: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


if __name__ == "__main__":
    main()