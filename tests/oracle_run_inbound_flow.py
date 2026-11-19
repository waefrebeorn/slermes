#!/usr/bin/env python3
"""Oracle for the gateway/run_inbound.py FLOW port
(src/gateway/run_inbound_flow.c).

Drives the LIVE GatewayInboundMixin methods with SCRIPTED seams: every policy
lookup, refusal helper, clarify/slash-confirm store call, adapter method,
eviction interrupt, command resolution, i18n call and clock is a recording stub
fed the same scripts the C harness feeds its universal seam. Compares the
DECISION SEQUENCE (ordered seam events {"name","args","ret"} in "trace") plus
the final return ("ret") and the mutated self/world state ("state", including
the final event double as "event_out").

The live defs are AST-extracted: the GatewayInboundMixin ClassDef execs WHOLE
with stub mixin bases (ported siblings then compose for real); module constants
seeded (Assign AND AnnAssign, plain-Name targets). Platform / MessageType are
the REAL enums AST-extracted from gateway/config.py / gateway/platforms/event.py.
Every exec namespace is a registered sys.modules module (py3.14 dataclasses
_is_type needs cls.__module__ resolvable). The heavy quarry import chain is
never imported; in-function imports resolve through sys.modules stubs.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE run_inbound_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- Platform/MessageType members travel as .value strings; agents/adapters as
  attribute doubles built from spec dicts (id strings in seam args).
- _AGENT_PENDING_SENTINEL identity is a single shared object(); the fixture
  {"sentinel": true} materializes to it; seam args project {"__sentinel__": true}.
- tools.clarify_gateway.CANCELLED == "\\x00cancelled" (embedded NUL) is projected
  to the visible literal "\\\\x00cancelled" in the resolve seam's status arg.
- logger calls are silent on both sides (args still evaluated for raises).
- self._peek_session_state records the state node copy and returns a backed
  view so persistent/conversation writes persist into out.state on both sides.
- GATEWAY_KNOWN_COMMANDS is a snapshot of the live frozenset (87 names).
- Fixture corpus folds ASCII only and restricts adapter/pending/outcome scripts
  to the documented null|dict(|str) domains.
"""
import ast
import asyncio as _asyncio
import datetime
import enum
import json
import os
import re
import sys
import textwrap
import types
from pathlib import Path
from typing import Any, Optional
import __future__ as _fut

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
RI_SRC = os.path.join(Q, "gateway", "run_inbound.py")
CONFIG_SRC = os.path.join(Q, "gateway", "config.py")
EVENT_SRC = os.path.join(Q, "gateway", "platforms", "event.py")

_FLAGS = _fut.annotations.compiler_flag

# hermes_cli/commands.py:GATEWAY_KNOWN_COMMANDS snapshot (sorted, 87 names).
RI_KNOWN = [
    "agents", "approvals", "approve", "autoprune", "bg", "blueprint", "bp",
    "branch", "btw", "bundles", "busy", "codex-runtime", "codex_runtime",
    "commands", "compact", "compress", "context", "ctx", "curator", "debug",
    "deny", "diff", "discard", "egress", "fast", "footer", "fork", "goal",
    "hb", "heartbeat", "help", "init", "insights", "kanban", "learn", "login",
    "loop", "memory", "moa", "model", "new", "pause", "personality", "plan",
    "platform", "proactive", "profile", "q", "queue", "reasoning", "refine",
    "reload-mcp", "reload-skills", "reload_mcp", "reload_skills", "reset",
    "restart", "resume", "retry", "review", "rollback", "s", "save",
    "sessions", "set-home", "sethome", "skills", "start", "status", "steer",
    "stop", "subgoal", "suggest", "suggestions", "tasks", "title", "topic",
    "topup", "undo", "update", "usage", "v", "verbose", "version", "voice",
    "whoami", "yolo",
]


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


