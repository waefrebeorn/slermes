#!/usr/bin/env python3
"""Oracle for the tools/mcp_oauth.py UNCLAIMED-REMAINDER FLOW port
(src/tools/mcp_oauth_flow2.c).

Drives the LIVE tools/mcp_oauth.py defs with SCRIPTED I/O: every network /
file / clock / lock / logging / print call is a recording stub fed the same
scripts the C harness feeds its seams. Compares the DECISION SEQUENCE (ordered
seam names in "trace"), the return ("ret"), the mutated storage attributes
("state"), the ordered print captures ("prints") and wfile.write payloads
("writes").

Live defs are AST-extracted (module constants + RefreshFenceTimeout /
OAuthNonInteractiveError classes seeded; HermesTokenStorage methods bound to a
storage double named HermesTokenStorage so AttributeError messages match live;
the nested settle / close / _seconds defs exec'd standalone with their free
variables seeded as globals; do_GET / log_request / log_message reached through
the real _make_callback_handler factory with BaseHTTPRequestHandler=object).
Pure unclaimed siblings (_parse_redirect_query, _seconds, _announce_authorization_url,
_close_callback_server) stay REAL so nested decision logic is exercised, matching
the C port. Claimed siblings (_get_token_dir, _is_interactive,
_raise_if_non_interactive, _can_open_browser, _result_taken, _claim_result,
_make_callback_handler [wait drive], _start_callback_server, _callback_outcome,
_authorization_code_result, the storage partialmethods, get_dashboard_oauth_flow)
are SEAMED. urlparse / parse_qs / pathlib / html are the real stdlib (the C port
models their ASCII / POSIX subset, verified against live).

Deterministic threading model: the close-thread (name "mcp-oauth-cb-close")
runs close() synchronously at thread.start; loop.call_soon_threadsafe(settle)
runs settle() inline; the waiter's serve_forever / paste threads are inert.
Fail-closed: 0 cases => exit 1. Summary: 'ORACLE mcp_oauth_flow2: N cases, M mismatches'.

Deterministic subset: POSIX-style single-slash paths; ASCII URL/query fixtures;
integral floats normalized to int on both sides; tuples normalize to arrays;
type-name message positions avoid integral floats (JSON int literals read as
int live). log_message uses the %s/%r/%d/%% subset.
"""
import ast
import asyncio
import copy
import json
import math
import os
import sys
import types
import __future__
from pathlib import Path

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parents[1]
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "tools" / "mcp_oauth.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "tools" / "mcp_oauth.py")

os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

PY = {
    "stdin_is_console": "_stdin_is_console",
    "model_json": "_model_json",
    "path": "_path",
    "state_paths": "_state_paths",
    "bind_issuer": "bind_issuer",
    "credential_stamp": "credential_stamp",
    "parse_redirect_query": "_parse_redirect_query",
    "do_get": "do_GET",
    "log_request": "log_request",
    "log_message": "log_message",
    "announce_authorization_url": "_announce_authorization_url",
    "redirect_handler": "_redirect_handler",
    "close_callback_server": "_close_callback_server",
    "settle": "settle",
    "close": "close",
    "wait": "_wait",
    "seconds": "_seconds",
}

ANN_FLAG = __future__.annotations.compiler_flag


def make_exc(spec):
    t = spec.get("type", "Exception")
    msg = spec.get("message", "")
    if t == "KeyError":
        e = KeyError(spec.get("key", msg))
    elif t == "OSError":
        e = OSError(msg)
    elif t == "FileNotFoundError":
        e = FileNotFoundError(msg)
    elif t == "PermissionError":
        e = PermissionError(msg)
    elif t == "ValueError":
        e = ValueError(msg)
    elif t == "TypeError":
        e = TypeError(msg)
    elif t == "AttributeError":
        e = AttributeError(msg)
    elif t == "ImportError":
        e = ImportError(msg)
    elif t == "ModuleNotFoundError":
        e = ModuleNotFoundError(msg)
    elif t == "RuntimeError":
        e = RuntimeError(msg)
    elif t == "OAuthNonInteractiveError":
        e = NS["OAuthNonInteractiveError"](msg)
    elif t == "RefreshFenceTimeout":
        e = NS["RefreshFenceTimeout"](msg)
    elif t == "KeyboardInterrupt":
        e = KeyboardInterrupt(msg)
    elif t == "SystemExit":
        e = SystemExit(msg)
    elif t == "GeneratorExit":
        e = GeneratorExit(msg)
    elif t == "CancelledError":
        e = asyncio.CancelledError(msg)
    elif t == "Exception":
        e = Exception(msg)
    else:
        e = type(t, (Exception,), {})(msg)
    if spec.get("errno") is not None:
        e.errno = spec["errno"]
    return e


