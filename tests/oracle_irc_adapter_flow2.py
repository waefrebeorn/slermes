#!/usr/bin/env python3
"""Oracle for the IRC adapter FLOW remainder port
(src/gateway/platforms/irc_adapter_flow2.c).

Drives the LIVE plugins/platforms/irc/adapter.py construction / connect /
disconnect lifecycle, the interactive CLI setup flow (+ nested info /
_required), the standalone-conn constructor, the registration / join pump
closures and the plugin entry point with SCRIPTED seams: every SEAMED foreign
collaborator and every flow1-claimed sibling is a recording stub fed the same
scripts the C harness feeds its universal seam irc2_call. Compares the DECISION
SEQUENCE (ordered seam events {"name","args","ret"} in "trace") plus the final
return ("ret") and the mutated state ("state").

The live defs are AST-extracted from adapter.py (module constants seeded).
IRCAdapter.__init__ is extracted inside a synthetic class body so its zero-arg
super() gets a __class__ closure cell; drive() rebinds that cell to a per-case
class whose base carries the seamed base.__init__ recorder. The nonlocal pump
closures (_on_registration / _on_join) are extracted inside synthetic factory
functions so their `nonlocal` cells bind; drive() instantiates the factory with
the fixture cell values and reads the mutated cells back via
inspect.getclosurevars. The in-body `from gateway.platform_registry import
platform_registry` is intercepted by a module-level __getattr__ firing the
import_platform_registry seam (dunder probes guarded). Every exec namespace is
a registered sys.modules module (py3.14). The heavy quarry import chain is
never imported; imported names are seeded as recorders / placeholders.

Async model: live coroutines driven under asyncio.run; awaited seams are async
recorders. asyncio.wait_for is TRANSPARENT; asyncio.create_task does NOT run
the receive loop (self._receive_loop() creates a coroutine projected to
{"__coro__":"_receive_loop"}; no seam fires for the creation).

Mode table (mirrors the C port): flow2 siblings run REAL (connect -> disconnect,
interactive_setup -> info / _required); flow1 siblings (_env_or_extra, _ssl_ctx,
_fail, _send_raw, _sa_error, conn.raw) and foreign collaborators are SEAMED.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included); scripted raises round-trip their spec bare.
Summary: 'ORACLE irc_adapter_flow2: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- The adapter self double is a real type("IRCAdapter", (base,), clsdict)
  instance; a DIRECT self._attr read with the key absent raises AttributeError
  naming the class. The conn double uses "_StandaloneConn"; its state keys are
  reader / writer / _loop. Closure cells travel as state keys.
- Stream/event doubles are presence markers projecting BACK to their marker in
  the state echo. Callable register kwargs project to {"__fn__":name} /
  {"__cls__":"IRCAdapter"}; the coroutine marker is {"__coro__":"_receive_loop"}.
- Integral floats == ints (canon folds). f-string/str() positions carry
  str/int/bool/None only. Fixture corpus restrictions live in the C port header.
"""
import ast
import asyncio
import copy
import inspect
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
_FLAGS = _fut.annotations.compiler_flag

# ported functions (harness func names)
TARGETS = [
    "__init__", "connect", "disconnect", "interactive_setup", "info", "_required",
    "_StandaloneConn.__init__", "_on_registration", "_on_join", "register",
]
METHOD_KINDS = {"__init__": "method", "connect": "method", "disconnect": "method"}
# seamed base methods bound on every adapter instance (excluded from state echo)
BASE_SEAMS = {"_fail", "_send_raw", "_acquire_platform_lock", "_release_platform_lock",
              "_mark_connected", "_mark_disconnected", "_wire_plugin_handlers", "_receive_loop"}
ADAPTER_STATE_ATTRS = {
    "server", "channel", "nickname", "port", "use_tls", "server_password",
    "nickserv_password", "_current_nick", "_writer", "_reader", "_recv_task",
    "_registration_event", "_registered", "max_message_length", "allowed_users",
    "_allowed_users_lower", "is_connected", "_message_handler",
}
CONN_STATE_ATTRS = {"reader", "writer", "_loop"}
CELL_ATTRS = {"nick_base", "nick_attempts", "standalone_nick", "target"}


def seg(src, node):
    return "\n".join(src.splitlines()[node.lineno - 1:node.end_lineno])