# ── support: real Platform / MessageType enums ──
def extract_support():
    mod = new_module("ri_oracle_support")
    ns = mod.__dict__
    ns.update({"Enum": enum.Enum, "Any": Any, "Optional": Optional})
    gsrc = open(CONFIG_SRC, encoding="utf-8").read()
    for node in class_nodes(gsrc, ["Platform"]):
        exec_node(gsrc, node, ns, CONFIG_SRC)
    evsrc = open(EVENT_SRC, encoding="utf-8").read()
    for node in class_nodes(evsrc, ["MessageType"]):
        exec_node(evsrc, node, ns, EVENT_SRC)
    missing = [k for k in ("Platform", "MessageType") if k not in ns]
    if missing:
        print(f"FATAL oracle_run_inbound_flow: support missing: {missing}")
        sys.exit(2)
    return ns


SUP = extract_support()
Platform = SUP["Platform"]
MessageType = SUP["MessageType"]


# ── run_inbound namespace ──
def extract_ri():
    mod = new_module("ri_oracle_ri")
    ns = mod.__dict__
    import concurrent.futures
    import logging
    import time
    from contextlib import suppress
    ns.update({"logging": logging, "TYPE_CHECKING": False,
               "asyncio": _asyncio, "concurrent": concurrent.futures,
               "dataclasses": __import__("dataclasses"), "json": json,
               "os": os, "re": re, "time": time, "suppress": suppress,
               "Path": Path, "Any": Any, "Dict": dict, "List": list,
               "Optional": Optional, "Tuple": tuple,
               "Platform": Platform, "MessageType": MessageType})
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
    seed_constants(src, ns, RI_SRC)
    ns["logger"] = _SilentLogger()
    mixin = ns.get("GatewayInboundMixin")
    if mixin is None:
        print("FATAL oracle_run_inbound_flow: GatewayInboundMixin missing")
        sys.exit(2)
    for meth in RI_FUNCS:
        if not hasattr(mixin, meth):
            print(f"FATAL oracle_run_inbound_flow: mixin missing {meth}")
            sys.exit(2)
    return ns


# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
        if isinstance(x, enum.Enum):
            return x.value
        if isinstance(x, (SessionStateNS, TurnNS, StateNS)):
            return conv(object.__getattribute__(x, "_d"))
        if isinstance(x, EventDouble):
            return norm_event(x)
        if isinstance(x, AgentDouble):
            return {"id": x.id}
        if isinstance(x, (RiAdapterDouble,)):
            return dict(object.__getattribute__(x, "_spec"))
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, datetime.datetime):
            return x.timestamp()
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
    "OSError": OSError, "TimeoutError": TimeoutError, "ValueError": ValueError,
    "TypeError": TypeError, "RuntimeError": RuntimeError, "KeyError": KeyError,
    "AttributeError": AttributeError, "ImportError": ImportError,
    "Exception": Exception, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── per-case script backend (mirrors the C ri_call exactly) ──
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


SENTINEL = object()


class AgentDouble:
    def __init__(self, spec, ctx):
        self.id = spec.get("id")
        self._ctx = ctx
        if spec.get("has_steer"):
            self.steer = lambda *a: None
        if spec.get("has_redirect"):
            self.redirect = lambda *a: None
        self._supports_active_turn_redirect = spec.get("supports_redirect", False)

    def interrupt(self, text):
        return self._ctx.call("agent.interrupt", {"agent": self.id, "text": text})


def make_agent(spec, ctx):
    if spec is None:
        return None
    if isinstance(spec, dict):
        if spec.get("sentinel"):
            return SENTINEL
        return AgentDouble(spec, ctx)
    return spec


def agent_proj(a):
    if a is None:
        return None
    if a is SENTINEL:
        return {"__sentinel__": True}
    if isinstance(a, AgentDouble):
        return a.id
    return a


class RiAdapterDouble:
    def __init__(self, spec, ctx):
        object.__setattr__(self, "_spec", dict(spec))
        self.id = spec.get("id")
        self._ctx = ctx
        self._pending_messages = spec.get("pending_messages", {})

    def _event_session_key(self, event):
        return self._ctx.call("adapter._event_session_key",
                              {"adapter": self.id, "event": norm_event(event)})

    def _discard_text_debounce(self, key):
        return self._ctx.call("adapter._discard_text_debounce",
                              {"adapter": self.id, "key": key})


