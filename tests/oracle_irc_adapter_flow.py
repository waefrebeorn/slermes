#!/usr/bin/env python3
"""Oracle for the IRC adapter FLOW port
(src/gateway/platforms/irc_adapter_flow.c).

Drives the LIVE plugins/platforms/irc/adapter.py protocol-parse / nick-collision
/ PRIVMSG-gating / dispatch cluster, the markdown-strip + chunk-split pipeline,
the env/config leaves, the async receive loop and the standalone (cron) send
lifecycle with SCRIPTED seams: every SEAMED foreign collaborator is a recording
stub fed the same scripts the C harness feeds its universal seam irc_call.
Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"} in
"trace") plus the final return ("ret") and the mutated state ("state").

The live defs are AST-extracted from adapter.py (module constants seeded, Assign
AND AnnAssign, plain-Name targets only; IRCAdapter / _StandaloneConn methods
extracted with decorators stripped). The real MessageType enum + MessageEvent /
SendResult dataclasses are AST-extracted from gateway/platforms/event.py /
base.py. Every exec namespace is a registered sys.modules module (py3.14). The
heavy quarry import chain (gateway.platforms.base / _shared, agent.i18n,
gateway.config, gateway.platforms.helpers) is never imported; the imported names
are seeded as placeholders / recorders.

Async model: the live coroutines are driven under asyncio.run; awaited seams are
async recorders. asyncio.wait_for is TRANSPARENT (not a seam) — timeouts are
scripted as raises on the inner operation. asyncio.CancelledError / TimeoutError
/ IncompleteReadError are the real classes.

Mode table (mirrors the C port): every ported function runs REAL (inline); only
the foreign collaborators / base methods / stream doubles are SEAMED (regex-
extracted from the C irc_call sites). A REAL function calls its REAL children
directly and its SEAMED children through the recorders.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included); scripted raises round-trip their spec bare.
Summary: 'ORACLE irc_adapter_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- The adapter self double is a real type("IRCAdapter", (), clsdict) instance;
  the conn double a type("_StandaloneConn", (), clsdict) instance. A DIRECT
  self._attr read with the key absent raises AttributeError naming the class.
- Stream/loop/event doubles are presence markers (null == None, object marker ==
  live double); their methods are SEAMED. They project BACK to their marker in
  the state echo. _allowed_users_lower travels as a list (membership semantics
  identical to the live set).
- bytes project to their utf-8 decoded str (ASCII corpus). tuple returns project
  to 2-arrays; the _EOF sentinel projects to {"__eof__":true}; SendResult /
  MessageEvent project via dataclasses.asdict (all 8 / 33 fields; MessageType.TEXT
  -> "text"). Integral floats == ints (canon folds).
- Fixture corpus restrictions are documented in the C port header.
"""
import ast
import asyncio
import copy
import dataclasses
import json
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "plugins", "platforms", "irc", "adapter.py")
EVENT_SRC = os.path.join(Q, "gateway", "platforms", "event.py")
BASE_SRC = os.path.join(Q, "gateway", "platforms", "base.py")
_FLAGS = _fut.annotations.compiler_flag