def exec_text(text, ns, path):
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


def _find(tree, name, cls=None):
    for n in ast.walk(tree):
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name == name:
            if cls is None:
                return n
    return None


def _method_node(tree, clsname, methname):
    for n in tree.body:
        if isinstance(n, ast.ClassDef) and n.name == clsname:
            for sub in n.body:
                if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name == methname:
                    return sub
    return None


def extract():
    mod = new_module("ia2_oracle_adapter")
    ns = mod.__dict__
    import logging
    import ssl as _ssl
    import time as _time
    import uuid as _uuid
    import contextlib
    from typing import Any, Optional, Dict, List
    from datetime import datetime
    from enum import Enum
    ns.update({"re": re, "json": json, "os": os, "time": _time, "logging": logging,
               "asyncio": asyncio, "contextlib": contextlib, "uuid": _uuid, "ssl": _ssl,
               "Enum": Enum, "datetime": datetime, "Any": Any, "Optional": Optional,
               "Dict": Dict, "List": List, "logger": _SilentLogger()})
    for nm in ["coerce_port", "_get_scoped_secret", "send_error", "t", "BasePlatformAdapter",
               "SendResult", "cancel_task", "MessageEvent", "MessageType", "Platform",
               "_env_or_extra", "_ssl_ctx", "_sa_error", "print"]:
        ns[nm] = None
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # module constants (plain-Name Assign / AnnAssign)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_text(textwrap.dedent(seg(src, node)), ns, SRC)
            except Exception:
                pass
    # module-level functions
    for nm in ["interactive_setup", "register"]:
        exec_text(textwrap.dedent(seg(src, _find(tree, nm))), ns, SRC)
    # _StandaloneConn.__init__ extracted into ns (shares per-case asyncio double);
    # rename to _conn_init so it does not clobber the adapter __init__ below.
    conn_init_node = _method_node(tree, "_StandaloneConn", "__init__")
    exec_text(textwrap.dedent(seg(src, conn_init_node)), ns, SRC)
    ns["_conn_init"] = ns.pop("__init__")
    # IRCAdapter.__init__ inside a synthetic class (super() __class__ cell)
    init_node = _method_node(tree, "IRCAdapter", "__init__")
    exec_text("class _Wrap:\n" + textwrap.indent(seg(src, init_node), "    "), ns, SRC)
    ns["__init__"] = ns["_Wrap"].__init__
    # connect / disconnect standalone (no super)
    for nm in ["connect", "disconnect"]:
        exec_text(textwrap.dedent(seg(src, _method_node(tree, "IRCAdapter", nm))), ns, SRC)
    # nested info / _required (global refs to the hermes_cli recorders)
    setup_node = _find(tree, "interactive_setup")
    for nm in ["info", "_required"]:
        sub = next(n for n in setup_node.body if isinstance(n, ast.FunctionDef) and n.name == nm)
        exec_text(textwrap.dedent(seg(src, sub)), ns, SRC)
    # nonlocal closure factories
    on_reg = next(n for n in _find(tree, "_sa_register").body
                  if isinstance(n, ast.AsyncFunctionDef) and n.name == "_on_registration")
    reg_src = ("def _reg_factory(nick_base, nick_attempts, standalone_nick, conn, _sa_error):\n"
               + textwrap.indent(textwrap.dedent(seg(src, on_reg)), "    ")
               + "\n    return _on_registration\n")
    exec_text(reg_src, ns, SRC)
    on_join = next(n for n in _find(tree, "_sa_join").body
                   if isinstance(n, ast.AsyncFunctionDef) and n.name == "_on_join")
    join_src = ("def _join_factory(target, _sa_error):\n"
                + textwrap.indent(textwrap.dedent(seg(src, on_join)), "    ")
                + "\n    return _on_join\n")
    exec_text(join_src, ns, SRC)
    ns["logger"] = _SilentLogger()
    required = ["interactive_setup", "register", "__init__", "connect", "disconnect",
                "_conn_init", "info", "_required", "_reg_factory", "_join_factory",
                "_TRUTHY", "_EOF"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_irc_adapter_flow2: adapter missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()
REG_FACTORY = NS["_reg_factory"]
JOIN_FACTORY = NS["_join_factory"]
CONN_CLS = type("_StandaloneConn", (), {"__init__": NS["_conn_init"]})


# placeholder callables for register's flow1-claimed references (stable __name__)
def _make_placeholder(name):
    def f(*a, **k):
        return None
    f.__name__ = name
    return f


PLACEHOLDERS = {nm: _make_placeholder(nm) for nm in
                ["check_requirements", "validate_config", "is_connected",
                 "_env_enablement", "_standalone_send"]}


# ── SEAMED names: regex-extracted from the C port's irc2_call sites (plus the
# thin irc2_print_text wrapper, whose 2nd arg is the literal seam name) ──
def seam_names_from_c():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "irc_adapter_flow2.c",
                encoding="utf-8").read()
    names = set(re.findall(r'irc2_call\(\s*ctx,\s*"([^"]+)"', csrc))
    names |= set(re.findall(r'irc2_print_text\(\s*ctx,\s*"([^"]+)"', csrc))
    return names


