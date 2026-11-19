#!/usr/bin/env python3
"""Oracle for src/gateway/relay_protocol_flow.c — drives the LIVE
gateway/relay/ws_transport.py UNCLAIMED-remainder FLOW defs (22 functions)
under IDENTICAL scripted universal seams.

The task names gateway/relay/protocol.py, which does not exist: transport.py
is the pure Protocol surface (method bodies are `...`) and ws_transport.py is
the REAL defining module of the relay wire protocol. PoP credit goes there.

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"], and compares the ordered trace, the return
(or raise spec), and the state window with an ORDERED canon (dict key order is
part of the wire-frame contract). Live defs are AST-extracted: module leaves
exec into the namespace; WebSocketRelayTransport methods re-host in a synthetic
class; _PendingRequest.__init__ re-hosts with a BaseDouble supplying the seamed
super().__init__. Claimed siblings / foreign I/O (_ws_dial_url, _event_from_wire,
_passthrough_from_wire, _dial_and_start, _reconnect_loop, _send_inbound_ack,
_close_code_of, clean_attachment_filename, CapabilityDescriptor.from_json,
json.dumps, asyncio.Event/create_task/wait_for, relay_callback, logger) are
recording seams fed the SAME scripts the C harness feeds. Ported siblings stay
REAL so composition is exercised (_media_types/_media_filenames ->
_per_url_from_wire; _redial_with_fresh_token -> _latch_if_fresh_token_refused ->
_close_reason_of; _fail_pending -> _settle_connection_lost via __self__).

Fail-closed: an absent / exhausted script RETURNS a RuntimeError raise spec
("no script for <name>"), recorded as the event ret before propagation; 0 cases
=> exit 1. A scripted BaseException (SystemExit / KeyboardInterrupt /
GeneratorExit / CancelledError) propagates past every `except Exception` guard
exactly as live (_await_bounded's except tuple ALSO catches CancelledError —
only the trio passes there). Per-case BaseException catch -> __raise__ spec;
scripted raises round-trip BARE via identity match against Script.handed. The
state window is projected from carriers materialized BEFORE the call, so a
raise leaves the mutation history at that point (matching the C in-place st).

Value model: future doubles {"__future__":true,id,done,result,exception,
cancelled,op?}; task doubles {"__task__":true,id,done,cancelled,exception};
event doubles {"__event__":true,id,set}; callable doubles {"__callable__":seam}
fire that seam with {args,kwargs}; relay_callback is a silent CM (the call is
the seam boundary). exc inputs/args are raise specs whose extra keys are
materialized as exception attributes (rcvd/sent/reason/code) and projected back
via vars(exc). descriptor results are SimpleNamespace doubles. tuple/set ->
array; integral floats -> ints; _inbound_acks -> sorted id list.

Domain (ASCII scope, documented in the port header): str() operands ASCII
scalars; raw/frame/descriptor maps dict-or-falsy; media_urls/media/identities
list-or-falsy; platform dict-key positions str-or-falsy; from_json results
carry a platform key (absent -> SimpleNamespace AttributeError edge);
create_task ack ids are fresh vs _inbound_acks. EXCLUDED: integral-FLOAT
literals in str()/type-name positions; KeyError raise specs in exc-arg
positions (str(KeyError) is repr-shaped); non-str truthy platform keys;
non-list truthy identities.

Summary: 'ORACLE relay_protocol_flow: N cases, M mismatches'.
"""
import __future__
import ast
import asyncio
import builtins
import copy
import json
import sys
import types
from pathlib import Path
from types import SimpleNamespace
from typing import Any, Optional, Callable, Awaitable, Dict, List

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (AttributeError, ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "gateway" / "relay" / "ws_transport.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "gateway" / "relay" / "ws_transport.py")
NAME = "relay_protocol_flow"

PORTED = [
    "_per_url_from_wire", "_media_types_from_wire", "_media_filenames_from_wire",
    "_await_bounded", "_PendingRequest.__init__", "__init__",
    "_fail_pending", "_settle_connection_lost", "_dialer_running",
    "_close_reason_of", "_latch_if_fresh_token_refused", "hold_redial",
    "release_redial", "_await_redial_hold", "_redial_with_fresh_token",
    "_on_descriptor", "_on_inbound", "_ack_admitted", "_on_going_idle_ack",
    "_on_outbound_result", "_on_interrupt_inbound", "_on_passthrough_forward",
]
MOD_FUNCS = {"_per_url_from_wire", "_media_types_from_wire",
             "_media_filenames_from_wire", "_await_bounded"}
WST_METHODS = [
    "__init__", "_fail_pending", "_settle_connection_lost", "_dialer_running",
    "_close_reason_of", "_redial_with_fresh_token", "_latch_if_fresh_token_refused",
    "hold_redial", "release_redial", "_await_redial_hold", "_on_descriptor",
    "_on_inbound", "_ack_admitted", "_on_going_idle_ack", "_on_outbound_result",
    "_on_interrupt_inbound", "_on_passthrough_forward",
]
ASYNC_FUNCS = {
    "_await_bounded", "_redial_with_fresh_token", "_await_redial_hold",
    "_on_descriptor", "_on_inbound", "_on_going_idle_ack", "_on_outbound_result",
    "_on_interrupt_inbound", "_on_passthrough_forward",
}

# ── AST extraction ──────────────────────────────────────────────────────────
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, path=SRC):
    return compile(ast.Module(body=[node], type_ignores=[]), path, "exec",
                   flags=_FLAGS, dont_inherit=True)