class RiCardAdapterDouble(RiAdapterDouble):
    async def retire_clarify_card(self, clarify_id, text):
        return self._ctx.call("adapter.retire_clarify_card",
                              {"adapter": self.id, "clarify_id": clarify_id, "text": text})


def make_adapter(r, ctx):
    if r is None:
        return None
    if isinstance(r, dict):
        if r.get("has_retire_card"):
            return RiCardAdapterDouble(r, ctx)
        return RiAdapterDouble(r, ctx)
    return r


def adapter_id(a):
    if a is None:
        return None
    if isinstance(a, RiAdapterDouble):
        return a.id
    return a


def make_source(d):
    if d is None:
        return None
    d = dict(d)
    if d.get("platform"):
        d["platform"] = Platform(d["platform"])
    return types.SimpleNamespace(**d)


def norm_source(s):
    if s is None:
        return None
    if isinstance(s, types.SimpleNamespace):
        d = dict(vars(s))
        p = d.get("platform")
        d["platform"] = p.value if isinstance(p, enum.Enum) else p
        return d
    return s


def make_pending(spec):
    if spec is None:
        return None
    if isinstance(spec, dict):
        return types.SimpleNamespace(**spec)
    return spec


class TurnNS:
    def __init__(self, d, ctx):
        object.__setattr__(self, "_d", d)
        object.__setattr__(self, "_ctx", ctx)

    def __getattr__(self, k):
        d = object.__getattribute__(self, "_d")
        if k not in d:
            raise AttributeError(k)
        if k == "agent":
            return make_agent(d[k], object.__getattribute__(self, "_ctx"))
        v = d[k]
        return StateNS(v) if isinstance(v, dict) else v


class SessionStateNS:
    def __init__(self, d, ctx):
        object.__setattr__(self, "_d", d)
        object.__setattr__(self, "_ctx", ctx)

    def __getattr__(self, k):
        d = object.__getattribute__(self, "_d")
        if k not in d:
            raise AttributeError(k)
        if k == "turn":
            return TurnNS(d[k], object.__getattribute__(self, "_ctx"))
        v = d[k]
        return StateNS(v) if isinstance(v, dict) else v


class EventDouble:
    def __init__(self, d, ctx):
        dd = dict(d or {})
        if "source" in dd:
            dd["source"] = make_source(dd["source"])
        if dd.get("message_type"):
            dd["message_type"] = MessageType(dd["message_type"])
        if isinstance(dd.get("timestamp"), (int, float)) and not isinstance(dd.get("timestamp"), bool):
            dd["timestamp"] = datetime.datetime.fromtimestamp(dd["timestamp"], tz=datetime.timezone.utc)
        if isinstance(dd.get("raw_message"), dict):
            rm = dict(dd["raw_message"])
            if isinstance(rm.get("edit_date"), (int, float)) and not isinstance(rm.get("edit_date"), bool):
                rm["edit_date"] = datetime.datetime.fromtimestamp(rm["edit_date"], tz=datetime.timezone.utc)
            dd["raw_message"] = types.SimpleNamespace(**rm)
        object.__setattr__(self, "_d", dd)
        object.__setattr__(self, "_ctx", ctx)

    def __getattr__(self, k):
        d = object.__getattribute__(self, "_d")
        if k in d:
            return d[k]
        raise AttributeError(k)

    def __setattr__(self, k, v):
        d = object.__getattribute__(self, "_d")
        if k == "text":
            tr = d.get("text_raise")
            if isinstance(tr, dict):
                raise make_exc(tr)
            if d.get("text_readonly"):
                raise AttributeError("text readonly")
        d[k] = v

    def get_command(self):
        return object.__getattribute__(self, "_ctx").call("event.get_command", {})

    def get_command_args(self):
        return object.__getattribute__(self, "_ctx").call("event.get_command_args", {})

    def sender_id(self):
        return object.__getattribute__(self, "_ctx").call("event.sender_id", {})


