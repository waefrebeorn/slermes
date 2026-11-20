#!/usr/bin/env python3
"""Oracle for the UNCLAIMED event-handler / settings / server-request FLOW
remainder of hermes_cli/gateway_agent.py (src/cli/gateway_agent_flow2.c,
second wave after port_gateway_agent_flow.c / gateway_agent_cli_flow.c
claimed the reasoning-route, submit-staging, interrupt and CLI-summary
cores).

Drives the LIVE GatewayAgent methods with SCRIPTED seams: every connection
RPC, callback sink, i18n call, clock, tool-preview builder, todo-store
restore and server-request modal answer is a recording stub fed the same
scripts the C harness feeds its universal seam. Compares the DECISION
SEQUENCE (ordered seam events {"name","args","ret"} in "trace") plus the
final return ("ret"), the mutated agent state ("state") and the mutated
working args ("args", e.g. the starts map).

The live defs are AST-extracted from hermes_cli/gateway_agent.py (the whole
GatewayAgent / _Turn / _CompressorView ClassDefs exec; module constants
seeded, Assign AND AnnAssign, plain-Name targets only). Support types are
the REAL ones, AST-extracted: hermes_cli.gateway_client.GatewayClientError /
GatewayRPCError and agent.credits_tracker.AgentNotice (dataclass). Every
exec namespace is a registered sys.modules module (py3.14 dataclasses
_is_type needs cls.__module__ resolvable). The heavy quarry import chain is
never imported.

Claimed siblings (flow1 ports) and unported collaborators are SEAMED as
"self.<name>" instance recorders (or module-global recorders for the
claimed module functions _effort_word / clarify_reply); the 36 ported
methods stay LIVE on the class and compose through normal attribute lookup.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE gateway_agent_flow2: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}
  (GatewayRPCError adds code/data; OSError adds errno when set).
- Callback attributes travel as seam-name STRINGS (or null); the oracle
  materializes recording sinks and projects them back to the markers.
- self._turn travels as an attribute dict; seen/answered sets project to
  SORTED lists; the real _Turn class is materialized live.
- The connection / TodoStore doubles are installed silently; only their
  call/reply/restore seams are traced. self._state is a traced get-or-create
  accessor returning a borrowed view whose apply_usage fires state.apply_usage.
- The _lost(turn) abandon closure is a pure factory: it projects to
  {"__callable__": "_lost", "turn": <session_id>} and is never invoked.
- diff_printer(_cprint) projects its argument to {"__callable__": "_cprint"}
  and its scripted result is a seam-name string fired once per diff line.
- logger calls are silent on both sides (not traced).
- Fixture corpus is ASCII; integral numbers only; bool counts as int for
  the bare isinstance(x, int) gates (Python semantics); documented domain
  in the port header.
"""
import ast
import json
import os
import re
import sys
import textwrap
import types
from dataclasses import asdict, is_dataclass
from pathlib import Path
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
GA_SRC = os.path.join(Q, "hermes_cli", "gateway_agent.py")
GC_SRC = os.path.join(Q, "hermes_cli", "gateway_client.py")
CT_SRC = os.path.join(Q, "agent", "credits_tracker.py")

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


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