MOD_NODES = {}
WST_NODES = {}
PR_INIT_NODE = None
CONST_NODES = []
_CONST_NAMES = {"_HANDSHAKE_TIMEOUT_S", "_OUTBOUND_TIMEOUT_S",
                "_TEARDOWN_AWAIT_TIMEOUT_S", "_RELAY_UNAUTHORIZED_CLOSE_CODE",
                "_RELAY_EXPIRED_CLOSE_REASON", "REDIAL_HOLD_MAX_S",
                "_AMBIGUOUS_ON_READER_EXIT_OPS"}
for _node in _TREE.body:
    if isinstance(_node, (ast.FunctionDef, ast.AsyncFunctionDef)) and _node.name in MOD_FUNCS:
        MOD_NODES[_node.name] = _node
    elif isinstance(_node, ast.ClassDef) and _node.name == "WebSocketRelayTransport":
        for _sub in _node.body:
            if isinstance(_sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and _sub.name in WST_METHODS:
                WST_NODES[_sub.name] = _sub
    elif isinstance(_node, ast.ClassDef) and _node.name == "_PendingRequest":
        for _sub in _node.body:
            if isinstance(_sub, ast.FunctionDef) and _sub.name == "__init__":
                PR_INIT_NODE = _sub
    elif isinstance(_node, ast.Assign):
        if any(isinstance(t, ast.Name) and t.id in _CONST_NAMES for t in _node.targets):
            CONST_NODES.append(_node)
_missing = (set(MOD_FUNCS) - set(MOD_NODES)) | (set(WST_METHODS) - set(WST_NODES))
if _missing or PR_INIT_NODE is None:
    print(f"ORACLE {NAME}: FATAL missing live defs {_missing}", file=sys.stderr)
    sys.exit(1)


# ── canonical projection ────────────────────────────────────────────────────
class RecDouble:
    """JSON-backed attribute double: state echo reads _d verbatim."""
    def __init__(self, d):
        self._d = dict(d) if isinstance(d, dict) else {}

    def __getattr__(self, k):
        d = object.__getattribute__(self, "_d")
        if k in d:
            return d[k]
        raise AttributeError(k)


def norm_value(v):
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, RecDouble):
        return norm_value(v._d)
    if isinstance(v, SimpleNamespace):
        return {k: norm_value(x) for k, x in vars(v).items() if not callable(x)}
    if isinstance(v, dict):
        return {k: norm_value(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm_value(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm_value(x) for x in v), key=lambda s: str(s))
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def norm_args(v):
    v = norm_value(v)
    if isinstance(v, dict):
        return {k: norm_args(x) for k, x in v.items() if x is not None}
    # arrays/scalars copied VERBATIM (C rp_h_strip_nulls recurses only into
    # object values; array elements keep their null keys / null elements).
    return v


def canon(x):
    if isinstance(x, dict):
        return ("obj", [(k, canon(v)) for k, v in x.items()])
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", x)


def materialize_attr(v):
    if isinstance(v, dict):
        return SimpleNamespace(**{k: materialize_attr(x) for k, x in v.items()})
    if isinstance(v, list):
        return [materialize_attr(x) for x in v]
    return v


def make_exc(marker):
    kind = marker.get("__raise__", "RuntimeError")
    msg = marker.get("message", marker.get("msg", ""))
    base = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
            "GeneratorExit": GeneratorExit}
    builtin = {
        "TypeError": TypeError, "ValueError": ValueError, "RuntimeError": RuntimeError,
        "AttributeError": AttributeError, "KeyError": KeyError, "Exception": Exception,
        "TimeoutError": TimeoutError, "OSError": OSError, "LookupError": LookupError,
        "InterruptedError": InterruptedError, "FileNotFoundError": FileNotFoundError,
        "IndexError": IndexError, "StopIteration": StopIteration,
        "NotImplementedError": NotImplementedError, "AssertionError": AssertionError,
        "NameError": NameError, "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
        "ZeroDivisionError": ZeroDivisionError, "OverflowError": OverflowError,
        "UnicodeError": UnicodeError, "RecursionError": RecursionError, "MemoryError": MemoryError,
        "EOFError": EOFError, "SyntaxError": SyntaxError, "SystemError": SystemError,
        "BufferError": BufferError, "ReferenceError": ReferenceError, "Warning": Warning,
        "ConnectionError": ConnectionError, "BrokenPipeError": BrokenPipeError,
        "PermissionError": PermissionError, "FileExistsError": FileExistsError,
        "BlockingIOError": BlockingIOError, "ChildProcessError": ChildProcessError,
        "ProcessLookupError": ProcessLookupError, "IsADirectoryError": IsADirectoryError,
        "NotADirectoryError": NotADirectoryError, "ConnectionAbortedError": ConnectionAbortedError,
        "ConnectionRefusedError": ConnectionRefusedError, "ConnectionResetError": ConnectionResetError,
        "StopAsyncIteration": StopAsyncIteration, "UnboundLocalError": UnboundLocalError,
        "IndentationError": IndentationError, "TabError": TabError, "FloatingPointError": FloatingPointError,
        "ExceptionGroup": ExceptionGroup, "BaseExceptionGroup": BaseExceptionGroup,
    }
    if kind in base:
        cls = base[kind]
    elif kind in builtin:
        cls = builtin[kind]
    elif kind == "CancelledError":
        cls = asyncio.CancelledError
    else:
        cls = type(kind, (Exception,), {})
    if kind == "KeyError":
        try:
            e = cls(ast.literal_eval(msg))
        except (ValueError, SyntaxError):
            e = cls(msg)
    else:
        e = cls(msg)
    for k, v in marker.items():
        if k not in ("__raise__", "message", "msg"):
            setattr(e, k, materialize_attr(v))
    return e