def norm_event(ev):
    if ev is None:
        return None
    d = object.__getattribute__(ev, "_d")
    out = {}
    for k, v in d.items():
        if k == "source":
            out[k] = norm_source(v)
        elif k == "message_type":
            out[k] = v.value if isinstance(v, enum.Enum) else v
        elif k == "timestamp":
            out[k] = v.timestamp() if isinstance(v, datetime.datetime) else v
        elif k == "raw_message":
            if isinstance(v, types.SimpleNamespace):
                rd = dict(vars(v))
                if isinstance(rd.get("edit_date"), datetime.datetime):
                    rd["edit_date"] = rd["edit_date"].timestamp()
                out[k] = rd
            else:
                out[k] = v
        else:
            out[k] = v
    return out


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


def make_store(spec, ctx):
    if spec is None:
        return None
    store = types.SimpleNamespace()
    if spec.get("has_peek"):
        store.peek_session_id = lambda key: ctx.call("store.peek_session_id", {"key": key})
    if spec.get("has_ended"):
        store._is_session_ended_in_db = lambda sid: ctx.call(
            "store._is_session_ended_in_db", {"session_id": sid})
    return store


RI_FUNCS = [
    "_prompt_admin_ids", "_hm_update_reply_refused", "_refused_clarify",
    "_refused_slash_confirm", "_slash_confirm_text_choices",
    "_release_pending_clarify", "_release_clarify_owning_turn",
    "_hm_evict_running_agent", "_hm_evict_reaped_agent",
    "_hm_merge_pending_for_source", "_hm_drop_held_input", "_hm_text_only",
    "_hm_in_telegram_grace", "_hm_busy_telegram_grace_queue", "_hm_busy_steer",
    "_hm_busy_interrupt", "_clarify_takeover", "_hm_busy_preempt",
    "_hm_send_payload_as_turn", "_hm_unknown_slash_reply",
    "_hm_bundle_slash_rewrite", "_consume_pending_native_image_paths",
    "_inbound_original_names", "_restore_pending_one_turn_model_override",
]
RI_ASYNC = {"_release_pending_clarify", "_release_clarify_owning_turn",
            "_hm_busy_interrupt", "_hm_busy_preempt"}

NS_RI = extract_ri()