def raise_spec(e):
    spec = {"__raise__": type(e).__name__, "message": str(e)}
    errno = getattr(e, "errno", None)
    if errno is not None:
        spec["errno"] = errno
    return spec


def guarded(fn, *a, **k):
    try:
        return fn(*a, **k)
    except BaseException as e:  # noqa: BLE001 - fail-closed projection
        return raise_spec(e)


def is_raise(v):
    return isinstance(v, dict) and "__raise__" in v


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.io = inp.get("io", {}) or {}
        self.cursors = {}
        self.state = copy.deepcopy(inp.get("state", {}) or {})
        self.prints = []
        self.writes = []

    def script_next(self, name):
        entry = self.io.get(name)
        if entry is None:
            return None
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                return None
            self.cursors[name] = idx + 1
            return entry[idx]
        return entry

    def recorder(self, name):
        ctx = self

        def call(*args, **kwargs):
            ctx.trace.append(name)
            if name == "print":
                ctx.prints.append({"text": args[0] if args else None, "file": "stderr"})
            elif name == "wfile.write":
                data = args[0] if args else None
                if isinstance(data, bytes):
                    data = data.decode("utf-8")  # C models .encode() as identity on the text
                ctx.writes.append(data)
            result = ctx.script_next(name)
            if isinstance(result, dict):
                eff = result.get("state")
                if isinstance(eff, dict):
                    ctx.state.update(eff)
                raise_spec_ = result.get("raise")
                if isinstance(raise_spec_, dict):
                    raise make_exc(raise_spec_)
                ret = result.get("ret")
            else:
                ret = result
            return convert_seam_ret(name, ret, args, ctx)

        return call


# ── seam-result doubles ───────────────────────────────────────────────────
class _Await:
    def __init__(self, v):
        self.v = v

    def __await__(self):
        return self._gen()

    def _gen(self):
        if False:
            yield
        return self.v


class _Done:
    def __await__(self):
        return iter(())


class PathDouble:
    def __init__(self, posix, ctx):
        self._p = posix
        self._ctx = ctx

    def stat(self):
        return self._ctx.recorder("path.stat")(self)

    def as_posix(self):
        return self._p

    def __str__(self):
        return self._p


class ModelDouble:
    def __init__(self, marker, ctx):
        self.marker = marker
        self._ctx = ctx

    def model_dump(self, **kwargs):
        return self._ctx.recorder("model_dump")(self.marker, **kwargs)


class StdinDouble:
    def __init__(self, ctx):
        self._ctx = ctx

    def isatty(self):
        return self._ctx.recorder("sys.stdin.isatty")()

    def fileno(self):
        return self._ctx.recorder("sys.stdin.fileno")()


class WFileDouble:
    def __init__(self, ctx):
        self._ctx = ctx

    def write(self, data):
        return self._ctx.recorder("wfile.write")(data)


class HandlerClsDouble:
    def __init__(self, marker):
        self.marker = marker
        self.result_lock = object()


class DashboardDouble:
    def __init__(self, ctx):
        self._ctx = ctx

    def publish_authorization_url(self, url):
        self._ctx.recorder("dashboard_flow.publish_authorization_url")(url)
        return _Done()

    def wait_for_callback(self):
        ret = self._ctx.recorder("dashboard_flow.wait_for_callback")()
        return _Await(ret)


class ServerDouble:
    def __init__(self, sid, ctx):
        self.id = sid
        self._ctx = ctx

    def shutdown(self):
        return self._ctx.recorder("server.shutdown")(self.id)

    def server_close(self):
        return self._ctx.recorder("server.server_close")(self.id)

    def serve_forever(self, **kwargs):
        return None  # inert: the listener loop never runs in the model


class FutureDouble:
    def __init__(self, ctx):
        self._ctx = ctx

    def done(self):
        return self._ctx.recorder("future.done")()

    def set_result(self, v):
        return self._ctx.recorder("future.set_result")(v)