SEAMED = seam_names_from_c()


# ── canonical projection ──
class _CoroMarker:
    def __init__(self, name):
        self.name = name


def canon(v):
    def conv(x):
        if x is NS.get("_EOF"):
            return {"__eof__": True}
        if isinstance(x, _CoroMarker):
            return {"__coro__": x.name}
        if hasattr(x, "__ia_marker__"):
            return x.__ia_marker__
        if isinstance(x, BaseException):
            return str(x)
        if isinstance(x, __import__("enum").Enum):
            return x.value
        if isinstance(x, type):
            return {"__cls__": x.__name__}
        if callable(x):
            return {"__fn__": getattr(x, "__name__", "callable")}
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (frozenset, set)):
            return sorted(conv(i) for i in x)
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
    "IncompleteReadError": asyncio.IncompleteReadError, "ImportError": ImportError,
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


# ── per-case script backend (mirrors the C irc2_call exactly) ──
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


# ── stream / event doubles (project back to their marker in echoes) ──
class WriterDouble:
    def __init__(self, ctx, marker):
        self.__ia_marker__ = marker
        self._ctx = ctx

    def is_closing(self):
        return self._ctx.call("writer.is_closing", {})

    def close(self):
        return self._ctx.call("writer.close", {})

    async def wait_closed(self):
        return self._ctx.call("writer.wait_closed", {})


class ReaderDouble:
    def __init__(self, ctx, marker):
        self.__ia_marker__ = marker
        self._ctx = ctx


class EventDouble:
    def __init__(self, ctx, marker):
        self.__ia_marker__ = marker
        self._ctx = ctx

    async def wait(self):
        return self._ctx.call("registration_event.wait", {})

    def clear(self):
        return self._ctx.call("registration_event.clear", {})

    def set(self):
        return self._ctx.call("registration_event.set", {})


class ConnRawDouble:
    def __init__(self, ctx):
        self._ctx = ctx

    async def raw(self, line):
        return self._ctx.call("conn.raw", {"line": line})


class RegistryDouble:
    def __init__(self, ctx):
        self._ctx = ctx

    def get(self, name):
        raw = self._ctx.call("platform_registry.get", {"name": name})
        return types.SimpleNamespace(**raw) if isinstance(raw, dict) else raw


class CtxDouble:
    def __init__(self, ctx):
        self._ctx = ctx

    def register_platform(self, **kw):
        return self._ctx.call("ctx.register_platform", kw)


def mat_writer(v, ctx):
    return None if v is None else WriterDouble(ctx, v)


def mat_reader(v, ctx):
    return None if v is None else ReaderDouble(ctx, v)


def mat_event(v, ctx):
    return None if v is None else EventDouble(ctx, v)


def mat_config(d):
    return None if d is None else types.SimpleNamespace(**d)


def _conn_build(r, ctx):
    if isinstance(r, list) and len(r) == 2:
        return (mat_reader(r[0], ctx), mat_writer(r[1], ctx))
    return r


def _registry_build(r, ctx):
    return None if r is None else RegistryDouble(ctx)


