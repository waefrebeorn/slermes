#!/usr/bin/env python3
"""Oracle for the UNCLAIMED command-dispatch / admission FLOW remainder of
gateway/run_inbound.py (src/cli/gateway_inbound_flow2.c, second wave after
gateway_inbound_flow / run_inbound_flow claimed the ingress/busy/authority
cores).

Drives the LIVE GatewayInboundMixin methods with SCRIPTED seams: every
command resolution, quick/plugin handler, skill scan, prompt builder,
authority admission, busy metric, adapter lookup, i18n call and clock is a
recording stub fed the same scripts the C harness feeds its universal seam.
Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"} in
"trace") plus the final return ("ret"), the mutated world state ("state")
and the possibly text-mutated INPUT event ("event").

The live defs are AST-extracted from gateway/run_inbound.py (the whole
GatewayInboundMixin ClassDef execs with stub mixin bases; module constants
seeded, Assign AND AnnAssign, plain-Name targets only). Support types are
the REAL ones, AST-extracted from their modules: gateway.config.Platform,
gateway.platforms.event.MessageType, persistence.core.errors
(AuthorityRefusal family + _one_line), gateway.turn_lease.TurnLeaseTimeoutError.
Every exec namespace is a registered sys.modules module (py3.14 dataclasses
_is_type needs cls.__module__ resolvable). The heavy quarry import chain is
never imported.

Claimed siblings (flow1 ports) and unported collaborators are SEAMED as
"self.<name>" instance recorders; the 21 ported methods stay LIVE on the
class and compose through normal attribute lookup.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE gateway_inbound_flow2: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc), ...}
  (AuthorityRefusal adds reason/field/detail; OSError adds errno when set).
- Platform/MessageType members travel as .value strings; events/sources as
  attribute dicts (EventDouble / SimpleNamespace materialization).
- Authorities/adapters travel as id strings; logger calls silent; the
  execute closure projects to {"__callable__": "execute"} (never invoked).
- executing_admission.get() reads state.executing_admission silently.
- asyncio.to_thread / _run_in_executor_with_context run their callable
  synchronously (no trace of their own); a `build` callable travels as
  {"fn": seam, "args": {...}} and fires that one seam.
- self._session_state is a get-or-create STATE accessor (traced, borrowed
  view); _AGENT_PENDING_SENTINEL travels as "__agent_pending_sentinel__".
- Fixture corpus is ASCII; integral numbers only; documented domain in the
  port header (resolve_command null-or-{name}, str builder results, etc.).
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
LEASE_SRC = os.path.join(Q, "gateway", "turn_lease.py")

_FLAGS = _fut.annotations.compiler_flag
SENTINEL_TOKEN = "__agent_pending_sentinel__"
SENTINEL = object()   # _AGENT_PENDING_SENTINEL identity marker


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


# ── support namespace: real enums + error families ──
def extract_support():
    mod = new_module("gi2_oracle_support")
    ns = mod.__dict__
    ns.update({"enum": enum, "Enum": enum.Enum, "StrEnum": enum.StrEnum,
               "contextlib": contextlib, "Any": Any,
               "Optional": Optional, "dataclass": __import__("dataclasses").dataclass,
               "field": __import__("dataclasses").field})
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
    lsrc = open(LEASE_SRC, encoding="utf-8").read()
    for node in class_nodes(lsrc, ["TurnLeaseTimeoutError"]):
        exec_node(lsrc, node, ns, LEASE_SRC)
    required = ["Platform", "MessageType", "AuthorityRefusal", "StoreUnavailable",
                "TurnLeaseTimeoutError", "_one_line"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_gateway_inbound_flow2: support missing: {missing}")
        sys.exit(2)
    return ns


SUP = extract_support()


# ── run_inbound namespace ──
def extract_ri():
    mod = new_module("gi2_oracle_ri")
    ns = mod.__dict__
    import concurrent.futures
    import logging
    import time as _time

    async def _to_thread(fn, *a, **k):
        return fn(*a, **k)

    ns.update({"logging": logging, "TYPE_CHECKING": False,
               "asyncio": types.SimpleNamespace(to_thread=_to_thread),
               "concurrent": concurrent.futures,
               "dataclasses": __import__("dataclasses"),
               "json": json, "os": os, "re": re, "time": _time,
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
    ns["TurnLeaseTimeoutError"] = SUP["TurnLeaseTimeoutError"]
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
        print("FATAL oracle_gateway_inbound_flow2: GatewayInboundMixin missing")
        sys.exit(2)
    missing = [m for m in FUNCS if not hasattr(mixin, m)]
    if missing:
        print(f"FATAL oracle_gateway_inbound_flow2: mixin missing {missing}")
        sys.exit(2)
    return ns


FUNCS = [
    "_hm_resolve_command", "_hm_confirm_destructive", "_hm_cmd_new",
    "_hm_cmd_start", "_hm_cmd_egress", "_hm_rewrite_turn_to_prompt",
    "_hm_cmd_learn", "_hm_cmd_plan", "_hm_cmd_init", "_hm_cmd_blueprint",
    "_hm_cmd_undo", "_hm_cmd_queue", "_hm_cmd_steer",
    "_hm_dispatch_canonical_command", "_hm_skill_slash_rewrite",
    "_hm_pending_reply_intercepts", "_hm_dispatch_idle_commands",
    "_hm_rescue_orphaned_fifo", "_hm_authority_admission",
    "_hm_admit_to_fifo", "_handle_message",
]
FUNC_SET = set(FUNCS)

NS_RI = extract_ri()


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
    "TurnLeaseTimeoutError": SUP["TurnLeaseTimeoutError"],
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    if kind == "AuthorityRefusal":
        return SUP["AuthorityRefusal"](spec.get("reason", ""),
                                       field=spec.get("field"), detail=spec.get("detail"))
    if kind == "TurnLeaseTimeoutError":
        return SUP["TurnLeaseTimeoutError"](
            spec.get("session_id", ""), owner_key=spec.get("owner_key", ""),
            generation=spec.get("generation", 0), wait_seconds=spec.get("wait_seconds", 0))
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


# ── per-case script backend (mirrors the C gi2_call exactly) ──
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


class NoopAsyncCM:
    async def __aenter__(self):
        return self

    async def __aexit__(self, *a):
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

    def __setattr__(self, k, v):
        object.__getattribute__(self, "_d")[k] = v

    def get_command(self):
        return object.__getattribute__(self, "_ctx").call("event.get_command", {})

    def get_command_args(self):
        return object.__getattribute__(self, "_ctx").call("event.get_command_args", {})


def norm_event(ev):
    d = dict(object.__getattribute__(ev, "_d"))
    if "source" in d:
        d["source"] = norm_source(d["source"])
    mt = d.get("message_type")
    if isinstance(mt, enum.Enum):
        d["message_type"] = mt.value
    return d


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


def make_handler(name):
    async def h(event):
        return None
    if isinstance(name, str) and name:
        async def h(event):   # noqa: F811
            return CTX_HOLDER["ctx"].call(name, {"event": norm_event(event)})
    return h


CTX_HOLDER = {"ctx": None}


# ── per-case setup ──
def setup(args, state, ctx):
    self = object.__new__(NS_RI["GatewayInboundMixin"])
    CTX_HOLDER["ctx"] = ctx
    self._external_drain_active = state.get("_external_drain_active", False)
    self.session_authority = state.get("session_authority")

    NS_RI["t"] = lambda key, **kw: ctx.call("t", {"key": key, **kw})
    NS_RI["logger"] = _SilentLogger()
    NS_RI["time"] = types.SimpleNamespace(time=rec(ctx, "time.time", args_fn=lambda: {}))
    for name in ("note_busy_admitting", "settle_busy_arrival", "stamp_busy_arrival",
                 "pending_setup_busy_notice"):
        if name == "stamp_busy_arrival":
            NS_RI[name] = rec(ctx, name, args_fn=lambda adapter, event: {
                "adapter": adapter, "event": norm_event(event)})
        elif name == "pending_setup_busy_notice":
            NS_RI[name] = rec(ctx, name, args_fn=lambda runner, key, source: {
                "key": key, "source": norm_source(source)})
        else:
            NS_RI[name] = rec(ctx, name, args_fn=lambda event: {"event": norm_event(event)})

    def session_state(key):
        states = state.setdefault("session_states", {})
        node = states.get(key)
        if node is None:
            node = {}
            states[key] = node
        ctx.trace.append({"name": "self._session_state", "args": {"key": key},
                          "ret": canon(node)})
        return StateNS(node)

    self._session_state = session_state

    def admit_build(raw):
        if raw is None:
            return None
        return [EventDouble(raw[0], ctx), make_source(raw[1]), raw[2]]

    def blueprint_build(raw):
        return None if raw is None else types.SimpleNamespace(**raw)

    def rescued_build(raw):
        return None if raw is None else EventDouble(raw, ctx)

    def handlers_build(raw):
        if not isinstance(raw, dict):
            return raw
        return {k: make_handler(v) for k, v in raw.items()}

    def confirm_stub(event=None, command=None, title=None, detail=None, execute=None):
        return ctx.call("self._maybe_confirm_destructive_slash", {
            "event": norm_event(event), "command": command, "title": title,
            "detail": detail, "execute": {"__callable__": "execute"}})

    async def confirm_stub_a(**kw):
        return confirm_stub(**kw)

    async def exec_ctx(func, *a):
        return func(*a)

    self._run_in_executor_with_context = exec_ctx

    # claimed siblings + unported collaborators: instance recorders shadow
    # the live class methods (the 21 ported methods stay live and compose).
    self._hm_admit_event = arec(ctx, "self._hm_admit_event", args_fn=ev_arg,
                                build=admit_build)
    self._hm_estop_gate = rec(ctx, "self._hm_estop_gate",
                              args_fn=lambda event, source, is_internal: {
                                  "event": norm_event(event), "source": norm_source(source),
                                  "is_internal": is_internal})
    self._session_key_for_source = rec(ctx, "self._session_key_for_source", args_fn=src_arg)
    self._hm_update_prompt_reply = rec(ctx, "self._hm_update_prompt_reply",
                                       args_fn=lambda event, key: {
                                           "event": norm_event(event), "key": key})
    self._hm_clarify_reply = arec(ctx, "self._hm_clarify_reply",
                                  args_fn=lambda event, source, key: {
                                      "event": norm_event(event), "source": norm_source(source),
                                      "key": key})
    self._hm_slash_confirm_reply = arec(ctx, "self._hm_slash_confirm_reply",
                                        args_fn=lambda event, key: {
                                            "event": norm_event(event), "key": key})
    self._hm_evict_idle_stale_agent = rec(ctx, "self._hm_evict_idle_stale_agent",
                                          args_fn=lambda key: {"key": key})
    self._is_session_running = rec(ctx, "self._is_session_running",
                                   args_fn=lambda key: {"key": key})
    self._hm_evict_reaped_agent = rec(ctx, "self._hm_evict_reaped_agent",
                                      args_fn=lambda key: {"key": key})
    self._hm_handle_running_session_message = arec(
        ctx, "self._hm_handle_running_session_message",
        args_fn=lambda event, source, key: {
            "event": norm_event(event), "source": norm_source(source), "key": key})
    self._hm_dispatch_quick_and_plugin_commands = arec(
        ctx, "self._hm_dispatch_quick_and_plugin_commands",
        args_fn=lambda event, source, command: {
            "event": norm_event(event), "source": norm_source(source),
            "command": command})
    self._admit_command_turn = arec(ctx, "self._admit_command_turn",
                                    args_fn=lambda event, source, key, stop_epoch: {
                                        "event": norm_event(event), "source": norm_source(source),
                                        "key": key, "stop_epoch": stop_epoch})
    self._route_stop_epoch = rec(ctx, "self._route_stop_epoch",
                                 args_fn=lambda key: {"key": key})
    self._claim_active_session_slot = rec(ctx, "self._claim_active_session_slot",
                                          args_fn=lambda key, source: {
                                              "key": key, "source": norm_source(source)})
    self._is_telegram_topic_root_lobby = rec(ctx, "self._is_telegram_topic_root_lobby",
                                             args_fn=src_arg)
    self._should_send_telegram_lobby_reminder = rec(
        ctx, "self._should_send_telegram_lobby_reminder", args_fn=src_arg)
    self._telegram_topic_root_lobby_message = rec(
        ctx, "self._telegram_topic_root_lobby_message", args_fn=lambda: {})
    self._telegram_topic_root_new_message = rec(
        ctx, "self._telegram_topic_root_new_message", args_fn=lambda: {})
    self._persist_active_agents = rec(ctx, "self._persist_active_agents", args_fn=lambda: {})
    self._begin_session_run_generation = rec(ctx, "self._begin_session_run_generation",
                                             args_fn=lambda key: {"key": key})
    self._hm_post_admission_consume = arec(ctx, "self._hm_post_admission_consume",
                                           args_fn=lambda event, source, key: {
                                               "event": norm_event(event),
                                               "source": norm_source(source), "key": key})
    self._handle_message_with_agent = arec(ctx, "self._handle_message_with_agent",
                                           args_fn=lambda event, source, key, run_generation: {
                                               "event": norm_event(event),
                                               "source": norm_source(source),
                                               "key": key, "run_generation": run_generation})
    self._run_post_turn_hooks = arec(ctx, "self._run_post_turn_hooks",
                                     args_fn=lambda agent_result, source, is_internal, event: {
                                         "agent_result": agent_result,
                                         "source": norm_source(source),
                                         "is_internal": is_internal,
                                         "event": norm_event(event)})
    self._restore_pending_one_turn_model_override = rec(
        ctx, "self._restore_pending_one_turn_model_override",
        args_fn=lambda key, run_generation=None: {"key": key, "run_generation": run_generation})
    self._clear_durable_active_turn = arec(ctx, "self._clear_durable_active_turn",
                                           args_fn=ev_arg)
    self._release_running_agent_state = rec(ctx, "self._release_running_agent_state",
                                            args_fn=lambda key, run_generation=None: {
                                                "key": key, "run_generation": run_generation})
    self._release_turn_lease = rec(ctx, "self._release_turn_lease",
                                   args_fn=lambda key, run_generation=None: {
                                       "key": key, "run_generation": run_generation})
    self._hm_quick_commands = rec(ctx, "self._hm_quick_commands", args_fn=lambda: {})
    self._hm_expand_alias_quick_command = rec(ctx, "self._hm_expand_alias_quick_command",
                                              args_fn=lambda event, qcmd: {
                                                  "event": norm_event(event), "qcmd": qcmd})
    self._check_slash_access = rec(ctx, "self._check_slash_access",
                                   args_fn=lambda source, command: {
                                       "source": norm_source(source), "command": command})
    self._hm_command_hooks = arec(ctx, "self._hm_command_hooks",
                                  args_fn=lambda event, source, key, command, canonical: {
                                      "event": norm_event(event), "source": norm_source(source),
                                      "key": key, "command": command, "canonical": canonical})
    self._hm_drop_held_input = rec(ctx, "self._hm_drop_held_input", args_fn=ev_arg)
    self._hm_bundle_slash_rewrite = rec(ctx, "self._hm_bundle_slash_rewrite",
                                        args_fn=lambda event, source, key, command: {
                                            "event": norm_event(event), "source": norm_source(source),
                                            "key": key, "command": command})
    self._hm_unknown_slash_reply = rec(ctx, "self._hm_unknown_slash_reply",
                                       args_fn=lambda command, source: {
                                           "command": command, "source": norm_source(source)})
    self._hm_send_payload_as_turn = rec(ctx, "self._hm_send_payload_as_turn",
                                        args_fn=lambda event, usage: {
                                            "event": norm_event(event), "usage": usage})
    self._handle_reset_command = arec(ctx, "self._handle_reset_command", args_fn=ev_arg)
    self._handle_undo_command = arec(ctx, "self._handle_undo_command", args_fn=ev_arg)
    self._handle_blueprint_command = arec(ctx, "self._handle_blueprint_command",
                                          args_fn=ev_arg, build=blueprint_build)
    self._maybe_confirm_destructive_slash = confirm_stub_a
    self._send_command_ack = arec(ctx, "self._send_command_ack",
                                  args_fn=lambda source, text, label: {
                                      "source": norm_source(source), "ack": text, "name": label})
    self._gateway_plain_command_handlers = rec(ctx, "self._gateway_plain_command_handlers",
                                               args_fn=lambda: {}, build=handlers_build)
    self._gateway_idle_command_handlers = rec(ctx, "self._gateway_idle_command_handlers",
                                              args_fn=lambda: {}, build=handlers_build)
    self._async_profile_scope_for_source = rec(ctx, "self._async_profile_scope_for_source",
                                               args_fn=src_arg,
                                               build=lambda raw: NoopAsyncCM())
    self._delivery_adapter_for = rec(ctx, "self._delivery_adapter_for", args_fn=src_arg)
    self._rescue_orphaned_overflow = rec(ctx, "self._rescue_orphaned_overflow",
                                         args_fn=lambda key, adapter: {
                                             "key": key, "adapter": adapter},
                                         build=rescued_build)
    self._enqueue_fifo = rec(ctx, "self._enqueue_fifo",
                             args_fn=lambda key, event, adapter: {
                                 "key": key, "event": norm_event(event), "adapter": adapter})
    self._route_plaintext_approval_while_busy = arec(
        ctx, "self._route_plaintext_approval_while_busy",
        args_fn=lambda event, key: {"event": norm_event(event), "key": key})
    self._hm_busy_preempt = arec(ctx, "self._hm_busy_preempt",
                                 args_fn=lambda event, source, key: {
                                     "event": norm_event(event), "source": norm_source(source),
                                     "key": key})
    self._send_busy_ack_reply = arec(ctx, "self._send_busy_ack_reply",
                                     args_fn=lambda event, adapter, message: {
                                         "event": norm_event(event), "adapter": adapter,
                                         "notice": message})

    stub_modules({
        "hermes_cli.commands": {
            "resolve_command": rec(ctx, "resolve_command",
                                   args_fn=lambda cmd: {"cmd": cmd},
                                   build=lambda r: None if r is None
                                   else types.SimpleNamespace(**r)),
            "is_gateway_known_command": rec(ctx, "is_gateway_known_command",
                                            args_fn=lambda canonical: {"canonical": canonical}),
            "is_interrupt_then_dispatch": rec(ctx, "is_interrupt_then_dispatch",
                                              args_fn=lambda canonical: {"canonical": canonical}),
        },
        "gateway.session_authorities": {
            "active_authority": rec(ctx, "active_authority", args_fn=lambda runner: {})},
        "gateway.session_ingress": {
            "executing_admission": types.SimpleNamespace(
                get=lambda: state.get("executing_admission")),
            "admit_message": arec(ctx, "admit_message",
                                  args_fn=lambda authority, event: {
                                      "authority": authority, "event": norm_event(event)})},
        "hermes_cli.observability.shared_metrics_events": {
            "record_gateway_slash_command": rec(ctx, "record_gateway_slash_command",
                                                args_fn=ev_arg)},
        "hermes_cli.observability.shared_metrics_gateway": {
            "start_reply_clock": rec(ctx, "start_reply_clock",
                                     args_fn=lambda source, internal=False: {
                                         "source": norm_source(source), "internal": internal})},
        "gateway.session_moa_turn": {
            "arm_admitted_moa": rec(ctx, "arm_admitted_moa",
                                    args_fn=lambda runner, key, event: {
                                        "key": key, "event": norm_event(event)})},
        "gateway.run": {"_AGENT_PENDING_SENTINEL": SENTINEL},
        "gateway.run_plugin_injection": {
            "person_input_joined_turn": rec(ctx, "person_input_joined_turn",
                                            args_fn=lambda runner, key: {"key": key})},
        "hermes_cli.proxy_cli": {
            "format_status_text": rec(ctx, "format_status_text", args_fn=lambda: {})},
        "agent.learn_prompt": {
            "build_learn_prompt": rec(ctx, "build_learn_prompt",
                                      args_fn=lambda req: {"req": req})},
        "agent.plan_prompt": {
            "build_plan_prompt": rec(ctx, "build_plan_prompt",
                                     args_fn=lambda task: {"task": task})},
        "hermes_cli.init_command": {
            "build_init_prompt_for_cwd": rec(ctx, "build_init_prompt_for_cwd",
                                             args_fn=lambda extra, session_key: {
                                                 "extra": extra, "session_key": session_key})},
        "agent.skill_commands": {
            "get_skill_commands": rec(ctx, "get_skill_commands", args_fn=lambda: {}),
            "resolve_skill_command_key": rec(ctx, "resolve_skill_command_key",
                                             args_fn=lambda command: {"command": command}),
            "split_stacked_skill_commands": rec(ctx, "split_stacked_skill_commands",
                                                args_fn=lambda rest: {"user_instruction": rest}),
            "build_stacked_skill_invocation_message": rec(
                ctx, "build_stacked_skill_invocation_message",
                args_fn=lambda keys, instruction, task_id=None: {
                    "keys": keys, "instruction": instruction, "task_id": task_id}),
            "build_skill_invocation_message": rec(
                ctx, "build_skill_invocation_message",
                args_fn=lambda cmd_key, user_instruction, task_id=None: {
                    "cmd_key": cmd_key, "user_instruction": user_instruction,
                    "task_id": task_id}),
        },
        "agent.skill_utils": {
            "get_disabled_skill_names": rec(ctx, "get_disabled_skill_names",
                                            args_fn=lambda platform=None: {"platform": platform})},
        "gateway.run_inbound_transcription": {
            "hand_prepared_voice_to_turn": rec(ctx, "hand_prepared_voice_to_turn",
                                               args_fn=ev_arg)},
        "persistence.core.errors": {
            "AuthorityRefusal": SUP["AuthorityRefusal"],
            "StoreUnavailable": SUP["StoreUnavailable"]},
    })
    ev = EventDouble(args.get("event") or {}, ctx) if args.get("event") is not None else None
    return self, ev


# ── per-func invocation ──
def invoke(func, args, self, ctx, ev):
    src = make_source(args.get("source"))
    key = args.get("key", "")
    run = _asyncio.run
    if func == "_hm_resolve_command":
        return run(self._hm_resolve_command(ev, src, key))
    if func == "_hm_confirm_destructive":
        hname = args.get("handler")
        handler = make_handler(hname) if isinstance(hname, str) else hname
        return run(self._hm_confirm_destructive(ev, args.get("command", ""),
                                                args.get("detail", ""), handler))
    if func in ("_hm_cmd_new", "_hm_cmd_start", "_hm_cmd_egress", "_hm_cmd_learn",
                "_hm_cmd_plan", "_hm_cmd_init", "_hm_cmd_blueprint", "_hm_cmd_undo",
                "_hm_cmd_queue", "_hm_cmd_steer"):
        return run(getattr(self, func)(ev, src, key))
    if func == "_hm_rewrite_turn_to_prompt":
        desc = args.get("build") or {}
        fn, bargs = desc.get("fn", ""), dict(desc.get("args") or {})
        build = lambda: ctx.call(fn, bargs)   # noqa: E731
        return run(self._hm_rewrite_turn_to_prompt(ev, src, args.get("name", ""),
                                                   args.get("ack", ""), build))
    if func == "_hm_dispatch_canonical_command":
        return run(self._hm_dispatch_canonical_command(ev, src, key, args.get("canonical")))
    if func == "_hm_skill_slash_rewrite":
        return self._hm_skill_slash_rewrite(ev, src, key, args.get("command"))
    if func == "_hm_pending_reply_intercepts":
        return run(self._hm_pending_reply_intercepts(ev, src, key))
    if func == "_hm_dispatch_idle_commands":
        return run(self._hm_dispatch_idle_commands(ev, src, key))
    if func == "_hm_rescue_orphaned_fifo":
        event, source, is_internal = self._hm_rescue_orphaned_fifo(
            ev, src, args.get("is_internal", False), key)
        return [norm_event(event), norm_source(source), is_internal]
    if func == "_hm_authority_admission":
        return run(self._hm_authority_admission(ev, src, key, args.get("is_internal", False)))
    if func == "_hm_admit_to_fifo":
        return run(self._hm_admit_to_fifo(args.get("authority"), ev, src, key,
                                          args.get("busy", False)))
    if func == "_handle_message":
        return run(self._handle_message(ev))
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
    "event.get_command", "event.get_command_args",
    "resolve_command", "is_gateway_known_command", "is_interrupt_then_dispatch",
    "self._hm_quick_commands", "self._hm_expand_alias_quick_command",
    "self._check_slash_access", "self._hm_command_hooks", "self._hm_drop_held_input",
    "self._maybe_confirm_destructive_slash",
    "self._is_telegram_topic_root_lobby", "self._telegram_topic_root_new_message",
    "person_input_joined_turn", "t", "format_status_text",
    "self._send_command_ack", "build_learn_prompt", "build_plan_prompt",
    "build_init_prompt_for_cwd", "self._session_key_for_source",
    "self._handle_blueprint_command", "self._hm_send_payload_as_turn",
    "self._gateway_plain_command_handlers", "self._gateway_idle_command_handlers",
    "self._async_profile_scope_for_source",
    "self._hm_bundle_slash_rewrite", "get_skill_commands", "resolve_skill_command_key",
    "self._hm_unknown_slash_reply", "split_stacked_skill_commands",
    "get_disabled_skill_names", "build_stacked_skill_invocation_message",
    "build_skill_invocation_message",
    "self._handle_reset_command", "self._handle_undo_command",
    "self._hm_update_prompt_reply", "self._hm_clarify_reply", "self._hm_slash_confirm_reply",
    "self._route_stop_epoch", "self._hm_dispatch_quick_and_plugin_commands",
    "self._admit_command_turn",
    "self._delivery_adapter_for", "self._rescue_orphaned_overflow", "self._enqueue_fifo",
    "active_authority", "self._is_session_running", "stamp_busy_arrival",
    "settle_busy_arrival",
    "self._route_plaintext_approval_while_busy", "self._hm_busy_preempt",
    "self._hm_post_admission_consume", "hand_prepared_voice_to_turn",
    "note_busy_admitting", "pending_setup_busy_notice", "self._send_busy_ack_reply",
    "admit_message",
    "self._hm_admit_event", "record_gateway_slash_command", "self._hm_estop_gate",
    "self._hm_evict_idle_stale_agent", "self._hm_evict_reaped_agent",
    "self._hm_handle_running_session_message",
    "self._should_send_telegram_lobby_reminder", "self._telegram_topic_root_lobby_message",
    "self._claim_active_session_slot", "self._session_state", "time.time",
    "start_reply_clock", "self._persist_active_agents",
    "self._begin_session_run_generation", "arm_admitted_moa",
    "self._handle_message_with_agent", "self._run_post_turn_hooks",
    "self._restore_pending_one_turn_model_override", "self._clear_durable_active_turn",
    "self._release_running_agent_state", "self._release_turn_lease",
    # dynamic handler seams (fixture-chosen names fired through the handler
    # maps; passed through as strings in C, not literals)
    "handler.plain", "handler.idle",
]
DYNAMIC_SEAMS = {"handler.plain", "handler.idle"}


def cross_check():
    csrc = open(_REPO / "src" / "cli" / "gateway_inbound_flow2.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES
                 if n not in DYNAMIC_SEAMS and f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_gateway_inbound_flow2: seams absent from C port: {missing_c}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'(?:rec|arec)\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'ctx\.call\(\s*"([^"]+)"', osrc))
    used |= {"self._session_state"}
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_gateway_inbound_flow2: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "gateway_inbound_flow2.jsonl"
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
            print(f"FATAL oracle_gateway_inbound_flow2: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        badf = fix_funcs - FUNC_SET
        if badf:
            print(f"FATAL oracle_gateway_inbound_flow2: fixture funcs outside port set: {sorted(badf)}")
            sys.exit(2)
        missingf = FUNC_SET - fix_funcs
        if missingf:
            print(f"FATAL oracle_gateway_inbound_flow2: ported funcs without fixtures: {sorted(missingf)}")
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
    print(f"ORACLE gateway_inbound_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


if __name__ == "__main__":
    main()