# ── support namespace: real error family + AgentNotice dataclass ──
def extract_support():
    mod = new_module("ga2_oracle_support")
    ns = mod.__dict__
    import dataclasses
    ns.update({"dataclass": dataclasses.dataclass, "field": dataclasses.field,
               "Optional": __import__("typing").Optional})
    gsrc = open(GC_SRC, encoding="utf-8").read()
    for node in class_nodes(gsrc, ["GatewayClientError", "GatewayRPCError"]):
        exec_node(gsrc, node, ns, GC_SRC)
    csrc = open(CT_SRC, encoding="utf-8").read()
    for node in class_nodes(csrc, ["AgentNotice"]):
        exec_node(csrc, node, ns, CT_SRC)
    required = ["GatewayClientError", "GatewayRPCError", "AgentNotice"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_gateway_agent_flow2: support missing: {missing}")
        sys.exit(2)
    return ns


SUP = extract_support()


# ── gateway_agent namespace ──
def extract_ga():
    mod = new_module("ga2_oracle_ga")
    ns = mod.__dict__
    import logging
    import queue as _queue
    import threading as _threading
    import time as _time
    import uuid as _uuid
    ns.update({"logging": logging, "queue": _queue, "threading": _threading,
               "time": _time, "uuid": _uuid, "json": json, "suppress": suppress,
               "GatewayClientError": SUP["GatewayClientError"],
               "GatewayRPCError": SUP["GatewayRPCError"],
               "DISCONNECTED": "Gateway disconnected; turn outcome is unknown.",
               "NOT_SENT": "Gateway disconnected before the request was sent; nothing was sent.",
               "CallStopped": type("CallStopped", (SUP["GatewayClientError"],), {}),
               "lost_before": lambda frame, epoch: False})
    ns["logger"] = _SilentLogger()
    src = open(GA_SRC, encoding="utf-8").read()
    seed_constants(src, ns, GA_SRC)
    for node in class_nodes(src, ["_CompressorView", "_Turn", "GatewayAgent"]):
        exec_node(src, node, ns, GA_SRC)
    seed_constants(src, ns, GA_SRC)
    ns["logger"] = _SilentLogger()
    cls = ns.get("GatewayAgent")
    if cls is None:
        print("FATAL oracle_gateway_agent_flow2: GatewayAgent missing")
        sys.exit(2)
    if "_SERVER_REQUESTS" not in ns:
        print("FATAL oracle_gateway_agent_flow2: _SERVER_REQUESTS missing")
        sys.exit(2)
    missing = [m for m in FUNCS if not hasattr(cls, m)]
    if missing:
        print(f"FATAL oracle_gateway_agent_flow2: class missing {missing}")
        sys.exit(2)
    return ns


FUNCS = [
    "tool_progress_mode", "pin_reasoning", "stage_turn_reasoning",
    "restore_reasoning", "reasoning_callback",
    "_close_segment", "_on_delta", "_on_interim",
    "_on_tool_generating", "_on_tool_progress", "_on_tool_start", "_on_tool_complete",
    "_todo_from_result", "_set_todos", "_on_todo",
    "_on_control_update", "_on_thinking", "_on_reasoning", "_on_reasoning_available",
    "_on_notice", "_on_notice_clear", "_on_reaction",
    "_on_connection_request", "_on_connection_update", "_restore_connection_card",
    "connection_respond", "connection_retry",
    "_on_request_withdrawn", "_ask_clarify", "_ask_sudo", "_ask_vault_unlock",
    "_ask_vault_save_login", "_ask_vault_code", "_answer_request",
    "_voice_stopped", "_refresh_stats",
]
FUNC_SET = set(FUNCS)

NS_GA = extract_ga()
RESULT_DEFAULTS = NS_GA["_RESULT_DEFAULTS"]
COUNTER_VALUES = list(NS_GA["_COUNTER_FROM_USAGE"].values())


# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
        if isinstance(x, AgentNoticeBox):
            return conv(x.d)
        if is_dataclass(x) and not isinstance(x, type):
            return conv(asdict(x))
        if isinstance(x, TurnBox):
            return conv(proj_turn(x.t))
        if isinstance(x, _LostClosure):
            return {"__callable__": "_lost", "turn": x.turn.session_id}
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, (set, frozenset)):
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
    "FileNotFoundError": FileNotFoundError, "ValueError": ValueError, "TypeError": TypeError,
    "RuntimeError": RuntimeError, "KeyError": KeyError, "IndexError": IndexError,
    "AttributeError": AttributeError, "NameError": NameError, "ImportError": ImportError,
    "StopIteration": StopIteration, "AssertionError": AssertionError, "Exception": Exception,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "queue.Empty": __import__("queue").Empty,
    "GatewayClientError": SUP["GatewayClientError"],
    "GatewayRPCError": SUP["GatewayRPCError"],
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    if kind == "GatewayRPCError":
        return SUP["GatewayRPCError"](spec.get("message", ""), code=spec.get("code"),
                                      data=spec.get("data"))
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
    if isinstance(exc, SUP["GatewayRPCError"]):
        d["code"] = exc.code
        d["data"] = exc.data
    if isinstance(exc, OSError) and exc.errno is not None:
        d["errno"] = exc.errno
    return d