def proj_exc(e):
    d = {"__raise__": type(e).__name__, "message": str(e)}
    for k, v in vars(e).items():
        d[k] = norm_value(v)
    return d


def exc_from_input(j):
    if isinstance(j, dict) and "__raise__" in j:
        return make_exc(j)
    return j


# ── scripted seams (fail-closed) ────────────────────────────────────────────
_MISSING = object()
CURRENT = {"ctx": None}


class Raise:
    def __init__(self, spec, exc):
        self.spec = spec
        self.exc = exc


class Script:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.handed = []

    def next(self, name):
        entry = self.io.get(name, _MISSING)
        if entry is _MISSING:
            return self._fail(name)
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                return self._fail(name)
            self.cursors[name] = idx + 1
            val = entry[idx]
        else:
            val = entry
        return self._unwrap(val)

    def _fail(self, name):
        spec = {"__raise__": "RuntimeError", "message": "no script for " + name}
        r = Raise(spec, make_exc(spec))
        self.handed.append(r)
        return r

    def _unwrap(self, val):
        if isinstance(val, dict) and "__raise__" in val:
            r = Raise(val, make_exc(val))
            self.handed.append(r)
            return r
        if isinstance(val, dict) and val.get("__script__") is True:
            return val.get("ret")
        return val


def proj_cb(cb):
    nm = getattr(cb, "__seam_name__", None) or getattr(cb, "__name__", None) or type(cb).__name__
    return {"__callable__": nm}