# ── per-case setup ──
def setup_case(args, state, ctx):
    self = object.__new__(NS_RI["GatewayInboundMixin"])
    self._draining = state.get("_draining", False)
    self.session_store = make_store(state.get("session_store"), ctx)
    if "_pending_model_notes" in state:
        self._pending_model_notes = state["_pending_model_notes"]

    def ev_arg(event):
        return {"event": norm_event(event)}

    def src_arg(source):
        return {"source": norm_source(source)}

    def peek(key):
        node = (state.get("session_states") or {}).get(key)
        ctx.trace.append({"name": "self._peek_session_state", "args": {"key": key},
                          "ret": canon(node) if node is not None else None})
        return SessionStateNS(node, ctx) if node is not None else None

    self._peek_session_state = peek
    self._delivery_adapter_for = lambda source: make_adapter(
        ctx.call("self._delivery_adapter_for", src_arg(source)), ctx)
    self._intake_adapter_for = lambda source: make_adapter(
        ctx.call("self._intake_adapter_for", src_arg(source)), ctx)
    self._interrupt_running_turn = lambda key, interrupt_reason=None, invalidation_reason=None, tool_reason=None: \
        ctx.call("self._interrupt_running_turn",
                 {"key": key, "interrupt_reason": interrupt_reason,
                  "invalidation_reason": invalidation_reason, "tool_reason": tool_reason})
    self._drop_turn_slot = lambda key, run_generation=None: \
        ctx.call("self._drop_turn_slot", {"key": key, "run_generation": run_generation})
    self._steer_text_with_origin = lambda text, event: \
        ctx.call("self._steer_text_with_origin", {"text": text, "event": norm_event(event)})
    self._steer_running_agent = lambda agent, text: \
        ctx.call("self._steer_running_agent", {"agent": agent_proj(agent), "text": text})
    self._fold_into_running_turn = lambda agent, key, event: \
        ctx.call("self._fold_into_running_turn",
                 {"agent": agent_proj(agent), "key": key, "event": norm_event(event)})
    self._queue_or_replace_pending_event = lambda key, event: \
        ctx.call("self._queue_or_replace_pending_event", {"key": key, "event": norm_event(event)})
    self._redirect_active_turn = lambda agent, text, key, event: \
        ctx.call("self._redirect_active_turn",
                 {"agent": agent_proj(agent), "text": text, "key": key, "event": norm_event(event)})
    self._pending_event_audio_paths = lambda event: \
        ctx.call("self._pending_event_audio_paths", ev_arg(event))
    self._busy_interrupt_text = lambda event: \
        ctx.call("self._busy_interrupt_text", ev_arg(event))
    self._current_session_run_generation = lambda key: \
        ctx.call("self._current_session_run_generation", {"key": key})
    self._effective_busy_input_mode = lambda source: \
        ctx.call("self._effective_busy_input_mode", src_arg(source))
    self._effective_busy_text_mode = lambda source: \
        ctx.call("self._effective_busy_text_mode", src_arg(source))
    self._resolve_busy_steer_or_redirect = arec(
        ctx, "self._resolve_busy_steer_or_redirect",
        args_fn=lambda event, key, mode, agent: {
            "event": norm_event(event), "key": key, "mode": mode, "agent": agent_proj(agent)},
        build=lambda r: None if r is None else types.SimpleNamespace(**r))
    self._busy_interrupt_and_ack = arec(
        ctx, "self._busy_interrupt_and_ack",
        args_fn=lambda event, key, adapter, st, agent, outcome: {
            "event": norm_event(event), "key": key, "adapter": adapter_id(adapter),
            "state": canon(st) if st is not None else None,
            "agent": agent_proj(agent),
            "outcome": canon(outcome) if outcome is not None else None})
    self._enqueue_fifo = lambda key, event, adapter: \
        ctx.call("self._enqueue_fifo",
                 {"key": key, "event": norm_event(event), "adapter": adapter_id(adapter)})
    self._is_session_run_current = lambda key, run_generation: \
        ctx.call("self._is_session_run_current", {"key": key, "run_generation": run_generation})
    self._restore_session_model_override = lambda key, snapshot: \
        ctx.call("self._restore_session_model_override", {"key": key, "snapshot": snapshot})

    NS_RI["t"] = lambda key, **kw: ctx.call("t", {"key": key, **kw})
    NS_RI["approval_input_words"] = rec(ctx, "approval_input_words",
                                        args_fn=lambda input_key: {"input_key": input_key})
    NS_RI["note_busy_outcome"] = lambda event, outcome: ctx.call(
        "note_busy_outcome",
        {"event": norm_event(event), "outcome": canon(outcome) if outcome is not None else None})
    NS_RI["time"] = types.SimpleNamespace(time=lambda: ctx.call("time.time", {}))
    NS_RI["logger"] = _SilentLogger()

    stub_modules({
        "gateway.slash_access": {
            "policy_for_runner_source": rec(
                ctx, "policy_for_runner_source",
                args_fn=lambda slf, source: {"source": norm_source(source)},
                build=lambda r: None if r is None else (
                    types.SimpleNamespace(**r) if isinstance(r, dict) else r))},
        "gateway.approval_actor": {
            "update_prompt_refusal": rec(ctx, "update_prompt_refusal",
                                         args_fn=lambda user_id, admin_ids=None:
                                         {"user_id": user_id, "admin_ids": admin_ids}),
            "clarify_refusal": rec(ctx, "clarify_refusal",
                                   args_fn=lambda clarify_id, sender_id, admin_ids=None, admitted=None:
                                   {"clarify_id": clarify_id, "sender_id": sender_id,
                                    "admin_ids": admin_ids, "admitted": admitted}),
            "slash_confirm_refusal": rec(ctx, "slash_confirm_refusal",
                                         args_fn=lambda session_key, confirm_id, sender_id, admin_ids=None, admitted=None:
                                         {"session_key": session_key, "confirm_id": confirm_id,
                                          "sender_id": sender_id, "admin_ids": admin_ids, "admitted": admitted}),
            "refusal_text": rec(ctx, "refusal_text", args_fn=lambda requester: {"requester": requester})},
        "gateway.relay.admitted_authors": {
            "answer_admitted": rec(ctx, "answer_admitted", args_fn=lambda event: {"event": norm_event(event)})},
        "tools.clarify_gateway": {
            "resolve_gateway_clarify": rec(ctx, "clarify_gateway.resolve_gateway_clarify",
                                           args_fn=lambda clarify_id, status:
                                           {"clarify_id": clarify_id, "status": status.replace("\x00", "\\x00")}),
            "get_pending_for_session": rec(ctx, "clarify_gateway.get_pending_for_session",
                                           args_fn=lambda session_key, include_choice_prompts=False:
                                           {"session_key": session_key, "include_choice_prompts": include_choice_prompts},
                                           build=make_pending),
            "CANCELLED": "\x00cancelled"},
        "gateway.platforms.base": {
            "merge_pending_message_event": rec(ctx, "merge_pending_message_event",
                                               args_fn=lambda pending, key, event, merge_text=False:
                                               {"pending_messages": pending, "key": key,
                                                "event": norm_event(event), "merge_text": merge_text})},
        "gateway.run": {
            "_float_env": rec(ctx, "_float_env", args_fn=lambda name, default: {"name": name, "default": default}),
            "_check_unavailable_skill": rec(ctx, "_check_unavailable_skill", args_fn=lambda command: {"command": command}),
            "_INTERRUPT_REASON_EVICTED": "Session ended while the turn was running",
            "_INTERRUPT_TOOL_REASON_EVICTED": "session evicted",
            "_AGENT_PENDING_SENTINEL": SENTINEL},
        "hermes_cli.commands": {"GATEWAY_KNOWN_COMMANDS": frozenset(RI_KNOWN)},
        "agent.skill_bundles": {
            "resolve_bundle_command_key": rec(ctx, "resolve_bundle_command_key", args_fn=lambda command: {"command": command}),
            "build_bundle_invocation_message": rec(ctx, "build_bundle_invocation_message",
                                                   args_fn=lambda bundle_key, instruction, task_id=None, platform=None:
                                                   {"bundle_key": bundle_key, "instruction": instruction,
                                                    "task_id": task_id, "platform": platform})},
    })
    return self