# ── per-case script backend (mirrors the C ga2_call exactly) ──
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
        self.trace.append({"name": name, "args": canon(args), "ret": canon(ret)})
        # C's universal seam returns a FRESH copy; copy so flow mutations
        # (answer.clear()) never touch the fixture io.
        return json.loads(json.dumps(ret, ensure_ascii=False, default=str))


def rec(ctx, name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


# ── attribute / turn doubles ──
class AgentNoticeBox:
    def __init__(self, d):
        self.d = d


class TurnBox:
    def __init__(self, t):
        self.t = t


class _LostClosure:
    def __init__(self, turn):
        self.turn = turn


def make_turn(d):
    if d is None:
        return None
    t = NS_GA["_Turn"](d.get("session_id"))
    t.admission = d.get("admission")
    t._shown = list(d.get("_shown") or [])
    t._spoken = list(d.get("_spoken") or [])
    t.seen = set(d.get("seen") or [])
    t.answered = set(d.get("answered") or [])
    t.gapped = bool(d.get("gapped"))
    return t


def proj_turn(t):
    if t is None:
        return None
    return {"session_id": t.session_id, "admission": t.admission,
            "_shown": list(t._shown), "_spoken": list(t._spoken),
            "seen": sorted(t.seen), "answered": sorted(t.answered),
            "gapped": t.gapped}


class StateDouble:
    """Borrowed dict-backed view of self._state(); apply_usage is a seam."""
    def __init__(self, d, ctx):
        object.__setattr__(self, "_d", d)
        object.__setattr__(self, "_ctx", ctx)

    def __getattr__(self, k):
        d = object.__getattribute__(self, "_d")
        if k not in d:
            raise AttributeError(k)
        return d[k]

    def __setattr__(self, k, v):
        object.__getattribute__(self, "_d")[k] = v

    def apply_usage(self, stats):
        return object.__getattribute__(self, "_ctx").call("state.apply_usage", {"stats": stats})


class ConnDouble:
    def __init__(self, ctx, epoch):
        object.__setattr__(self, "_ctx", ctx)
        object.__setattr__(self, "epoch", epoch)

    def call(self, method, **kw):
        return object.__getattribute__(self, "_ctx").call(
            "connection.call", {"method": method, **kw})

    def reply(self, rid, result):
        return object.__getattribute__(self, "_ctx").call(
            "connection.reply", {"id": rid, "result": result})


class TodoDouble:
    def __init__(self, ctx):
        object.__setattr__(self, "_ctx", ctx)

    def restore(self, todos, revision=None):
        return object.__getattribute__(self, "_ctx").call(
            "todo_store.restore", {"todos": todos, "revision": revision})


# callback attribute keys -> seam-name markers in state
CALLBACK_KEYS = {
    "stream_delta_callback", "interim_assistant_callback", "tool_gen_callback",
    "tool_progress_callback", "tool_complete_callback", "thinking_callback",
    "_reasoning_callback", "notice_callback", "notice_clear_callback",
    "reaction_callback", "control_update_callback", "connection_callback",
    "connection_update_callback", "modal_withdraw_callback", "clarify_callback",
}
# args_fn per callback seam (uniform tool_progress_callback carries **kw)
CB_ARGS = {
    "stream_delta_callback": lambda text: {"text": text},
    "interim_assistant_callback": lambda text, already_streamed=False: {
        "text": text, "already_streamed": already_streamed},
    "tool_gen_callback": lambda name: {"name": name},
    "tool_progress_callback": lambda kind, name, message, args, **kw: {
        "kind": kind, "name": name, "message": message, "args": args, **kw},
    "tool_complete_callback": lambda call_id, name, args, result: {
        "call_id": call_id, "name": name, "args": args, "result": result},
    "thinking_callback": lambda text: {"text": text},
    "_reasoning_callback": lambda text: {"text": text},
    "notice_callback": lambda notice: {"notice": canon(notice)},
    "notice_clear_callback": lambda key: {"key": key},
    "reaction_callback": lambda kind: {"kind": kind},
    "control_update_callback": lambda: {},
    "connection_callback": lambda payload: {"payload": payload},
    "connection_update_callback": lambda payload: {"payload": payload},
    "modal_withdraw_callback": lambda method: {"method": method},
    "clarify_callback": lambda questions: {"questions": questions},
}


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


CTX_HOLDER = {"ctx": None}


def cb_build(name):
    """A scripted callback getter returns a seam-name string -> a sink."""
    def build(raw):
        if isinstance(raw, str) and raw:
            return rec(CTX_HOLDER["ctx"], raw, args_fn=lambda *a, **k: {"args": list(a), "kw": k})
        return None
    return build


# ── per-case setup ──
def setup(args, state, ctx):
    self = object.__new__(NS_GA["GatewayAgent"])
    CTX_HOLDER["ctx"] = ctx
    cb_markers = {}
    # plain attributes from state (underscored real names)
    skip = {"connection", "_todos", "_turn"}
    for k, v in state.items():
        if k in skip:
            continue
        if k in CALLBACK_KEYS:
            cb_markers[k] = v
            if isinstance(v, str) and v:
                setattr(self, k, rec(ctx, v, args_fn=CB_ARGS.get(k, (lambda *a, **kw: {}))))
            else:
                setattr(self, k, None)
        else:
            setattr(self, k, v)
    # defaults for attributes the flows read but fixtures may omit
    for k, dflt in (("_session_id", None), ("_tool_progress_mode", "all"),
                    ("_reasoning_config", None), ("_turn_reasoning", None),
                    ("_show_reasoning_sent", None), ("_service_tier", None),
                    ("suppress_status_output", False), ("_usage_stale", False),
                    ("_open_request", None), ("_generation", None),
                    ("session_api_calls", 0), ("session_input_tokens", 0),
                    ("session_output_tokens", 0), ("_voice_turn_pending", False)):
        if not hasattr(self, k):
            setattr(self, k, dflt)
    self.connection = ConnDouble(ctx, state.get("connection_epoch", 0))
    self._todos = TodoDouble(ctx)
    self._turn = make_turn(state.get("_turn"))

    def _state():
        node = state.setdefault("state", {})
        ctx.trace.append({"name": "self._state", "args": {}, "ret": canon(node)})
        return StateDouble(node, ctx)
    self._state = _state

    # claimed siblings + unported collaborators: instance recorders shadow
    # the live class methods (the 36 ported methods stay live and compose).
    self._set_reasoning = rec(ctx, "self._set_reasoning",
                              args_fn=lambda config, explicit: {"config": config, "explicit": explicit})
    self._sync_show_reasoning = rec(ctx, "self._sync_show_reasoning", args_fn=lambda: {})
    self._emit_print = rec(ctx, "self._emit_print", args_fn=lambda text: {"text": text})
    self._give_back_turn_route = rec(ctx, "self._give_back_turn_route", args_fn=lambda: {})
    self._owns = rec(ctx, "self._owns", args_fn=lambda turn: {"turn": proj_turn(turn)})
    self._lost = lambda turn: _LostClosure(turn)   # pure closure factory (projected)
    self._ask_secret = rec(ctx, "self._ask_secret", args_fn=lambda params: {"params": params})
    # _SERVER_REQUESTS holds the CLASS function for "secret" (unbound), so the
    # instance recorder above cannot shadow it; redirect the table entry to the
    # seam for this case (the heavy skills_tool/callbacks chain is never imported).
    NS_GA["_SERVER_REQUESTS"]["secret"] = (
        lambda slf, params, _ctx=ctx: _ctx.call("self._ask_secret", {"params": params}))

    # claimed module functions -> module-global recorders
    NS_GA["_effort_word"] = rec(ctx, "_effort_word", args_fn=lambda config: {"config": config})
    NS_GA["clarify_reply"] = rec(ctx, "clarify_reply",
                                 args_fn=lambda callback, params: {
                                     "callback_present": callback is not None, "params": params})
    NS_GA["logger"] = _SilentLogger()
    NS_GA["time"] = types.SimpleNamespace(
        monotonic=rec(ctx, "time.monotonic", args_fn=lambda: {}))

    def diff_printer_build(raw):
        name = raw if isinstance(raw, str) and raw else "diff_emit"
        return lambda line: ctx.call(name, {"line": line})

    stub_modules({
        "agent.i18n": {"t": rec(ctx, "t", args_fn=lambda key, **kw: {"key": key, **kw})},
        "agent.display": {"build_tool_preview": rec(ctx, "build_tool_preview",
                                                    args_fn=lambda name, args: {"name": name, "args": args})},
        "hermes_cli.gateway_tools": {"mark_dirty": rec(ctx, "mark_dirty", args_fn=lambda: {})},
        "hermes_cli.cli_modern_layout": {"diff_printer": rec(ctx, "diff_printer",
                                                               args_fn=lambda printer: {"printer": {"__callable__": "_cprint"}},
                                                               build=diff_printer_build)},
        "cli": {"_cprint": lambda *a, **k: None},
        "tools.terminal_tool": {"_get_sudo_password_callback": rec(
            ctx, "_get_sudo_password_callback", args_fn=lambda: {},
            build=lambda raw: (rec(ctx, raw, args_fn=lambda: {}) if isinstance(raw, str) and raw else None))},
        "agent.vault_backends.unlock": {
            "get_unlock_prompt_callback": rec(ctx, "get_unlock_prompt_callback", args_fn=lambda: {},
                                              build=lambda raw: (rec(ctx, raw, args_fn=lambda a, b: {"backend": a, "display_name": b}) if isinstance(raw, str) and raw else None)),
            "get_save_login_prompt_callback": rec(ctx, "get_save_login_prompt_callback", args_fn=lambda: {},
                                                  build=lambda raw: (rec(ctx, raw, args_fn=lambda a, b: {"origin": a, "site": b}) if isinstance(raw, str) and raw else None)),
            "get_code_prompt_callback": rec(ctx, "get_code_prompt_callback", args_fn=lambda: {},
                                            build=lambda raw: (rec(ctx, raw, args_fn=lambda a, b: {"site": a, "hint": b}) if isinstance(raw, str) and raw else None)),
        },
    })
    return self, cb_markers


SEAMED_ATTRS = {"_set_reasoning", "_sync_show_reasoning", "_emit_print",
                "_give_back_turn_route", "_owns", "_lost", "_ask_secret", "_state"}


def proj_state(self, state, cb_markers):
    out = {}
    for k, v in vars(self).items():
        if k in ("connection", "_todos", "_state_double") or k in SEAMED_ATTRS:
            continue
        if k == "_turn":
            out[k] = proj_turn(v)
        elif k in CALLBACK_KEYS:
            if isinstance(v, str) or v is None:
                out[k] = v
            else:
                out[k] = cb_markers.get(k)
        elif k == "_open_request":
            out[k] = list(v) if isinstance(v, tuple) else v
        else:
            out[k] = v
    # preserve state keys not present as instance attrs (e.g. connection_epoch, state)
    for k, v in state.items():
        if k not in out and k not in ("_turn",):
            out[k] = v
    return out


# ── per-func invocation ──
def invoke(func, args, self, ctx):
    if func == "tool_progress_mode":
        self.tool_progress_mode = args.get("mode")
        return None
    if func == "reasoning_callback":
        self.reasoning_callback = args.get("callback")
        return None
    if func == "pin_reasoning":
        return self.pin_reasoning(args.get("config"))
    if func == "stage_turn_reasoning":
        return self.stage_turn_reasoning(args.get("config"))
    if func == "restore_reasoning":
        return self.restore_reasoning(args.get("config"))
    if func == "_close_segment":
        return self._close_segment(args.get("starts") or {}, separator=args.get("separator", False))
    if func == "_todo_from_result":
        return self._todo_from_result(args.get("result"))
    if func == "_set_todos":
        return self._set_todos(args.get("snapshot") or {})
    if func == "_restore_connection_card":
        return self._restore_connection_card(args.get("pending"))
    if func == "connection_respond":
        return self.connection_respond(args.get("op_id"), args.get("result"))
    if func == "connection_retry":
        return self.connection_retry(args.get("name"))
    if func == "_on_request_withdrawn":
        return self._on_request_withdrawn(args.get("payload") or {})
    if func in ("_ask_clarify", "_ask_sudo", "_ask_vault_unlock",
                "_ask_vault_save_login", "_ask_vault_code"):
        return getattr(self, func)(args.get("params") or {})
    if func == "_answer_request":
        return self._answer_request(args.get("frame") or {})
    if func == "_voice_stopped":
        return self._voice_stopped(args.get("history") or [])
    if func == "_refresh_stats":
        return self._refresh_stats(turn=make_turn(args.get("turn")))
    # event handlers: (payload, stream_callback, starts)
    sc = args.get("stream_callback")
    stream_cb = rec(ctx, sc, args_fn=lambda text: {"text": text}) if isinstance(sc, str) and sc else None
    starts = args.get("starts")
    if starts is None:
        starts = {}
        args["starts"] = starts
    return getattr(self, func)(args.get("payload") or {}, stream_cb, starts)


# ── per-case drive ──
def drive(func, inp):
    args = json.loads(json.dumps(inp.get("args") or {}, ensure_ascii=False, default=str))
    state = canon(inp.get("state") or {})
    ctx = Ctx(inp.get("io"))
    self, cb_markers = setup(args, state, ctx)
    try:
        ret = invoke(func, args, self, ctx)
    except BaseException as exc:   # SystemExit included
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace),
            "state": canon(proj_state(self, state, cb_markers)),
            "args": canon(args)}