class EventWait:
    def __init__(self, id):
        self.id = id


def proj_awaitable(aw):
    if isinstance(aw, EventWait):
        return {"__event_wait__": aw.id}
    if isinstance(aw, SeamAwaitable):
        return {"__callable__": aw.__seam_name__}
    return norm_value(aw)


def proj(name, a, k):
    if name == "_ws_dial_url":
        return {"url": a[0] if a else None}
    if name == "clean_attachment_filename":
        return {"n": a[0] if a else None}
    if name == "json.dumps":
        return {"obj": norm_value(a[0]) if a else None}
    if name == "CapabilityDescriptor.from_json":
        return {"s": a[0] if a else None}
    if name in ("_event_from_wire", "_passthrough_from_wire"):
        return {"raw": norm_value(a[0]) if a else None}
    if name == "_send_inbound_ack":
        return {"buffer_id": a[0] if a else None}
    if name in ("_dial_and_start", "_reconnect_loop"):
        return {}
    if name == "_close_code_of":
        x = a[0] if a else None
        return {"exc": proj_exc(x) if isinstance(x, BaseException) else norm_value(x)}
    if name == "super.__init__":
        return {"loop": k.get("loop")}
    return {"args": [norm_value(x) for x in a],
            "kwargs": {kk: norm_value(vv) for kk, vv in k.items()}}


def record(name, argsdict):
    ctx = CURRENT["ctx"]
    args = norm_args(argsdict)
    raw = ctx.script.next(name)
    if isinstance(raw, Raise):
        ctx.trace.append({"ev": name, "args": args, "ret": raw.spec})
        raise raw.exc
    ctx.trace.append({"ev": name, "args": args, "ret": norm_value(raw)})
    return materialize_result(name, raw)


class SeamFn:
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **k):
        return record(self.name, proj(self.name, a, k))


class SeamAwaitable:
    """Async sibling seam result: records at AWAIT time (coroutine creation is
    inert, so create_task(self._reconnect_loop()) fires only create_task)."""
    def __init__(self, name, a, k):
        self.__seam_name__ = name
        self.a = a
        self.k = k

    def __await__(self):
        if False:
            yield
        return record(self.__seam_name__, proj(self.__seam_name__, self.a, self.k))


class AsyncSeamFn:
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **k):
        return SeamAwaitable(self.name, a, k)


class LoggerDouble:
    def _log(self, level, msg, args, kw):
        d = {"msg": msg,
             "args": [str(x) if isinstance(x, BaseException) else norm_value(x) for x in args]}
        ei = kw.get("exc_info")
        if ei is True:
            d["exc_info"] = True
        elif isinstance(ei, BaseException):
            d["exc_info"] = proj_exc(ei)
        elif ei:
            d["exc_info"] = norm_value(ei)
        return record("logger." + level, d)

    def debug(self, msg, *a, **k):
        return self._log("debug", msg, a, k)

    def info(self, msg, *a, **k):
        return self._log("info", msg, a, k)

    def warning(self, msg, *a, **k):
        return self._log("warning", msg, a, k)

    def error(self, msg, *a, **k):
        return self._log("error", msg, a, k)

    def exception(self, msg, *a, **k):
        return self._log("exception", msg, a, k)

    def log(self, level, msg, *a, **k):
        return self._log(str(level), msg, a, k)