# module-level functions ported REAL
MODULE_TARGETS = [
    "_parse_irc_message", "_extract_nick", "_ms_id", "_env_or_extra", "_server_channel",
    "_chunk_paragraph", "_privmsg_budget", "_split_lines", "_encode_line", "_ssl_ctx",
    "check_requirements", "validate_config", "is_connected", "_env_enablement",
    "_strip_irc_control_chars", "_is_irc_channel", "_sa_error",
    "_sa_register", "_sa_join", "_standalone_send",
]
# IRCAdapter methods ported REAL: name -> kind
METHOD_KINDS = {
    "name": "method", "_fail": "method", "_split_message": "method",
    "_strip_markdown": "staticmethod", "_send_raw": "method", "_handle_line": "method",
    "_handle_privmsg": "method", "_dispatch_message": "method", "send": "method",
    "send_typing": "method", "get_chat_info": "method", "_receive_loop": "method",
}
# _StandaloneConn methods ported REAL
CONN_KINDS = {"__init__": "method", "raw": "method", "pump": "method", "close": "method"}
TARGETS = MODULE_TARGETS + [k for k in METHOD_KINDS] + ["raw", "pump", "close"]
ASYNC_FUNCS = {
    "_send_raw", "_handle_line", "_handle_privmsg", "_dispatch_message", "send",
    "send_typing", "_receive_loop", "raw", "pump", "close", "get_chat_info",
    "_sa_register", "_sa_join", "_standalone_send",
}
# state-emission flags (mirror the harness): bit0 self
SELF_EMIT = {"_handle_line"}
# seamed base methods bound on every adapter instance (excluded from state echo)
BASE_SEAMS = {"_set_fatal_error", "_notify_fatal_error", "build_source", "handle_message"}