# ── seam argument projections / result builders ──
ARGMAP = {
    "Platform": lambda value: {"value": value},
    "base.__init__": lambda config=None, platform=None: {"config": config, "platform": platform},
    "_env_or_extra": lambda extra, env, key, default="": {"extra": extra, "env": env, "key": key, "default": default},
    "coerce_port": lambda value, default: {"value": value, "default": default},
    "_get_scoped_secret": lambda name, default=None: {"name": name, "default": default},
    "import_platform_registry": lambda: {},
    "platform_registry.get": lambda name: {"name": name},
    "asyncio.Event": lambda: {},
    "asyncio.open_connection": lambda server, port, ssl=None: {"server": server, "port": port, "ssl": ssl},
    "asyncio.sleep": lambda seconds: {"seconds": seconds},
    "asyncio.create_task": lambda coro: {"coro": coro},
    "asyncio.get_running_loop": lambda: {},
    "_ssl_ctx": lambda use_tls: {"use_tls": use_tls},
    "_fail": lambda code, message, *, retryable: {"code": code, "message": message, "retryable": retryable},
    "_send_raw": lambda line: {"line": line},
    "_sa_error": lambda detail: {"detail": detail},
    "conn.raw": lambda line: {"line": line},
    "_acquire_platform_lock": lambda scope, identity, resource_desc: {
        "scope": scope, "identity": identity, "resource_desc": resource_desc},
    "_release_platform_lock": lambda: {},
    "_mark_connected": lambda **k: k,
    "_mark_disconnected": lambda: {},
    "_wire_plugin_handlers": lambda native=None: {"native": native},
    "cancel_task": lambda task: {"task": task},
    "registration_event.wait": lambda: {},
    "registration_event.clear": lambda: {},
    "writer.is_closing": lambda: {},
    "writer.close": lambda: {},
    "writer.wait_closed": lambda: {},
    "t": lambda key: {"key": key},
    "print": lambda *a, **k: {"args": list(a)},
    "print_header": lambda title, **k: {"title": title},
    "print_info": lambda text: {"text": text},
    "print_warning": lambda text: {"text": text},
    "print_success": lambda text: {"text": text},
    "prompt": lambda question, **k: {"question": question, **k},
    "prompt_yes_no": lambda question, default=True, **k: {"question": question, "default": default},
    "save_env_value": lambda key, value: {"key": key, "value": value},
    "get_env_value": lambda key: {"key": key},
    "declines_reconfigure": lambda label, question, *env_vars: {
        "label": label, "question": question, "env_vars": list(env_vars)},
    "ctx.register_platform": lambda **kw: kw,
}
BUILD = {
    "asyncio.open_connection": _conn_build,
    "asyncio.Event": mat_event,
    "import_platform_registry": _registry_build,
}
ASYNC_SEAMS = {
    "asyncio.sleep", "asyncio.open_connection", "writer.wait_closed",
    "registration_event.wait", "cancel_task", "conn.raw", "_send_raw",
}
DYNAMIC_SEAMS = {"logger.debug", "logger.error", "logger.warning", "logger.info"}


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
    a.create_task = rec(ctx, "asyncio.create_task", ARGMAP["asyncio.create_task"])
    a.get_running_loop = rec(ctx, "asyncio.get_running_loop", ARGMAP["asyncio.get_running_loop"])
    a.Event = rec(ctx, "asyncio.Event", ARGMAP["asyncio.Event"], BUILD["asyncio.Event"])
    return a