# ── asyncio / future / task / event doubles ─────────────────────────────────
class FutureDouble(RecDouble):
    def done(self):
        return bool(self._d.get("done", False))

    def cancelled(self):
        return bool(self._d.get("cancelled", False))

    def exception(self):
        e = self._d.get("exception")
        return make_exc(e) if isinstance(e, dict) and "__raise__" in e else None

    def set_result(self, v):
        record("future.set_result", {"id": self._d.get("id"), "result": norm_value(v)})
        self._d["done"] = True
        self._d["result"] = v

    def add_done_callback(self, cb):
        record("future.add_done_callback", {"id": self._d.get("id"), "cb": proj_cb(cb)})


class TaskDouble(RecDouble):
    def done(self):
        return bool(self._d.get("done", False))

    def cancelled(self):
        return bool(self._d.get("cancelled", False))

    def exception(self):
        e = self._d.get("exception")
        return make_exc(e) if isinstance(e, dict) and "__raise__" in e else None

    def add_done_callback(self, cb):
        record("task.add_done_callback", {"id": self._d.get("id"), "cb": proj_cb(cb)})


class EventDouble(RecDouble):
    def set(self):
        record("event.set", {"id": self._d.get("id")})
        self._d["set"] = True

    def clear(self):
        record("event.clear", {"id": self._d.get("id")})
        self._d["set"] = False

    def wait(self):
        record("event.wait", {"id": self._d.get("id")})
        return EventWait(self._d.get("id"))


class AsyncioDouble:
    CancelledError = asyncio.CancelledError

    def Event(self):
        r = record("asyncio.Event", {})
        if isinstance(r, EventDouble):
            return r
        return EventDouble(r if isinstance(r, dict) else {})

    def create_task(self, coro, name=None):
        target = getattr(coro, "__seam_name__", None) or type(coro).__name__
        k = {"target": target}
        if name is not None:
            k["name"] = name
        r = record("asyncio.create_task", k)
        if isinstance(r, TaskDouble):
            return r
        return TaskDouble(r if isinstance(r, dict) else {})

    async def wait_for(self, aw, timeout=None):
        return record("asyncio.wait_for", {"aw": proj_awaitable(aw), "timeout": timeout})

    def get_running_loop(self):
        return record("asyncio.get_running_loop", {})


def mk_async_callable(name):
    async def f(*a, **k):
        return record(name, {"args": list(a), "kwargs": k})
    f.__seam_name__ = name
    return f


def mk_sync_callable(name):
    def f(*a, **k):
        return record(name, {"args": list(a), "kwargs": k})
    f.__seam_name__ = name
    return f


def materialize_result(name, raw):
    if raw is None:
        return None
    if isinstance(raw, dict):
        if "__callable__" in raw:
            return mk_async_callable(raw["__callable__"])
        if raw.get("__future__"):
            return FutureDouble(raw)
        if raw.get("__task__"):
            return TaskDouble(raw)
        if raw.get("__event__"):
            return EventDouble(raw)
        if name == "CapabilityDescriptor.from_json":
            return SimpleNamespace(**raw)
    return raw


# ── live namespace + synthetic classes ──────────────────────────────────────
_MOD = types.ModuleType("rp_live")
sys.modules["rp_live"] = _MOD
NS = _MOD.__dict__
NS.update({
    "__name__": "rp_live", "__builtins__": builtins,
    "Any": Any, "Optional": Optional, "Callable": Callable, "Awaitable": Awaitable,
    "Dict": Dict, "List": List,
    "logger": LoggerDouble(),
    "asyncio": AsyncioDouble(),
    "json": SimpleNamespace(dumps=SeamFn("json.dumps")),
    "_ws_dial_url": SeamFn("_ws_dial_url"),
    "_event_from_wire": SeamFn("_event_from_wire"),
    "_passthrough_from_wire": SeamFn("_passthrough_from_wire"),
    "clean_attachment_filename": SeamFn("clean_attachment_filename"),
    "CapabilityDescriptor": SimpleNamespace(from_json=SeamFn("CapabilityDescriptor.from_json")),
    "WEBSOCKETS_AVAILABLE": True,
})
for _node in CONST_NODES:
    exec(_compile_node(_node), NS)
for _nm, _node in MOD_NODES.items():
    exec(_compile_node(_node), NS)


class BaseDouble:
    pass


BaseDouble.__init__ = SeamFn("super.__init__")
NS["BaseDouble"] = BaseDouble