class LoopDouble:
    def __init__(self, marker, ctx):
        self.marker = marker
        self._ctx = ctx

    def create_future(self):
        self._ctx.recorder("loop.create_future")(self.marker)
        return FutureDouble(self._ctx)

    def call_soon_threadsafe(self, fn):
        self._ctx.recorder("loop.call_soon_threadsafe")(fn)
        return fn()  # settle runs inline in the deterministic model


class ThreadDouble:
    def __init__(self, ctx, target=None, daemon=False, name=None, kwargs=None, args=None):
        self._ctx = ctx
        self._target = target
        self._name = name
        ctx.recorder("threading.Thread")(target, daemon, name, kwargs, args)

    def start(self):
        self._ctx.recorder("thread.start")()
        if self._name == "mcp-oauth-cb-close" and self._target is not None:
            self._target()  # the close-thread runs synchronously in the model


def convert_seam_ret(name, ret, args, ctx):
    if name == "get_token_dir":
        return Path(ret) if isinstance(ret, str) else ret
    if name in ("storage._tokens_path", "storage._client_info_path", "storage._meta_path"):
        return PathDouble(ret, ctx) if isinstance(ret, str) else ret
    if name == "path.stat":
        return types.SimpleNamespace(**ret) if isinstance(ret, dict) else ret
    if name == "make_callback_handler":
        if isinstance(ret, dict):
            return (HandlerClsDouble(ret.get("handler")), ret.get("result") or {})
        return ret
    if name == "start_callback_server":
        return ServerDouble(ret, ctx) if isinstance(ret, str) else ret
    if name == "get_dashboard_oauth_flow":
        return DashboardDouble(ctx) if ret is not None else None
    if name == "asyncio.get_running_loop":
        return LoopDouble(ret, ctx) if isinstance(ret, str) else ret
    return ret


# ── AST extraction of the live module ─────────────────────────────────────
METHODS = {}
STATIC_METHODS = set()
NESTED = {}
REAL = {}


def extract():
    import contextlib
    import contextvars
    import logging
    import re as _re
    import threading
    import time as _time
    import webbrowser
    import html
    from urllib.parse import parse_qs, urlparse

    mod = types.ModuleType("tools.mcp_oauth")
    sys.modules["tools.mcp_oauth"] = mod
    ns = mod.__dict__
    ns["__name__"] = "tools.mcp_oauth"
    ns["errno"] = types.SimpleNamespace(EWOULDBLOCK=11, EAGAIN=11, EACCES=13, EDEADLK=35)
    ns["_importlib_util"] = types.SimpleNamespace(find_spec=lambda name: None)
    ns.update({
        "asyncio": asyncio, "contextlib": contextlib, "contextvars": contextvars,
        "html": html, "json": json, "logging": logging, "os": os, "re": _re,
        "socket": types.SimpleNamespace(AF_INET=2, SOCK_STREAM=1),
        "stat": types.SimpleNamespace(S_IRUSR=0o400, S_IWUSR=0o200),
        "sys": sys, "threading": threading, "time": _time, "webbrowser": webbrowser,
        "Path": Path, "parse_qs": parse_qs, "urlparse": urlparse,
        "Any": object, "TYPE_CHECKING": False,
        "BaseHTTPRequestHandler": object, "HTTPServer": object, "ThreadingMixIn": object,
        "partialmethod": lambda *a, **k: None,
        "secure_parent_dir": lambda *a, **k: None,
        "atomic_json_write": lambda *a, **k: None,
        "_contextvar_set": lambda *a, **k: None,
        "get_dashboard_oauth_flow": lambda *a, **k: None,
        "fcntl": None, "msvcrt": None, "AnyUrl": None,
    })
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)

    def cexec(node, target_ns=None):
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=ANN_FLAG),
             target_ns if target_ns is not None else ns)

    # module constants (Assign + AnnAssign, plain-Name targets only)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
            if not isinstance(tgt, ast.Name):
                continue
            try:
                cexec(node)
            except Exception:
                pass
    # small exception classes (real, so raise / type names match)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in ("RefreshFenceTimeout", "OAuthNonInteractiveError"):
            cexec(node)
    # module-level functions (real)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                cexec(node)
            except Exception:
                pass
    # HermesTokenStorage methods (strip decorators; record staticmethods)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "HermesTokenStorage":
            for sub in node.body:
                if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)):
                    is_static = any(
                        (isinstance(d, ast.Name) and d.id == "staticmethod") for d in sub.decorator_list
                    )
                    sub.decorator_list = []
                    try:
                        cexec(sub)
                        METHODS[sub.name] = ns[sub.name]
                        if is_static:
                            STATIC_METHODS.add(sub.name)
                    except Exception:
                        pass
    # nested defs: settle / close (free vars seeded per case), _seconds (pure)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            if node.name == "_close_callback_server":
                for sub in node.body:
                    if isinstance(sub, ast.FunctionDef) and sub.name in ("settle", "close"):
                        NESTED[sub.name] = sub
            elif node.name == "login_connect_timeout":
                for sub in node.body:
                    if isinstance(sub, ast.FunctionDef) and sub.name == "_seconds":
                        NESTED["_seconds"] = sub
    # logger is an Assign that replaced any seeded recorder; per-case re-set anyway
    ns["logger"] = logging.getLogger("tools.mcp_oauth")
    # do_GET/log_request/log_message live in the _Handler class inside the real
    # _make_callback_handler factory; _redirect_handler/_wait are nested in their
    # factories — all reached by driving the factory, not bound in ns.
    via_factory = {"do_GET", "log_request", "log_message", "_redirect_handler", "_wait"}
    for name in PY.values():
        if name in via_factory:
            continue
        if name not in ns and name not in METHODS and name not in NESTED:
            print(f"FATAL mcp_oauth_flow2: extraction missing {name}")
            sys.exit(2)
    for name in ("_parse_redirect_query", "_announce_authorization_url",
                 "_close_callback_server", "_make_callback_handler",
                 "_make_redirect_handler", "_make_callback_waiter"):
        REAL[name] = ns.get(name)
    return ns