def install_recorders(ctx, func):
    NS["_env_or_extra"] = rec(ctx, "_env_or_extra", ARGMAP["_env_or_extra"])
    NS["_ssl_ctx"] = rec(ctx, "_ssl_ctx", ARGMAP["_ssl_ctx"])
    NS["_sa_error"] = rec(ctx, "_sa_error", ARGMAP["_sa_error"])
    NS["_get_scoped_secret"] = rec(ctx, "_get_scoped_secret", ARGMAP["_get_scoped_secret"])
    NS["coerce_port"] = rec(ctx, "coerce_port", ARGMAP["coerce_port"])
    NS["t"] = rec(ctx, "t", ARGMAP["t"])
    NS["Platform"] = rec(ctx, "Platform", ARGMAP["Platform"])
    NS["cancel_task"] = arec(ctx, "cancel_task", ARGMAP["cancel_task"])
    NS["logger"] = RecLogger(ctx)
    NS["print"] = rec(ctx, "print", ARGMAP["print"])
    NS["asyncio"] = make_asyncio_double(ctx)
    for nm, ph in PLACEHOLDERS.items():
        NS[nm] = ph
    # hermes_cli wizard stub modules (from-import binds these recorders)
    setup_names = ["prompt", "prompt_yes_no", "save_env_value", "get_env_value",
                   "print_header", "print_info", "print_warning", "print_success"]
    parent = types.ModuleType("hermes_cli")
    parent.__path__ = []
    setup = types.ModuleType("hermes_cli.setup")
    for nm in setup_names:
        r = rec(ctx, nm, ARGMAP[nm])
        setattr(setup, nm, r)
        NS[nm] = r   # nested info/_required direct drive resolves these globals
    sp = types.ModuleType("hermes_cli.setup_platforms")
    sp.declines_reconfigure = rec(ctx, "declines_reconfigure", ARGMAP["declines_reconfigure"])
    sys.modules["hermes_cli"] = parent
    sys.modules["hermes_cli.setup"] = setup
    sys.modules["hermes_cli.setup_platforms"] = sp
    # platform_registry import seam via module __getattr__ (dunder probes guarded)
    gparent = types.ModuleType("gateway")
    gparent.__path__ = []
    gmod = types.ModuleType("gateway.platform_registry")

    def ga(name):
        if name.startswith("__"):
            raise AttributeError(name)
        raw = ctx.call("import_platform_registry", {})
        return _registry_build(raw, ctx)
    gmod.__getattr__ = ga
    sys.modules["gateway"] = gparent
    sys.modules["gateway.platform_registry"] = gmod


# ── per-case adapter class (base carries seamed __init__; rebind super cell) ──
def build_adapter_cls(ctx):
    # base.__init__ is CLASS-level (super() MRO), so Python inserts self; the
    # recorder must accept and drop self (instance-attr seams elsewhere don't).
    def _base_init(self, config=None, platform=None):
        return ctx.call("base.__init__", {"config": config, "platform": platform})
    base = type("BasePlatformAdapter", (), {"__init__": _base_init})
    clsdict = {nm: NS[nm] for nm in METHOD_KINDS}
    cls = type("IRCAdapter", (base,), clsdict)
    init_fn = NS["__init__"]
    for n, c in zip(init_fn.__code__.co_freevars, init_fn.__closure__ or ()):
        if n == "__class__":
            c.cell_contents = cls
    return cls


def mat_self_attr(k, v, ctx):
    if k == "_writer":
        return mat_writer(v, ctx)
    if k == "_reader":
        return mat_reader(v, ctx)
    if k == "_registration_event":
        return mat_event(v, ctx)
    return copy.deepcopy(v)


def make_adapter(state, ctx, cls):
    inst = object.__new__(cls)
    for k, v in (state or {}).items():
        setattr(inst, k, mat_self_attr(k, v, ctx))
    inst._fail = rec(ctx, "_fail", ARGMAP["_fail"])
    inst._send_raw = arec(ctx, "_send_raw", ARGMAP["_send_raw"])
    inst._acquire_platform_lock = rec(ctx, "_acquire_platform_lock", ARGMAP["_acquire_platform_lock"])
    inst._release_platform_lock = rec(ctx, "_release_platform_lock", ARGMAP["_release_platform_lock"])
    inst._mark_connected = rec(ctx, "_mark_connected", ARGMAP["_mark_connected"])
    inst._mark_disconnected = rec(ctx, "_mark_disconnected", ARGMAP["_mark_disconnected"])
    inst._wire_plugin_handlers = rec(ctx, "_wire_plugin_handlers", ARGMAP["_wire_plugin_handlers"])
    inst._receive_loop = lambda: _CoroMarker("_receive_loop")
    inst.__ia_injected__ = set(BASE_SEAMS)
    return inst


def norm_state(inst):
    out = {}
    injected = getattr(inst, "__ia_injected__", set())
    for k, v in vars(inst).items():
        if k.startswith("__ia_") or k in injected:
            continue
        if hasattr(v, "__ia_marker__"):
            out[k] = v.__ia_marker__
        else:
            out[k] = v
    return out


def exc_spec(exc, ctx):
    for e, spec in ctx.scripted:
        if exc is e:
            return spec
    return proj_exc(exc)