_syn = ast.ClassDef(name="WSTDouble", bases=[], keywords=[],
                    body=[WST_NODES[n] for n in WST_METHODS], decorator_list=[])
ast.fix_missing_locations(_syn)
exec(_compile_node(_syn), NS)
WSTDouble = NS["WSTDouble"]

_syn2 = ast.ClassDef(name="PRDouble", bases=[ast.Name(id="BaseDouble", ctx=ast.Load())],
                     keywords=[], body=[PR_INIT_NODE], decorator_list=[])
ast.fix_missing_locations(_syn2)
exec(_compile_node(_syn2), NS)
PRDouble = NS["PRDouble"]

# in-function import: from gateway.session_ingress_context import relay_callback
# (stub EVERY intermediate package; a None entry would halt the import).
_pkg = types.ModuleType("gateway")
_pkg.__path__ = []
_sic = types.ModuleType("gateway.session_ingress_context")


def _relay_callback_stub(transport, event):
    record("relay_callback", {"transport": {"__transport__": True}, "event": norm_value(event)})

    class _CM:
        def __enter__(self):
            return None

        def __exit__(self, *a):
            return False
    return _CM()


_sic.relay_callback = _relay_callback_stub
_pkg.session_ingress_context = _sic
sys.modules["gateway"] = _pkg
sys.modules["gateway.session_ingress_context"] = _sic


# ── per-case state defaults / mutation window ───────────────────────────────
def _ev0():
    return {"__event__": True, "id": "ev0", "set": False}


DEFAULTS = {
    "_fail_pending": {"_pending": {}},
    "_on_outbound_result": {"_pending": {}},
    "_on_descriptor": {"_descriptors_by_platform": {}, "_descriptor": None,
                       "_descriptor_ready": None, "_handshake_succeeded": False},
    "_dialer_running": {"_supervisor": None},
    "_latch_if_fresh_token_refused": {"_auth_retry_pending": False, "_auth_revoked": False},
    "hold_redial": {"_redial_held": False, "_redial_release": _ev0()},
    "release_redial": {"_redial_held": False, "_redial_release": _ev0()},
    "_await_redial_hold": {"_redial_held": False, "_redial_release": _ev0(),
                           "_redial_hold_max_s": 60.0},
    "_redial_with_fresh_token": {"_closing": False, "_reconnect": False, "_supervisor": None,
                                 "_auth_retry_pending": False, "_auth_revoked": False},
    "_on_inbound": {"_inbound": None},
    "_ack_admitted": {"_inbound_acks": []},
    "_on_going_idle_ack": {"_going_idle_ack": None},
    "_on_interrupt_inbound": {"_interrupt_inbound_handler": None},
    "_on_passthrough_forward": {"_passthrough_handler": None},
}
MUT = {
    "_fail_pending": ["_pending"],
    "_settle_connection_lost": ["fut"],
    "_latch_if_fresh_token_refused": ["_auth_retry_pending", "_auth_revoked"],
    "hold_redial": ["_redial_held", "_redial_release"],
    "release_redial": ["_redial_held", "_redial_release"],
    "_await_redial_hold": ["_redial_held", "_redial_release", "_redial_hold_max_s"],
    "_redial_with_fresh_token": ["_closing", "_reconnect", "_supervisor",
                                 "_auth_retry_pending", "_auth_revoked"],
    "_on_descriptor": ["_descriptor", "_descriptors_by_platform",
                       "_handshake_succeeded", "_descriptor_ready"],
    "_ack_admitted": ["_inbound_acks"],
    "_on_going_idle_ack": ["_going_idle_ack"],
    "_on_outbound_result": ["_pending"],
}


def materialize_state(k, v):
    if k == "_inbound_acks":
        return {TaskDouble({"__task__": True, "id": i, "done": False,
                            "cancelled": False, "exception": None})
                for i in (v or [])}
    if k == "_pending":
        return {kk: (FutureDouble(vv) if isinstance(vv, dict) else vv)
                for kk, vv in (v or {}).items()}
    if isinstance(v, dict):
        if v.get("__future__"):
            return FutureDouble(v)
        if v.get("__task__"):
            return TaskDouble(v)
        if v.get("__event__"):
            return EventDouble(v)
        if "__callable__" in v:
            return mk_async_callable(v["__callable__"])
    return copy.deepcopy(v)


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.script = Script(inp.get("io"))