NS = extract()


class HermesTokenStorage:
    """Storage double named exactly like the live class so AttributeError
    messages match ('HermesTokenStorage' object has no attribute ...)."""

    def __init__(self, ctx, seams):
        object.__setattr__(self, "_ctx", ctx)
        object.__setattr__(self, "_seams", set(seams))

    def __getattr__(self, name):
        ctx = object.__getattribute__(self, "_ctx")
        seams = object.__getattribute__(self, "_seams")
        if name in seams:
            return ctx.recorder("storage." + name)
        if name in METHODS and name not in STATIC_METHODS:
            return types.MethodType(METHODS[name], self)
        if name in ctx.state:
            return ctx.state[name]
        raise AttributeError(f"'HermesTokenStorage' object has no attribute '{name}'")

    def __setattr__(self, name, value):
        object.__getattribute__(self, "_ctx").state[name] = value


class _Logger:
    def __init__(self, ctx):
        self._ctx = ctx

    def __getattr__(self, level):
        return self._ctx.recorder("logger." + level)


def seed_case(ctx, inp, func):
    rec = ctx.recorder
    NS["logger"] = _Logger(ctx)
    NS["print"] = lambda *a, **k: rec("print")(*a, **k)
    platform = inp.get("platform", "posix")
    NS["os"] = types.SimpleNamespace(name=platform, getenv=rec("os.getenv"))
    NS["webbrowser"] = types.SimpleNamespace(open=rec("webbrowser.open"))
    NS["html"] = __import__("html")
    from urllib.parse import parse_qs, urlparse
    NS["urlparse"] = urlparse
    NS["parse_qs"] = parse_qs
    NS["contextlib"] = __import__("contextlib")
    NS["threading"] = types.SimpleNamespace(
        Thread=lambda *a, **k: ThreadDouble(ctx, *a, **k),
        Lock=__import__("threading").Lock)
    NS["asyncio"] = types.SimpleNamespace(
        sleep=lambda s: (rec("asyncio.sleep")(s), _Done())[1],
        get_running_loop=rec("asyncio.get_running_loop"),
        shield=lambda f: (rec("asyncio.shield")(f), _Done())[1],
        CancelledError=asyncio.CancelledError)
    NS["sys"] = types.SimpleNamespace(stdin=None, stderr=object())
    # claimed siblings -> seams
    NS["_can_open_browser"] = rec("can_open_browser")
    NS["_raise_if_non_interactive"] = rec("raise_if_non_interactive")
    NS["_is_interactive"] = rec("is_interactive")
    NS["_result_taken"] = rec("result_taken")
    NS["_claim_result"] = rec("claim_result")
    NS["_start_callback_server"] = rec("start_callback_server")
    NS["_callback_outcome"] = rec("callback_outcome")
    NS["_authorization_code_result"] = rec("authorization_code_result")
    NS["_get_token_dir"] = rec("get_token_dir")
    NS["get_dashboard_oauth_flow"] = rec("get_dashboard_oauth_flow")
    # pure unclaimed siblings stay REAL (composed flows exercise them)
    for name in ("_parse_redirect_query", "_announce_authorization_url",
                 "_close_callback_server", "_make_redirect_handler", "_make_callback_waiter"):
        if REAL.get(name) is not None:
            NS[name] = REAL[name]
    if func in ("do_get", "log_request", "log_message"):
        NS["_make_callback_handler"] = REAL["_make_callback_handler"]
    else:
        NS["_make_callback_handler"] = rec("make_callback_handler")
    if func == "stdin_is_console":
        stdin = inp.get("stdin")
        NS["sys"] = types.SimpleNamespace(
            stdin=StdinDouble(ctx) if stdin is not None else None, stderr=object())