def invoke(func, args, self, ctx, ev):
    source = make_source(args.get("source"))
    if func == "_prompt_admin_ids":
        return self._prompt_admin_ids(ev)
    if func == "_hm_update_reply_refused":
        return self._hm_update_reply_refused(ev)
    if func == "_refused_clarify":
        return self._refused_clarify(ev, args.get("clarify_id", ""))
    if func == "_refused_slash_confirm":
        return self._refused_slash_confirm(ev, args.get("session_key", ""), args.get("confirm_id", ""))
    if func == "_slash_confirm_text_choices":
        return self._slash_confirm_text_choices()
    if func == "_release_pending_clarify":
        return _asyncio.run(self._release_pending_clarify(make_pending(args.get("pending")), source))
    if func == "_release_clarify_owning_turn":
        return _asyncio.run(self._release_clarify_owning_turn(args.get("session_key", ""), source))
    if func == "_hm_evict_running_agent":
        return self._hm_evict_running_agent(args.get("key", ""), args.get("reason", ""))
    if func == "_hm_evict_reaped_agent":
        return self._hm_evict_reaped_agent(args.get("key", ""))
    if func == "_hm_merge_pending_for_source":
        return self._hm_merge_pending_for_source(source, args.get("key", ""), ev,
                                                 merge_text=args.get("merge_text", False))
    if func == "_hm_drop_held_input":
        return self._hm_drop_held_input(ev)
    if func == "_hm_text_only":
        return self._hm_text_only(ev)
    if func == "_hm_in_telegram_grace":
        return self._hm_in_telegram_grace(ev, source, args.get("key", ""))
    if func == "_hm_busy_telegram_grace_queue":
        return self._hm_busy_telegram_grace_queue(ev, source, args.get("key", ""),
                                                  args.get("effective_busy_input_mode", ""))
    if func == "_hm_busy_steer":
        return self._hm_busy_steer(ev, make_agent(args.get("agent"), ctx), args.get("key", ""))
    if func == "_hm_busy_interrupt":
        return _asyncio.run(self._hm_busy_interrupt(ev, source, make_agent(args.get("agent"), ctx),
                                                    args.get("key", "")))
    if func == "_clarify_takeover":
        return self._clarify_takeover(ev, args.get("key", ""))
    if func == "_hm_busy_preempt":
        return _asyncio.run(self._hm_busy_preempt(ev, source, args.get("key", "")))
    if func == "_hm_send_payload_as_turn":
        return self._hm_send_payload_as_turn(ev, args.get("usage", ""))
    if func == "_hm_unknown_slash_reply":
        return self._hm_unknown_slash_reply(args.get("command", ""), source)
    if func == "_hm_bundle_slash_rewrite":
        return self._hm_bundle_slash_rewrite(ev, source, args.get("key", ""), args.get("command", ""))
    if func == "_consume_pending_native_image_paths":
        return self._consume_pending_native_image_paths(args.get("session_key", ""))
    if func == "_inbound_original_names":
        return self._inbound_original_names(ev)
    if func == "_restore_pending_one_turn_model_override":
        return self._restore_pending_one_turn_model_override(args.get("session_key", ""),
                                                             args.get("run_generation"))
    raise RuntimeError(f"unknown func {func}")