def make_self(inp, func):
    st_in = inp.get("state") or {}
    slf = WSTDouble.__new__(WSTDouble)
    eff = dict(st_in)
    for k, v in DEFAULTS.get(func, {}).items():
        if k not in eff:
            eff[k] = copy.deepcopy(v)
    for k, v in eff.items():
        setattr(slf, k, materialize_state(k, v))
    # claimed siblings (instance attributes shadow the synthetic class)
    slf._dial_and_start = AsyncSeamFn("_dial_and_start")
    slf._reconnect_loop = AsyncSeamFn("_reconnect_loop")
    slf._send_inbound_ack = AsyncSeamFn("_send_inbound_ack")
    slf._close_code_of = SeamFn("_close_code_of")
    return slf


def drive(func, inp, holder):
    g = inp.get
    if func == "__init__":
        NS["WEBSOCKETS_AVAILABLE"] = bool(inp.get("websockets_available", True))
        slf = WSTDouble.__new__(WSTDouble)
        holder["proj"] = lambda: norm_value(vars(slf))
        _url = g("url"); _platform = g("platform"); _bot_id = g("bot_id")
        kw = {}
        for key in ("identities", "connect_timeout_s", "outbound_timeout_s",
                    "gateway_id", "upgrade_secret", "reconnect",
                    "reconnect_backoff_s", "reconnect_max_backoff_s"):
            if key in inp:
                kw[key] = g(key)
        return WSTDouble.__init__(slf, _url, _platform, _bot_id, **kw)
    if func == "_PendingRequest.__init__":
        slf = PRDouble.__new__(PRDouble)
        holder["proj"] = lambda: norm_value(vars(slf))
        _op = g("op"); _loop = g("loop")
        return PRDouble.__init__(slf, _op, loop=_loop)

    slf = make_self(inp, func)
    st_in = inp.get("state") or {}
    futbox = {}

    def proj_st():
        out = norm_value(copy.deepcopy(st_in))
        for k, v in DEFAULTS.get(func, {}).items():
            if k not in out:
                out[k] = norm_value(copy.deepcopy(v))
        for k in MUT.get(func, []):
            if k == "_inbound_acks":
                out[k] = sorted((t._d.get("id") for t in getattr(slf, k)),
                                key=lambda s: str(s))
            elif k == "fut":
                out[k] = norm_value(futbox.get("fut"))
            else:
                out[k] = norm_value(getattr(slf, k))
        return out

    holder["proj"] = proj_st
    if func == "_per_url_from_wire":
        return NS["_per_url_from_wire"](g("raw"), g("key"))
    if func == "_media_types_from_wire":
        return NS["_media_types_from_wire"](g("raw"))
    if func == "_media_filenames_from_wire":
        return NS["_media_filenames_from_wire"](g("raw"))
    if func == "_await_bounded":
        return asyncio.run(NS["_await_bounded"](g("aw")))
    if func == "_fail_pending":
        sj = g("settle")
        if isinstance(sj, dict) and sj.get("__self__") == "_settle_connection_lost":
            settle = WSTDouble._settle_connection_lost
        elif isinstance(sj, dict) and "__callable__" in sj:
            settle = mk_sync_callable(sj["__callable__"])
        else:
            settle = sj
        return slf._fail_pending(settle)
    if func == "_settle_connection_lost":
        fj = g("fut")
        fut = FutureDouble(fj) if isinstance(fj, dict) else fj
        futbox["fut"] = fut
        return WSTDouble._settle_connection_lost(fut)
    if func == "_dialer_running":
        return slf._dialer_running()
    if func == "_close_reason_of":
        return WSTDouble._close_reason_of(exc_from_input(g("exc")))
    if func == "_latch_if_fresh_token_refused":
        return slf._latch_if_fresh_token_refused(exc_from_input(g("exc")))
    if func == "hold_redial":
        return slf.hold_redial()
    if func == "release_redial":
        return slf.release_redial()
    if func == "_await_redial_hold":
        return asyncio.run(slf._await_redial_hold())
    if func == "_redial_with_fresh_token":
        return asyncio.run(slf._redial_with_fresh_token())
    if func == "_on_descriptor":
        return asyncio.run(slf._on_descriptor(g("frame")))
    if func == "_on_inbound":
        return asyncio.run(slf._on_inbound(g("frame")))
    if func == "_ack_admitted":
        aj = g("admission")
        adm = TaskDouble(aj) if isinstance(aj, dict) else TaskDouble({})
        return slf._ack_admitted(adm, g("buffer_id"))
    if func == "_on_going_idle_ack":
        return asyncio.run(slf._on_going_idle_ack(g("frame")))
    if func == "_on_outbound_result":
        return asyncio.run(slf._on_outbound_result(g("frame")))
    if func == "_on_interrupt_inbound":
        return asyncio.run(slf._on_interrupt_inbound(g("frame")))
    if func == "_on_passthrough_forward":
        return asyncio.run(slf._on_passthrough_forward(g("frame")))
    raise RuntimeError("unknown func " + func)