# ── per-case drive ──
def drive(func, inp):
    args = copy.deepcopy(inp.get("args") or {})
    ctx = Ctx(inp.get("io"))
    state = inp.get("state") or {}
    install_recorders(ctx, func)
    cls = build_adapter_cls(ctx)
    NS["IRCAdapter"] = cls
    inst = make_adapter(state, ctx, cls)
    conn_inst = object.__new__(CONN_CLS)
    holder = {}
    echo = {}
    try:
        if func == "__init__":
            inst.__init__(mat_config(args.get("config")))
            ret = None
        elif func == "connect":
            ret = asyncio.run(inst.connect(is_reconnect=args.get("is_reconnect", False)))
        elif func == "disconnect":
            ret = asyncio.run(inst.disconnect())
        elif func == "interactive_setup":
            ret = NS["interactive_setup"]()
        elif func == "info":
            ret = NS["info"](*(args.get("lines") or []))
        elif func == "_required":
            ret = NS["_required"](args.get("label"), args.get("env"),
                                  args.get("default"), args.get("what"))
        elif func == "_StandaloneConn.__init__":
            conn_inst.__init__(mat_reader(args.get("reader"), ctx),
                               mat_writer(args.get("writer"), ctx))
            ret = None
        elif func == "_on_registration":
            conn = None if args.get("conn") is None else ConnRawDouble(ctx)
            clo = REG_FACTORY(state.get("nick_base", ""), state.get("nick_attempts", 0),
                              state.get("standalone_nick", ""), conn, NS["_sa_error"])
            holder["clo"] = clo
            ret = asyncio.run(clo(args.get("cmd", "")))
        elif func == "_on_join":
            clo = JOIN_FACTORY(state.get("target", ""), NS["_sa_error"])
            holder["clo"] = clo
            ret = asyncio.run(clo(args.get("cmd", "")))
        elif func == "register":
            ctxd = None if args.get("ctx") is None else CtxDouble(ctx)
            ret = NS["register"](ctxd)
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = exc_spec(exc, ctx)
    # state echo (reflects partial mutations even on raise)
    if func in ("__init__", "connect", "disconnect"):
        echo["self"] = norm_state(inst)
    elif func == "_StandaloneConn.__init__":
        echo["conn"] = norm_state(conn_inst)
    elif func in ("_on_registration", "_on_join") and "clo" in holder:
        cv = inspect.getclosurevars(holder["clo"]).nonlocals
        if func == "_on_registration":
            echo["self"] = {k: cv[k] for k in ("nick_base", "nick_attempts", "standalone_nick")}
        else:
            echo["self"] = {"target": cv["target"]}
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(echo)}


# ── three-way seam-name cross-check ──
def cross_check():
    missing_argmap = sorted(SEAMED - set(ARGMAP) - DYNAMIC_SEAMS)
    if missing_argmap:
        print(f"FATAL oracle_irc_adapter_flow2: C seams without ARGMAP: {missing_argmap}")
        sys.exit(2)
    extra_argmap = sorted(set(ARGMAP) - SEAMED)
    if extra_argmap:
        print(f"FATAL oracle_irc_adapter_flow2: ARGMAP seams absent from C: {extra_argmap}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "irc_adapter_flow2.jsonl"
    if fix.exists():
        fix_keys, fix_funcs = set(), set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func"))
        bad = fix_keys - SEAMED - DYNAMIC_SEAMS
        if bad:
            print(f"FATAL oracle_irc_adapter_flow2: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        badf = fix_funcs - set(TARGETS)
        if badf:
            print(f"FATAL oracle_irc_adapter_flow2: fixture funcs without a target: {sorted(badf)}")
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
    print(f"ORACLE irc_adapter_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    fix = _REPO / "tests" / "fixtures" / "irc_adapter_flow2.jsonl"
    n = bad = 0
    counts = {}
    extraction_names = set(TARGETS) | SEAMED
    known_attrs = ADAPTER_STATE_ATTRS | CONN_STATE_ATTRS | CELL_ATTRS
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
                if f"'{nm}'" in msg and nm not in known_attrs:
                    bad += 1
                    print(f"SELFTEST ATTR FAMILY {func} case {n}: {msg}")
                    break
    faulthandler.cancel_dump_traceback_later()
    print(f"SELFTEST irc_adapter_flow2: {n} cases, {bad} drive/script failures")
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