def seg(src, node):
    # start at the def line (node.lineno), EXCLUDING decorators, so extracted
    # methods are plain functions that make_double re-wraps per KINDS.
    return "\n".join(src.splitlines()[node.lineno - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    start = node.lineno
    if isinstance(node, ast.ClassDef):  # keep @dataclass etc. on classes
        for d in getattr(node, "decorator_list", []):
            start = min(start, d.lineno)
    text = textwrap.dedent("\n".join(src.splitlines()[start - 1:node.end_lineno]))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


def extract():
    mod = new_module("ia_oracle_adapter")
    ns = mod.__dict__
    import logging
    import ssl as _ssl
    import time as _time
    import uuid as _uuid
    import contextlib
    from dataclasses import dataclass, field
    from typing import Any, Optional, Dict, List
    from datetime import datetime
    from enum import Enum
    ns.update({"re": re, "json": json, "os": os, "time": _time, "logging": logging,
               "asyncio": asyncio, "contextlib": contextlib, "uuid": _uuid, "ssl": _ssl,
               "dataclass": dataclass, "field": field, "Enum": Enum, "datetime": datetime,
               "Any": Any, "Optional": Optional, "Dict": Dict, "List": List,
               "SessionSource": object, "logger": _SilentLogger()})
    # imported names used by ported defs (seeded; recorders installed per case)
    for nm in ["coerce_port", "_get_scoped_secret", "_seed_extra_from_env", "send_error",
               "t", "BasePlatformAdapter", "SendResult", "cancel_task", "MessageEvent",
               "MessageType", "Platform"]:
        ns[nm] = None
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # module constants (plain-Name Assign / AnnAssign targets only)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    # real MessageType enum + MessageEvent dataclass from event.py
    ev_src = open(EVENT_SRC, encoding="utf-8").read()
    for node in ast.parse(ev_src).body:
        if isinstance(node, ast.ClassDef) and node.name in ("MessageType", "MessageEvent"):
            exec_node(ev_src, node, ns, EVENT_SRC)
    # real SendResult dataclass from base.py
    b_src = open(BASE_SRC, encoding="utf-8").read()
    for node in ast.parse(b_src).body:
        if isinstance(node, ast.ClassDef) and node.name == "SendResult":
            exec_node(b_src, node, ns, BASE_SRC)
    # module-level functions
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in MODULE_TARGETS:
            exec_node(src, node, ns, SRC)
    # IRCAdapter + _StandaloneConn methods
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "IRCAdapter":
            for sub in node.body:
                if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in METHOD_KINDS:
                    exec_node(src, sub, ns, SRC)
        if isinstance(node, ast.ClassDef) and node.name == "_StandaloneConn":
            for sub in node.body:
                if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in CONN_KINDS:
                    exec_node(src, sub, ns, SRC)
    ns["logger"] = _SilentLogger()
    required = TARGETS + ["_TRUTHY", "_MARKDOWN_RULES", "_EOF", "MessageEvent",
                          "MessageType", "SendResult", "__init__"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_irc_adapter_flow: adapter missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()


# ── SEAMED names: regex-extracted from the C port's ia_call sites ──
def seam_names_from_c():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "irc_adapter_flow.c",
                encoding="utf-8").read()
    return set(re.findall(r'irc_call\(\s*ctx,\s*"([^"]+)"', csrc))


SEAMED = seam_names_from_c()


# ── canonical projection ──
def canon(v):
    def conv(x):
        if x is NS.get("_EOF"):
            return {"__eof__": True}
        if isinstance(x, BaseException):
            return str(x)
        if isinstance(x, __import__("enum").Enum):
            return x.value
        if dataclasses.is_dataclass(x) and not isinstance(x, type):
            return conv(dataclasses.asdict(x))
        if isinstance(x, (frozenset, set)):
            return sorted(conv(i) for i in x)
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception materialization / projection ──
EXC_CLASSES = {
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "OSError": OSError,
    "Exception": Exception, "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "StopIteration": StopIteration,
    "TimeoutError": TimeoutError, "CancelledError": asyncio.CancelledError,
    "IncompleteReadError": asyncio.IncompleteReadError,
    "JSONDecodeError": json.JSONDecodeError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    msg = spec.get("message", "")
    if cls is json.JSONDecodeError:
        return cls(msg, "", 0)
    if cls is asyncio.IncompleteReadError:
        return cls(msg, None)
    return cls(msg)


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── per-case script backend (mirrors the C irc_call exactly) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []
        self.scripted = []

    def _raise(self, name, args, spec, exc):
        self.trace.append({"name": name, "args": canon(args), "ret": spec})
        self.scripted.append((exc, spec))
        raise exc

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            msg = f"no script for {name}"
            self._raise(name, args, {"__raise__": "RuntimeError", "message": msg}, RuntimeError(msg))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                msg = f"script exhausted for {name}"
                self._raise(name, args, {"__raise__": "RuntimeError", "message": msg}, RuntimeError(msg))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                rs = used["raise"]
                spec = {"__raise__": rs.get("type", "RuntimeError"), "message": rs.get("message", "")}
                self._raise(name, args, spec, make_exc(rs))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": canon(args), "ret": canon(ret)})
        return ret


def rec(ctx, name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw, ctx) if build else raw
    return f


def arec(ctx, name, args_fn=None, build=None):
    async def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw, ctx) if build else raw
    return f


class RecLogger:
    """logger.<level>(fmt, *args, **kw) -> scripted seam event. BaseException
    args project to str; exc_info kwarg recorded only when present."""
    def __init__(self, ctx):
        self._ctx = ctx

    def __getattr__(self, level):
        def log(fmt, *a, **k):
            args = [str(x) if isinstance(x, BaseException) else x for x in a]
            d = {"fmt": fmt, "args": args}
            if "exc_info" in k:
                d["exc_info"] = k["exc_info"]
            return self._ctx.call(f"logger.{level}", d)
        return log


# ── stream / loop / event doubles (project back to their marker in echoes) ──
class WriterDouble:
    def __init__(self, ctx, marker):
        self.__ia_marker__ = marker
        self._ctx = ctx

    def is_closing(self):
        return self._ctx.call("writer.is_closing", {})

    def write(self, data):
        return self._ctx.call("writer.write", {"data": data.decode() if isinstance(data, bytes) else data})

    async def drain(self):
        return self._ctx.call("writer.drain", {})

    def close(self):
        return self._ctx.call("writer.close", {})

    async def wait_closed(self):
        return self._ctx.call("writer.wait_closed", {})


class ReaderDouble:
    def __init__(self, ctx, marker):
        self.__ia_marker__ = marker
        self._ctx = ctx

    def at_eof(self):
        return self._ctx.call("reader.at_eof", {})

    async def read(self, n):
        v = self._ctx.call("reader.read", {"n": n})
        return v.encode() if isinstance(v, str) else v

    async def readuntil(self, sep):
        s = sep.decode() if isinstance(sep, bytes) else sep
        v = self._ctx.call("reader.readuntil", {"sep": s})
        return v.encode() if isinstance(v, str) else v


class LoopDouble:
    def __init__(self, ctx, marker):
        self.__ia_marker__ = marker
        self._ctx = ctx

    def time(self):
        return self._ctx.call("loop.time", {})


class EventDouble:
    def __init__(self, ctx, marker):
        self.__ia_marker__ = marker
        self._ctx = ctx

    def set(self):
        return self._ctx.call("registration_event.set", {})


def mat_writer(v, ctx):
    return None if v is None else WriterDouble(ctx, v)


def mat_reader(v, ctx):
    return None if v is None else ReaderDouble(ctx, v)


def mat_loop(v, ctx):
    return None if v is None else LoopDouble(ctx, v)


def mat_event(v, ctx):
    return None if v is None else EventDouble(ctx, v)


def mat_config(d):
    return None if d is None else types.SimpleNamespace(**d)


# ── seam argument projections / result builders ──
def _conv_tag(conv):
    if conv is int:
        return "int"
    if conv is None:
        return None
    return "truthy"


def _seed_spec_proj(spec):
    return [[env, key, _conv_tag(conv)] for env, key, conv in spec]


def _uuid_build(r, ctx):
    return types.SimpleNamespace(**r) if isinstance(r, dict) else r


def _conn_build(r, ctx):
    # open_connection result is a 2-array [reader_marker, writer_marker]; only
    # materialize doubles for a well-formed 2-array. Any other shape is returned
    # raw so the live `reader, writer = ...` unpack raises the natural error.
    if isinstance(r, list) and len(r) == 2:
        return (mat_reader(r[0], ctx), mat_writer(r[1], ctx))
    return r


def _loop_build(r, ctx):
    return mat_loop(r, ctx)


def _bytes_build(r, ctx):
    return r.encode() if isinstance(r, str) else r


ARGMAP = {
    "_get_scoped_secret": lambda name, default=None: {"name": name, "default": default},
    "_seed_extra_from_env": lambda spec, *, home_env=None, home_default="": {
        "spec": _seed_spec_proj(spec), "home_env": home_env, "home_default": home_default},
    "send_error": lambda message, **k: {"message": message},
    "t": lambda key: {"key": key},
    "time.time": lambda: {},
    "uuid.uuid4": lambda: {},
    "datetime.now": lambda: {},
    "ssl.create_default_context": lambda: {},
    "asyncio.open_connection": lambda server, port, ssl=None: {"server": server, "port": port, "ssl": ssl},
    "asyncio.sleep": lambda seconds: {"seconds": seconds},
    "asyncio.get_running_loop": lambda: {},
    "writer.is_closing": lambda: {},
    "writer.write": lambda data: {"data": data.decode() if isinstance(data, bytes) else data},
    "writer.drain": lambda: {},
    "writer.close": lambda: {},
    "writer.wait_closed": lambda: {},
    "reader.at_eof": lambda: {},
    "reader.read": lambda n: {"n": n},
    "reader.readuntil": lambda sep: {"sep": sep.decode() if isinstance(sep, bytes) else sep},
    "loop.time": lambda: {},
    "registration_event.set": lambda: {},
    "_set_fatal_error": lambda code, message, *, retryable: {"code": code, "message": message, "retryable": retryable},
    "_notify_fatal_error": lambda: {},
    "build_source": lambda **kw: kw,
    "handle_message": lambda event: {"event": canon(event)},
    "on_msg": lambda command: {"command": command},
}
BUILD = {
    "uuid.uuid4": _uuid_build,
    "asyncio.open_connection": _conn_build,
    "asyncio.get_running_loop": _loop_build,
    "reader.read": _bytes_build,
    "reader.readuntil": _bytes_build,
}
# seams whose live call is awaited (async recorder)
ASYNC_SEAMS = {
    "asyncio.sleep", "asyncio.open_connection", "writer.drain", "writer.wait_closed",
    "reader.read", "reader.readuntil", "_notify_fatal_error", "handle_message", "on_msg",
}


def make_asyncio_double(ctx):
    a = types.SimpleNamespace()
    a.CancelledError = asyncio.CancelledError
    a.TimeoutError = TimeoutError
    a.IncompleteReadError = asyncio.IncompleteReadError

    async def wait_for(aw, timeout):
        return await aw
    a.wait_for = wait_for
    a.sleep = arec(ctx, "asyncio.sleep", ARGMAP["asyncio.sleep"])
    a.open_connection = arec(ctx, "asyncio.open_connection", ARGMAP["asyncio.open_connection"],
                             BUILD["asyncio.open_connection"])
    a.get_running_loop = rec(ctx, "asyncio.get_running_loop", ARGMAP["asyncio.get_running_loop"],
                             BUILD["asyncio.get_running_loop"])
    return a


def install_recorders(ctx, func):
    NS["_get_scoped_secret"] = rec(ctx, "_get_scoped_secret", ARGMAP["_get_scoped_secret"])
    NS["_seed_extra_from_env"] = rec(ctx, "_seed_extra_from_env", ARGMAP["_seed_extra_from_env"])
    NS["send_error"] = rec(ctx, "send_error", ARGMAP["send_error"])
    NS["t"] = rec(ctx, "t", ARGMAP["t"])
    NS["time"] = types.SimpleNamespace(time=rec(ctx, "time.time", ARGMAP["time.time"]))
    NS["uuid"] = types.SimpleNamespace(uuid4=rec(ctx, "uuid.uuid4", ARGMAP["uuid.uuid4"], BUILD["uuid.uuid4"]))
    NS["datetime"] = types.SimpleNamespace(datetime=types.SimpleNamespace(
        now=rec(ctx, "datetime.now", ARGMAP["datetime.now"])))
    NS["ssl"] = types.SimpleNamespace(create_default_context=rec(
        ctx, "ssl.create_default_context", ARGMAP["ssl.create_default_context"]))
    NS["logger"] = RecLogger(ctx)
    NS["asyncio"] = make_asyncio_double(ctx)
    NS["IRCAdapter"] = ADAPTER_CLS
    NS["_StandaloneConn"] = CONN_CLS


# ── adapter / conn double classes (built once from the REAL extracted defs) ──
def _build_adapter_cls():
    clsdict = {}
    for name, kind in METHOD_KINDS.items():
        fn = NS[name]
        clsdict[name] = staticmethod(fn) if kind == "staticmethod" else fn
    return type("IRCAdapter", (), clsdict)


def _build_conn_cls():
    clsdict = {nm: NS[nm] for nm in CONN_KINDS}
    return type("_StandaloneConn", (), clsdict)


ADAPTER_CLS = _build_adapter_cls()
CONN_CLS = _build_conn_cls()


def mat_self_attr(k, v, ctx):
    if k == "_writer":
        return mat_writer(v, ctx)
    if k == "_reader":
        return mat_reader(v, ctx)
    if k == "_registration_event":
        return mat_event(v, ctx)
    return copy.deepcopy(v)


def make_adapter(state, ctx):
    inst = ADAPTER_CLS()
    for k, v in (state or {}).items():
        setattr(inst, k, mat_self_attr(k, v, ctx))
    # seamed base methods (instance attrs; excluded from the state echo)
    inst._set_fatal_error = rec(ctx, "_set_fatal_error", ARGMAP["_set_fatal_error"])
    inst._notify_fatal_error = arec(ctx, "_notify_fatal_error", ARGMAP["_notify_fatal_error"])
    inst.build_source = rec(ctx, "build_source", ARGMAP["build_source"])
    inst.handle_message = arec(ctx, "handle_message", ARGMAP["handle_message"])
    inst.__ia_injected__ = set(BASE_SEAMS)
    return inst


def make_conn(state, ctx):
    inst = object.__new__(CONN_CLS)   # bypass __init__ (no get_running_loop seam)
    st = state or {}
    # only set attrs PRESENT in state: an absent attr raises AttributeError
    # naming _StandaloneConn on access; a present null raises NoneType.<method>.
    if "reader" in st:
        inst.reader = mat_reader(st["reader"], ctx)
    if "writer" in st:
        inst.writer = mat_writer(st["writer"], ctx)
    if "loop" in st:
        inst._loop = mat_loop(st["loop"], ctx)
    return inst


def norm_state(inst):
    out = {}
    injected = getattr(inst, "__ia_injected__", set())
    for k, v in vars(inst).items():
        if k.startswith("__ia_") or k in injected:
            continue
        if isinstance(v, (WriterDouble, ReaderDouble, LoopDouble, EventDouble)):
            out[k] = v.__ia_marker__
        else:
            out[k] = v
    return out


def exc_spec(exc, ctx):
    for e, spec in ctx.scripted:
        if exc is e:
            return spec
    return proj_exc(exc)


def proj_event(event):
    return canon(event)


# ── per-case live call ──
def live_call(func, args, inst, conn, ctx):
    g = args.get
    if func == "_parse_irc_message":
        return NS[func](g("raw"))
    if func == "_extract_nick":
        return NS[func](g("prefix"))
    if func == "_ms_id":
        return NS[func]()
    if func == "_env_or_extra":
        return NS[func](g("extra"), g("env"), g("key"), g("default", ""))
    if func == "_server_channel":
        return NS[func](mat_config(g("config")))
    if func == "_chunk_paragraph":
        return NS[func](g("paragraph"), g("limit"))
    if func == "_privmsg_budget":
        return NS[func](g("target"))
    if func == "_split_lines":
        return NS[func](g("paragraphs"), g("limit"))
    if func == "_encode_line":
        return NS[func](g("line")).decode()
    if func == "_ssl_ctx":
        return NS[func](g("use_tls"))
    if func == "check_requirements":
        return NS[func]()
    if func == "validate_config":
        return NS[func](mat_config(g("config")))
    if func == "is_connected":
        return NS[func](mat_config(g("config")))
    if func == "_env_enablement":
        return NS[func]()
    if func == "_strip_irc_control_chars":
        return NS[func](g("text"))
    if func == "_is_irc_channel":
        return NS[func](g("target"))
    if func == "_sa_error":
        return NS[func](g("detail"))
    if func == "name":
        return inst.name()
    if func == "_fail":
        return inst._fail(g("code"), g("message"), retryable=g("retryable"))
    if func == "_split_message":
        return inst._split_message(g("content"), g("target"))
    if func == "_strip_markdown":
        return ADAPTER_CLS._strip_markdown(g("text"))
    if func == "get_chat_info":
        return asyncio.run(inst.get_chat_info(g("chat_id")))
    if func == "_send_raw":
        return asyncio.run(inst._send_raw(g("line")))
    if func == "_handle_line":
        return asyncio.run(inst._handle_line(g("raw")))
    if func == "_handle_privmsg":
        return asyncio.run(inst._handle_privmsg(g("sender_nick"), g("target"), g("text")))
    if func == "_dispatch_message":
        return asyncio.run(inst._dispatch_message(text=g("text"), chat_id=g("chat_id"),
                                                  chat_type=g("chat_type"), user_id=g("user_id"),
                                                  user_name=g("user_name")))
    if func == "send":
        return asyncio.run(inst.send(g("chat_id"), g("content"), reply_to=g("reply_to"),
                                     metadata=g("metadata")))
    if func == "send_typing":
        return asyncio.run(inst.send_typing(g("chat_id"), metadata=g("metadata")))
    if func == "_receive_loop":
        return asyncio.run(inst._receive_loop())
    if func == "raw":
        return asyncio.run(conn.raw(g("line")))
    if func == "pump":
        async def on_msg(command):
            return ctx.call("on_msg", {"command": command})
        return asyncio.run(conn.pump(g("timeout"), on_msg))
    if func == "close":
        return asyncio.run(conn.close())
    if func == "_sa_register":
        return asyncio.run(NS[func](conn, g("nick_base"), g("server_password")))
    if func == "_sa_join":
        return asyncio.run(NS[func](conn, g("target")))
    if func == "_standalone_send":
        return asyncio.run(NS[func](mat_config(g("pconfig")), g("chat_id"), g("message"),
                                    thread_id=g("thread_id"), media_files=g("media_files"),
                                    force_document=g("force_document", False)))
    raise RuntimeError(f"unknown func {func}")


# ── per-case drive ──
def drive(func, inp):
    args = copy.deepcopy(inp.get("args") or {})
    ctx = Ctx(inp.get("io"))
    install_recorders(ctx, func)
    state = inp.get("state")
    inst = make_adapter(state, ctx)
    conn = make_conn(state, ctx)
    state_out = {}
    try:
        ret = live_call(func, args, inst, conn, ctx)
    except BaseException as exc:  # SystemExit included
        ret = exc_spec(exc, ctx)
    if func in SELF_EMIT:
        state_out["self"] = norm_state(inst)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── three-way seam-name cross-check ──
# logger.<level> seams are recorded dynamically by RecLogger (not in ARGMAP).
DYNAMIC_SEAMS = {"logger.debug", "logger.error", "logger.warning"}


def cross_check():
    missing_argmap = sorted(SEAMED - set(ARGMAP) - DYNAMIC_SEAMS)
    if missing_argmap:
        print(f"FATAL oracle_irc_adapter_flow: C seams without ARGMAP: {missing_argmap}")
        sys.exit(2)
    extra_argmap = sorted(set(ARGMAP) - SEAMED)
    if extra_argmap:
        print(f"FATAL oracle_irc_adapter_flow: ARGMAP seams absent from C: {extra_argmap}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "irc_adapter_flow.jsonl"
    if fix.exists():
        fix_keys, fix_funcs = set(), set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func"))
        bad = fix_keys - SEAMED
        if bad:
            print(f"FATAL oracle_irc_adapter_flow: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        badf = fix_funcs - set(TARGETS)
        if badf:
            print(f"FATAL oracle_irc_adapter_flow: fixture funcs without a target: {sorted(badf)}")
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
            rec_in = json.loads(line)
        except Exception as exc:
            m += 1
            print(f"MISMATCH <unparsable harness line>: {exc}")
            continue
        n += 1
        func = rec_in.get("func", "")
        inp = rec_in.get("in") or {}
        got = rec_in.get("out")
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
    print(f"ORACLE irc_adapter_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    fix = _REPO / "tests" / "fixtures" / "irc_adapter_flow.jsonl"
    n = bad = 0
    counts = {}
    extraction_names = set(TARGETS) | SEAMED
    for line in open(fix, encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        case = json.loads(line)
        n += 1
        func = case["func"]
        counts[func] = counts.get(func, 0) + 1
        try:
            out = drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
            continue
        r = out["ret"]
        io = (case.get("in") or {}).get("io") or {}
        if isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            if io:
                bad += 1
                print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            for nm in extraction_names:
                if f"'{nm}'" in msg and nm not in ("_writer", "_reader", "_registration_event",
                                                    "_current_nick", "_allowed_users_lower",
                                                    "max_message_length", "nickname", "_message_handler",
                                                    "is_connected", "_loop", "writer", "reader", "loop"):
                    bad += 1
                    print(f"SELFTEST ATTR FAMILY {func} case {n}: {msg}")
                    break
    faulthandler.cancel_dump_traceback_later()
    print(f"SELFTEST irc_adapter_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    missing = set(TARGETS) - set(counts)
    if missing:
        print(f"SELFTEST MISSING FUNCS: {sorted(missing)}")
        bad += 1
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