def drive_coro(coro):
    try:
        while True:
            coro.send(None)
    except StopIteration as si:
        return si.value
    except BaseException as e:  # noqa: BLE001
        return raise_spec(e)


def make_handler(ctx, inp):
    handler_cls, result = NS["_make_callback_handler"]()
    h = handler_cls()
    h.path = inp.get("path", "")
    if "command" in inp:
        h.command = inp["command"]
    h.send_response = ctx.recorder("send_response")
    h.send_header = ctx.recorder("send_header")
    h.end_headers = ctx.recorder("end_headers")
    h.wfile = WFileDouble(ctx)
    return h


def run_nested(func, ctx, inp):
    """Drive the extracted settle / close defs with free variables seeded."""
    ns = {"logger": _Logger(ctx)}
    ns["closed"] = FutureDouble(ctx)
    ns["loop"] = LoopDouble(inp.get("loop", "loop1"), ctx)
    ns["server"] = ServerDouble(inp.get("server", "srv1"), ctx)
    exec(compile(ast.Module(body=[NESTED["settle"]], type_ignores=[]), SRC, "exec", flags=ANN_FLAG), ns)
    if func == "close":
        exec(compile(ast.Module(body=[NESTED["close"]], type_ignores=[]), SRC, "exec", flags=ANN_FLAG), ns)
        return guarded(ns["close"])
    return guarded(ns["settle"])


