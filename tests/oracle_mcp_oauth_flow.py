#!/usr/bin/env python3
"""Oracle for the tools/mcp_oauth.py FLOW + FUNCTION port
(src/tools/mcp_oauth_flow.c).

Drives the LIVE tools/mcp_oauth.py OAuth defs with SCRIPTED I/O: every network /
file / clock / lock / logging call is a recording stub fed the same scripts the C
harness feeds its seams. Compares the DECISION SEQUENCE (ordered seam names in
"trace"), the return ("ret"), the mutated module globals / storage attributes
("state"), and the mutated dict ("data"/"result") for the mutator flows.

Live defs are AST-extracted (module constants + RefreshFenceTimeout /
OAuthNonInteractiveError classes seeded; HermesTokenStorage methods extracted and
bound to a storage double). Pure siblings (_refresh_lock_path, _cached_client_info,
_result_taken, _rebase_expires_in) stay REAL so nested decision logic is exercised,
matching the C port. urlparse / pathlib are the real stdlib (the C port models their
ASCII / POSIX subset, verified against live). Fail-closed: 0 cases => exit 1.
Summary: 'ORACLE mcp_oauth_flow: N cases, M mismatches'.

Deterministic subset: errno pinned to Linux constants (EWOULDBLOCK=EAGAIN=11,
EACCES=13, EDEADLK=35) via an errno double; POSIX-style single-slash paths; ASCII
URLs; container str() reprs excluded from redirect fixtures. Integral floats are
normalized to int on both sides; tuples normalize to arrays.
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
    "refresh_lock_path": "_refresh_lock_path",
    "acquire_refresh_fence": "acquire_refresh_fence",
    "release_refresh_fence": "release_refresh_fence",
    "sdk_class": "_sdk_class",
    "bind_reserved": "_bind_reserved",
    "cached_client_info": "_cached_client_info",
    "cached_redirect": "_cached_redirect",
    "load_model": "_load_model",
    "rebase_expires_in": "_rebase_expires_in",
    "fixup_loaded_tokens": "_fixup_loaded_tokens",
    "stamp_issuer": "stamp_issuer",
    "strip_refresh_token": "strip_refresh_token",
    "coerce_secret_auth_method": "_coerce_secret_auth_method",
    "result_taken": "_result_taken",
    "claim_result": "_claim_result",
    "start_callback_server": "_start_callback_server",
    "callback_outcome": "_callback_outcome",
    "login_connect_timeout": "login_connect_timeout",
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
    elif t == "KeyboardInterrupt":
        e = KeyboardInterrupt(msg)
    elif t == "SystemExit":
        e = SystemExit(msg)
    elif t == "CancelledError":
        e = asyncio.CancelledError(msg)
    elif t == "Exception":
        e = Exception(msg)
    else:
        e = type(t, (Exception,), {})(msg)
    if spec.get("errno") is not None:
        e.errno = spec["errno"]
    if "errors" in spec:
        e.errors = lambda include_input=True: spec["errors"]
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
class SockDouble:
    def __init__(self, sid, prefix, ctx):
        self.id = sid
        self._prefix = prefix
        self._ctx = ctx

    def bind(self, addr):
        return self._ctx.recorder(self._prefix + ".bind")(self, addr)

    def close(self):
        return self._ctx.recorder(self._prefix + ".close")(self)

    def getsockname(self):
        return self._ctx.recorder(self._prefix + ".getsockname")(self)


class PathDouble:
    def __init__(self, posix, ctx):
        self._p = posix
        self._ctx = ctx

    def stat(self):
        # convert_seam_ret already builds SimpleNamespace(st_mtime=...)
        return self._ctx.recorder("path.stat")(self)

    def as_posix(self):
        return self._p

    def __str__(self):
        return self._p


class ClsDouble:
    def __init__(self, marker, ctx):
        self.marker = marker
        self._ctx = ctx

    def model_validate(self, data):
        return self._ctx.recorder("model_validate")(self.marker, data)


class LockDouble:
    def __init__(self, ctx):
        self._ctx = ctx

    def __enter__(self):
        self._ctx.recorder("lock.__enter__")()
        return self

    def __exit__(self, *a):
        self._ctx.recorder("lock.__exit__")(*a)
        return False


class _Done:
    def __await__(self):
        return iter(())


class _ImportMod(types.ModuleType):
    def __init__(self, name, ctx, seam, attr="auth"):
        super().__init__(name)
        self._ctx = ctx
        self._seam = seam
        self._attr = attr

    def __getattr__(self, nm):
        if nm == self._attr:
            return self._ctx.recorder(self._seam)()
        raise AttributeError(nm)


def convert_seam_ret(name, ret, args, ctx):
    if name == "socket.socket":
        return SockDouble(ret, "sock", ctx) if isinstance(ret, str) else ret
    if name in ("storage._tokens_path", "storage._client_info_path"):
        return PathDouble(ret, ctx) if isinstance(ret, str) else ret
    if name == "path.stat":
        return types.SimpleNamespace(st_mtime=ret["st_mtime"]) if isinstance(ret, dict) else ret
    if name in ("_sdk_class", "import_device_metadata"):
        return ClsDouble(ret, ctx) if ret is not None else None
    if name in ("import_client_auth", "import_shared_auth"):
        return types.SimpleNamespace(**ret) if isinstance(ret, dict) else ret
    return ret


def make_srv_class(ctx):
    class _SrvDouble:
        def __init__(self, addr, handler, bind_and_activate=False):
            r = ctx.recorder("CallbackServer")(addr, handler, bind_and_activate)
            self._id = r["server"]
            self.socket = SockDouble(r["socket"], "server.socket", ctx)
            self.server_address = addr
            self.allow_reuse_address = True  # HTTPServer.allow_reuse_address = 1

        def server_bind(self):
            return ctx.recorder("server.server_bind")(self._id)

        def server_activate(self):
            return ctx.recorder("server.server_activate")(self._id)

    return _SrvDouble


# ── AST extraction of the live module ─────────────────────────────────────
METHODS = {}
STATIC_METHODS = set()
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
    # errno pinned to Linux constants so _FENCE_CONTENTION_ERRNOS is host-independent
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
    })
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)

    def cexec(node):
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=ANN_FLAG), ns)

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
    # logger is an Assign that replaced any seeded recorder; per-case re-set anyway
    ns["logger"] = logging.getLogger("tools.mcp_oauth")
    for name in PY.values():
        if name not in ns and name not in METHODS:
            print(f"FATAL mcp_oauth_flow: extraction missing {name}")
            sys.exit(2)
    for name in ("_read_json", "_write_json", "_sdk_class", "_authorization_code_result",
                 "_cached_client_info", "_result_taken", "_refresh_lock_path", "_rebase_expires_in"):
        REAL[name] = ns.get(name)
    return ns


NS = extract()


class StorageDouble:
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
        raise AttributeError(name)

    def __setattr__(self, name, value):
        object.__getattribute__(self, "_ctx").state[name] = value


def mk_storage(ctx, inp, seams):
    s = inp.get("storage")
    if s is None:
        return None
    if isinstance(s, dict) and s.get("broken"):
        return object()  # lacks _client_info_path -> AttributeError
    return StorageDouble(ctx, seams)


class _Logger:
    def __init__(self, ctx):
        self._ctx = ctx

    def __getattr__(self, level):
        return self._ctx.recorder("logger." + level)


def seed_case(ctx, inp, func):
    rec = ctx.recorder
    NS["logger"] = _Logger(ctx)
    NS["time"] = types.SimpleNamespace(time=rec("time.time"), monotonic=rec("time.monotonic"))
    NS["os"] = types.SimpleNamespace(open=rec("os.open"), close=rec("os.close"),
                                     O_RDWR=2, O_CREAT=64, name="posix")
    NS["asyncio"] = types.SimpleNamespace(sleep=lambda s: (rec("asyncio.sleep")(s), _Done())[1])
    NS["socket"] = types.SimpleNamespace(socket=lambda af, ty: rec("socket.socket")(af, ty),
                                         AF_INET=2, SOCK_STREAM=1)
    NS["secure_parent_dir"] = rec("secure_parent_dir")
    platform = inp.get("platform", "posix")
    if platform == "posix":
        NS["fcntl"] = types.SimpleNamespace(flock=rec("fcntl.flock"), LOCK_EX=2, LOCK_NB=4, LOCK_UN=8)
        NS["msvcrt"] = None
    elif platform == "windows":
        NS["fcntl"] = None
        NS["msvcrt"] = types.SimpleNamespace(locking=rec("msvcrt.locking"), LK_NBLCK=2, LK_UNLCK=0)
    else:
        NS["fcntl"] = None
        NS["msvcrt"] = None
    # restore real siblings, then install module I/O seams
    for name, fn in REAL.items():
        if fn is not None:
            NS[name] = fn
    NS["_read_json"] = rec("_read_json")
    NS["_write_json"] = rec("_write_json")
    # hermes_constants stub (acquire imports mkdir_under_hermes_home in-body)
    hc = types.ModuleType("hermes_constants")
    hc.mkdir_under_hermes_home = rec("mkdir_under_hermes_home")
    hc.secure_parent_dir = rec("secure_parent_dir")
    hc.get_hermes_home = lambda: "/h"
    sys.modules["hermes_constants"] = hc
    # mcp SDK import doubles
    mcp = types.ModuleType("mcp")
    mcp.__path__ = []
    client = _ImportMod("mcp.client", ctx, "import_client_auth")
    shared = _ImportMod("mcp.shared", ctx, "import_shared_auth")
    mcp.client = client
    mcp.shared = shared
    sys.modules["mcp"] = mcp
    sys.modules["mcp.client"] = client
    sys.modules["mcp.shared"] = shared
    # device-metadata import double
    tools = types.ModuleType("tools")
    tools.__path__ = []
    dev = _ImportMod("tools.mcp_oauth_device", ctx, "import_device_metadata", attr="DeviceOAuthMetadata")
    sys.modules["tools"] = tools
    sys.modules["tools.mcp_oauth_device"] = dev

    state = ctx.state
    if func == "sdk_class":
        state.setdefault("_SDK_CLASSES", {})
        state.setdefault("_OAUTH_AVAILABLE", True)
        NS["_SDK_CLASSES"] = state["_SDK_CLASSES"]
        NS["_OAUTH_AVAILABLE"] = state["_OAUTH_AVAILABLE"]
    if func in ("bind_reserved", "start_callback_server"):
        state.setdefault("_reserved_sockets", {})
        prefix = "sock" if func == "bind_reserved" else "reserved"
        NS["_reserved_sockets"] = {int(k): SockDouble(v, prefix, ctx)
                                   for k, v in state["_reserved_sockets"].items()}
    if func == "load_model":
        NS["_sdk_class"] = rec("_sdk_class")
    if func == "callback_outcome":
        NS["_authorization_code_result"] = rec("authorization_code_result")
    if func == "start_callback_server":
        NS["_CallbackServer"] = make_srv_class(ctx)


def drive_coro(coro):
    try:
        while True:
            coro.send(None)
    except StopIteration as si:
        return si.value
    except BaseException as e:  # noqa: BLE001
        return raise_spec(e)


def dump_reserved():
    return {str(p): s.id for p, s in NS["_reserved_sockets"].items()}


def run_case(func, inp):
    ctx = CaseCtx(inp)
    seed_case(ctx, inp, func)
    out = {"ret": None, "trace": ctx.trace}
    state = ctx.state

    if func == "refresh_lock_path":
        r = guarded(NS["_refresh_lock_path"], Path(inp.get("path", "")))
        out["ret"] = r.as_posix() if isinstance(r, Path) else r
    elif func == "acquire_refresh_fence":
        coro = NS["acquire_refresh_fence"](Path(inp.get("path", "")),
                                           timeout=inp.get("timeout", 60.0))
        out["ret"] = drive_coro(coro)
    elif func == "release_refresh_fence":
        out["ret"] = guarded(NS["release_refresh_fence"], inp.get("fd", 0))
    elif func == "sdk_class":
        out["ret"] = guarded(NS["_sdk_class"], inp.get("name", ""))
        state["_OAUTH_AVAILABLE"] = NS["_OAUTH_AVAILABLE"]
    elif func == "bind_reserved":
        out["ret"] = guarded(NS["_bind_reserved"], inp.get("port", 0))
        state["_reserved_sockets"] = dump_reserved()
    elif func == "cached_client_info":
        out["ret"] = guarded(NS["_cached_client_info"], mk_storage(ctx, inp, {"_client_info_path"}))
    elif func == "cached_redirect":
        r = guarded(NS["_cached_redirect"], mk_storage(ctx, inp, {"_client_info_path"}))
        out["ret"] = list(r) if isinstance(r, tuple) else r
    elif func == "load_model":
        fixup = ctx.recorder("fixup") if inp.get("fixup") else None
        out["ret"] = guarded(NS["_load_model"], Path(inp.get("path", "")),
                             inp.get("sdk_name", "OAuthToken"), inp.get("label", "tokens"), fixup)
    elif func == "rebase_expires_in":
        data = copy.deepcopy(inp.get("data") or {})
        slf = StorageDouble(ctx, {"_tokens_path"})
        out["ret"] = guarded(METHODS["_rebase_expires_in"], slf, data)
        out["data"] = data
    elif func == "fixup_loaded_tokens":
        data = copy.deepcopy(inp.get("data") or {})
        slf = StorageDouble(ctx, {"_tokens_path"})
        out["ret"] = guarded(METHODS["_fixup_loaded_tokens"], slf, data)
        out["data"] = data
    elif func == "stamp_issuer":
        slf = StorageDouble(ctx, {"_tokens_path"})
        out["ret"] = guarded(METHODS["stamp_issuer"], slf, inp.get("issuer", ""))
    elif func == "strip_refresh_token":
        slf = StorageDouble(ctx, {"_tokens_path"})
        out["ret"] = guarded(METHODS["strip_refresh_token"], slf)
    elif func == "coerce_secret_auth_method":
        data = copy.deepcopy(inp.get("data") or {})
        out["ret"] = guarded(METHODS["_coerce_secret_auth_method"], data)
        out["data"] = data
    elif func == "result_taken":
        out["ret"] = guarded(NS["_result_taken"], inp.get("result") or {})
    elif func == "claim_result":
        result = copy.deepcopy(inp.get("result") or {})
        out["ret"] = guarded(NS["_claim_result"], result, LockDouble(ctx), **(inp.get("values") or {}))
        out["result"] = result
    elif func == "start_callback_server":
        srv = guarded(NS["_start_callback_server"], inp.get("port", 0), inp.get("handler", "H"))
        if is_raise(srv):
            out["ret"] = srv
        else:
            out["ret"] = {"server": srv._id, "socket": srv.socket.id,
                          "server_address": list(srv.server_address),
                          "allow_reuse_address": bool(srv.allow_reuse_address)}
        state["_reserved_sockets"] = dump_reserved()
    elif func == "callback_outcome":
        out["ret"] = guarded(NS["_callback_outcome"], inp.get("result") or {}, inp.get("cimd_url"))
    elif func == "login_connect_timeout":
        out["ret"] = guarded(NS["login_connect_timeout"], inp.get("config") or {})
    else:
        raise KeyError(func)

    out["state"] = state
    return out


def jnorm(v):
    if isinstance(v, Path):
        return v.as_posix()
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
              and g.get("state") == w.get("state"))
        for k in ("data", "result"):
            if k in w:
                ok = ok and g.get(k) == w.get(k)
        if not ok:
            mism += 1
            print(f"MISMATCH {func}")
            print(f"  C  ={json.dumps(g, sort_keys=True, default=str)}")
            print(f"  PY ={json.dumps(w, sort_keys=True, default=str)}")
    print(f"ORACLE mcp_oauth_flow: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