def run_case(func, inp):
    ctx = CaseCtx(inp)
    CURRENT["ctx"] = ctx
    holder = {}
    try:
        ret = drive(func, inp, holder)
        st = holder["proj"]() if "proj" in holder else norm_value(inp.get("state") or {})
        return {"ret": norm_value(ret), "trace": ctx.trace, "st": st}
    except BaseException as e:  # noqa: BLE001 - fail-closed per case
        spec = None
        for r in ctx.script.handed:
            if e is r.exc:
                spec = r.spec
                break
        if spec is None:
            spec = {"__raise__": type(e).__name__, "message": str(e)}
        st = holder["proj"]() if "proj" in holder else {}
        return {"ret": spec, "trace": ctx.trace, "st": st}


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = rec.get("func")
        inp = rec.get("in") or {}
        n += 1
        want = run_case(func, inp)
        got = norm_value({"ret": rec.get("ret"), "trace": rec.get("trace"), "st": rec.get("st")})
        if canon(got) != canon(want):
            mism += 1
            if mism <= 12:
                print("MISMATCH [%s] case %d" % (func, n))
                for key in ("ret", "trace", "st"):
                    if canon(got.get(key)) != canon(want.get(key)):
                        print("  %s GOT : %s" % (key, json.dumps(got.get(key), ensure_ascii=False)[:700]))
                        print("  %s WANT: %s" % (key, json.dumps(want.get(key), ensure_ascii=False)[:700]))
    print("ORACLE %s: %d cases, %d mismatches" % (NAME, n, mism))
    if n == 0:
        return 1
    return 1 if mism else 0


# ── self-test: drive every fixture, flag suspicious raises / drive failures ──
def selftest(fixtures):
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    bad = 0
    names = set(PORTED)
    funcs_seen = set()
    for i, rec in enumerate(fixtures):
        func = rec.get("func")
        funcs_seen.add(func)
        if func not in names:
            bad += 1
            print(f"SELFTEST case {i}: func {func} not in PORTED")
            continue
        try:
            out = run_case(func, rec.get("in") or {})
            spec = out["ret"]
            if isinstance(spec, dict) and spec.get("__raise__") == "AttributeError":
                msg = spec.get("message", "")
                if any(("'" + n + "'") in msg for n in names) or "WSTDouble" in msg or "PRDouble" in msg:
                    bad += 1
                    print(f"SELFTEST case {i} {func}: suspicious AttributeError {msg}")
        except BaseException as e:  # noqa: BLE001
            bad += 1
            print(f"SELFTEST case {i} {func}: DRIVE FAILURE {type(e).__name__}: {e}")
    faulthandler.cancel_dump_traceback_later()
    missing = names - funcs_seen
    if missing:
        bad += 1
        print(f"SELFTEST: funcs with no fixture: {sorted(missing)}")
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = _HERE.parent / "fixtures" / "relay_protocol_flow.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    sys.exit(main())