def run_case(func, inp):
    ctx = CaseCtx(inp)
    seed_case(ctx, inp, func)
    out = {"ret": None, "trace": ctx.trace}
    state = ctx.state

    if func == "stdin_is_console":
        saved = {}
        platform = inp.get("platform", "posix")
        if platform == "nt":
            for modname in ("ctypes", "msvcrt"):
                saved[modname] = sys.modules.get(modname)
            if inp.get("ctypes_missing"):
                sys.modules["ctypes"] = None
            else:
                ct = types.ModuleType("ctypes")
                ct.c_ulong = ctx.recorder("ctypes.c_ulong")
                ct.c_void_p = ctx.recorder("ctypes.c_void_p")
                ct.byref = ctx.recorder("ctypes.byref")
                ct.windll = types.SimpleNamespace(
                    kernel32=types.SimpleNamespace(
                        GetConsoleMode=ctx.recorder("ctypes.windll.kernel32.GetConsoleMode")))
                sys.modules["ctypes"] = ct
            if inp.get("msvcrt_missing"):
                sys.modules["msvcrt"] = None
            else:
                ms = types.ModuleType("msvcrt")
                ms.get_osfhandle = ctx.recorder("msvcrt.get_osfhandle")
                sys.modules["msvcrt"] = ms
        try:
            out["ret"] = guarded(NS["_stdin_is_console"])
        finally:
            for modname, prior in saved.items():
                if prior is None:
                    sys.modules.pop(modname, None)
                else:
                    sys.modules[modname] = prior
    elif func == "model_json":
        model = inp.get("model")
        if isinstance(model, str):
            model = ModelDouble(model, ctx)
        out["ret"] = guarded(NS["_model_json"], model)
    elif func == "path":
        slf = HermesTokenStorage(ctx, set())
        r = guarded(METHODS["_path"], slf, inp.get("suffix", ".json"))
        out["ret"] = r.as_posix() if isinstance(r, Path) else r
    elif func == "state_paths":
        slf = HermesTokenStorage(ctx, {"_tokens_path", "_client_info_path", "_meta_path"})
        r = guarded(METHODS["_state_paths"], slf)
        out["ret"] = [x.as_posix() if isinstance(x, PathDouble) else x for x in r] \
            if isinstance(r, tuple) else r
    elif func == "bind_issuer":
        slf = HermesTokenStorage(ctx, set())
        out["ret"] = guarded(METHODS["bind_issuer"], slf, inp.get("issuer"))
    elif func == "credential_stamp":
        slf = HermesTokenStorage(ctx, {"_tokens_path", "_client_info_path"})
        out["ret"] = guarded(METHODS["credential_stamp"], slf)
    elif func == "parse_redirect_query":
        out["ret"] = guarded(NS["_parse_redirect_query"], inp.get("query", ""))
    elif func == "do_get":
        h = make_handler(ctx, inp)
        out["ret"] = guarded(h.do_GET)
    elif func == "log_request":
        h = make_handler(ctx, inp)
        out["ret"] = guarded(h.log_request)
    elif func == "log_message":
        h = make_handler(ctx, inp)
        out["ret"] = guarded(h.log_message, inp.get("fmt", ""), *(inp.get("args") or []))
    elif func == "announce_authorization_url":
        out["ret"] = guarded(NS["_announce_authorization_url"],
                             inp.get("authorization_url", ""), inp.get("port", 0),
                             inp.get("redirect_uri"), inp.get("redirect_host"))
    elif func == "redirect_handler":
        factory = NS["_make_redirect_handler"]
        coro_fn = factory(inp.get("port", 0), inp.get("redirect_uri"), inp.get("redirect_host"))
        out["ret"] = drive_coro(coro_fn(inp.get("authorization_url", "")))
    elif func == "close_callback_server":
        srv = ServerDouble(inp.get("server", "srv1"), ctx)
        out["ret"] = drive_coro(NS["_close_callback_server"](srv))
    elif func in ("settle", "close"):
        out["ret"] = run_nested(func, ctx, inp)
    elif func == "wait":
        factory = NS["_make_callback_waiter"]
        coro_fn = factory(inp.get("port", 0), inp.get("cimd_url"), inp.get("timeout", 300.0))
        out["ret"] = drive_coro(coro_fn())
    elif func == "seconds":
        ns2 = {}
        exec(compile(ast.Module(body=[NESTED["_seconds"]], type_ignores=[]), SRC, "exec", flags=ANN_FLAG), ns2)
        out["ret"] = guarded(ns2["_seconds"], inp.get("value"), inp.get("default", 0.0))
    else:
        raise KeyError(func)

    out["state"] = state
    out["prints"] = ctx.prints
    out["writes"] = ctx.writes
    return out


def jnorm(v):
    if isinstance(v, Path):
        return v.as_posix()
    if isinstance(v, PathDouble):
        return v._p
    if isinstance(v, bool):
        return v
    if isinstance(v, float):
        if math.isfinite(v) and v == int(v) and abs(v) < 1e15:
            return int(v)
        return v
    if isinstance(v, tuple):
        return [jnorm(x) for x in v]
    if isinstance(v, list):
        return [jnorm(x) for x in v]
    if isinstance(v, dict):
        return {k: jnorm(x) for k, x in v.items()}
    if isinstance(v, set):
        return sorted((jnorm(x) for x in v), key=lambda z: json.dumps(z, sort_keys=True, default=str))
    return v


def main():
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        cases += 1
        func = c.get("func", "")
        if func not in PY:
            mism += 1
            print(f"PYERR {func}: unknown func")
            continue
        try:
            want = run_case(func, c.get("in") or {})
        except BaseException as e:  # noqa: BLE001 - SystemExit must not kill the loop
            mism += 1
            print(f"PYERR {func}: {type(e).__name__}: {e}")
            continue
        got = c.get("out") or {}
        g = jnorm(got)
        w = jnorm(want)
        ok = (g.get("ret") == w.get("ret")
              and g.get("trace") == w.get("trace")
              and g.get("state") == w.get("state")
              and g.get("prints") == w.get("prints")
              and g.get("writes") == w.get("writes"))
        if not ok:
            mism += 1
            print(f"MISMATCH {func}")
            print(f"  C  ={json.dumps(g, sort_keys=True, default=str)}")
            print(f"  PY ={json.dumps(w, sort_keys=True, default=str)}")
    print(f"ORACLE mcp_oauth_flow2: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())