# ── per-case drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    state = canon(inp.get("state") or {})
    ctx = Ctx(inp.get("io"))
    self = setup_case(args, state, ctx)
    ev = EventDouble(args.get("event"), ctx) if args.get("event") is not None else None
    try:
        ret = invoke(func, args, self, ctx, ev)
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    final = canon(state)
    if hasattr(self, "_pending_model_notes"):
        final["_pending_model_notes"] = self._pending_model_notes
    final["event_out"] = norm_event(ev) if ev is not None else None
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": final}


# ── three-way seam-name cross-check (C port ↔ oracle stubs ↔ fixture io) ──
SEAM_NAMES = [
    "policy_for_runner_source", "update_prompt_refusal", "event.sender_id",
    "answer_admitted", "clarify_refusal", "slash_confirm_refusal", "refusal_text",
    "approval_input_words", "clarify_gateway.resolve_gateway_clarify",
    "self._delivery_adapter_for", "t", "adapter.retire_clarify_card",
    "clarify_gateway.get_pending_for_session", "self._interrupt_running_turn",
    "self._drop_turn_slot", "store.peek_session_id", "store._is_session_ended_in_db",
    "merge_pending_message_event", "self._intake_adapter_for",
    "adapter._event_session_key", "adapter._discard_text_debounce", "_float_env",
    "self._peek_session_state", "time.time", "self._enqueue_fifo",
    "self._steer_text_with_origin", "self._steer_running_agent",
    "self._fold_into_running_turn", "self._queue_or_replace_pending_event",
    "self._redirect_active_turn", "self._pending_event_audio_paths",
    "self._busy_interrupt_text", "agent.interrupt",
    "self._current_session_run_generation", "self._effective_busy_input_mode",
    "event.get_command", "self._effective_busy_text_mode",
    "self._resolve_busy_steer_or_redirect", "note_busy_outcome",
    "self._busy_interrupt_and_ack", "event.get_command_args",
    "_check_unavailable_skill", "resolve_bundle_command_key",
    "build_bundle_invocation_message", "self._is_session_run_current",
    "self._restore_session_model_override",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "run_inbound_flow.c", encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_run_inbound_flow: seams absent from C port: {missing_c}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'(?:rec|arec)\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'ctx\.call\(\s*"([^"]+)"', osrc))
    used |= {"self._peek_session_state"}
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_run_inbound_flow: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "run_inbound_flow.jsonl"
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
            print(f"FATAL oracle_run_inbound_flow: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        badf = fix_funcs - set(RI_FUNCS)
        if badf:
            print(f"FATAL oracle_run_inbound_flow: fixture funcs without a port: {sorted(badf)}")
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
    print(f"ORACLE run_inbound_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


if __name__ == "__main__":
    main()