# ── three-way seam-name cross-check (C port ↔ oracle stubs ↔ fixture io) ──
SEAM_NAMES = [
    "connection.call", "connection.reply",
    "self._set_reasoning", "_effort_word", "self._sync_show_reasoning",
    "self._emit_print", "self._owns", "self._state", "state.apply_usage",
    "self._give_back_turn_route", "t", "time.monotonic", "build_tool_preview",
    "mark_dirty", "diff_printer", "todo_store.restore",
    "_get_sudo_password_callback", "get_unlock_prompt_callback",
    "get_save_login_prompt_callback", "get_code_prompt_callback",
    "clarify_reply", "self._ask_secret",
    # dynamic callback seams (fixture-chosen marker strings; passed through
    # as strings in C, not literals)
    "stream_delta_callback", "stream_callback", "interim_assistant_callback",
    "tool_gen_callback", "tool_progress_callback", "tool_complete_callback",
    "thinking_callback", "_reasoning_callback", "notice_callback",
    "notice_clear_callback", "reaction_callback", "control_update_callback",
    "connection_callback", "connection_update_callback", "modal_withdraw_callback",
    "clarify_callback", "sudo_password_callback", "vault_unlock_callback",
    "vault_save_login_callback", "vault_code_callback", "diff_emit",
]
DYNAMIC_SEAMS = {
    "stream_delta_callback", "stream_callback", "interim_assistant_callback",
    "tool_gen_callback", "tool_progress_callback", "tool_complete_callback",
    "thinking_callback", "_reasoning_callback", "notice_callback",
    "notice_clear_callback", "reaction_callback", "control_update_callback",
    "connection_callback", "connection_update_callback", "modal_withdraw_callback",
    "clarify_callback", "sudo_password_callback", "vault_unlock_callback",
    "vault_save_login_callback", "vault_code_callback", "diff_emit",
}


def cross_check():
    csrc = open(_REPO / "src" / "cli" / "gateway_agent_flow2.c", encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES
                 if n not in DYNAMIC_SEAMS and f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_gateway_agent_flow2: seams absent from C port: {missing_c}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'(?:rec|arec)\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'ctx\.call\(\s*"([^"]+)"', osrc))
    used |= {"self._state"}
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_gateway_agent_flow2: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "gateway_agent_flow2.jsonl"
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
            print(f"FATAL oracle_gateway_agent_flow2: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        badf = fix_funcs - FUNC_SET
        if badf:
            print(f"FATAL oracle_gateway_agent_flow2: fixture funcs outside port set: {sorted(badf)}")
            sys.exit(2)
        missingf = FUNC_SET - fix_funcs
        if missingf:
            print(f"FATAL oracle_gateway_agent_flow2: ported funcs without fixtures: {sorted(missingf)}")
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
    for key in ("state", "args"):
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
    print(f"ORACLE gateway_agent_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


if __name__ == "__main__":
    main()