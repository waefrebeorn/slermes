#!/usr/bin/env python3
"""Oracle for the Photon dashboard auth FLOW port (photon_auth_flow2).

Drives the LIVE plugins/platforms/photon/auth.py module functions with SCRIPTED
seams: every foreign / stdlib / in-function-import operation is a recording
stub fed the same scripts the C harness feeds its universal pa2_call.  Compares
the DECISION SEQUENCE (ordered {"name","args","ret"} seam events in "trace"),
the final return ("ret") and the state echo ("state", always {} here).

The live defs are AST-extracted from auth.py (module constants and dataclasses
seeded; PhotonDashboardAuthError exec'd as a real RuntimeError subclass).
Every exec namespace is a registered sys.modules module (py3.14).  The heavy
quarry import chain is never imported; in-function imports resolve to per-case
stub modules.

All 40 ported functions are module-level functions driven directly.  Logger
calls are SILENT on both sides.  Fail-closed: missing defs or 0 cases => exit 2.
Per-case wrapper catches BaseException (SystemExit included); scripted raises
round-trip their spec.  Summary: 'ORACLE photon_auth_flow2: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null pointer; a raise is {"__raise__":cls,"message":...,...}.
- Paths project to POSIX strings; tuples/dataclasses to JSON arrays/objects.
- Response doubles project to their backing dict; file/path doubles are helpers.
- Integral floats == ints (canon folds); dict comparison order-insensitive;
  trace event LIST order is semantic.
- config_available / httpx_available are per-case CONFIG args (default true),
  NOT traced seams (they gate in-function imports / httpx presence).
- Fixture corpus restrictions are documented in the C port header.
"""
import ast
import copy
import dataclasses
import json
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from pathlib import PurePosixPath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = __import__("pathlib").Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "plugins", "platforms", "photon", "auth.py")
_FLAGS = _fut.annotations.compiler_flag

MODULE_TARGETS = [
    "_bearer", "_basic", "_require_httpx", "_dashboard_host", "_spectrum_host",
    "_clean_bearer_token", "_header_value", "_normalize_phone",
    "_raise_on_error_key", "_unwrap_list", "user_assigned_line", "user_opted_in",
    "_get_config_env_value", "_configured_operator_phone", "_safe",
    "_response_error_detail", "_raise_for_status", "_pool_first",
    "_auth_json_path", "_load_auth", "_save_auth", "_store_pool_record",
    "load_photon_token", "store_photon_token", "clear_photon_token",
    "load_project_credentials", "load_dashboard_project_id",
    "store_project_credentials", "store_user_numbers", "_persist_runtime_env",
    "_device_response_token_candidates", "request_device_code",
    "check_photon_token_valid", "validate_photon_token",
    "_validated_dashboard_token", "_dashboard_get", "_dashboard_post",
    "_dashboard_list", "list_projects", "find_project_by_name",
]
TARGETS = MODULE_TARGETS
SEAM_NAMES = [
    "auth_store_lock", "config.get_env_value", "config.save_env_value",
    "get_hermes_home", "get_scoped_secret", "httpx.get", "httpx.post",
    "json.dump", "json.load", "os.close", "os.fdopen", "os.fsync",
    "os.getenv", "os.getpid", "os.open", "path.exists", "path.mkdir",
    "path.replace", "path.unlink", "resp.json", "resp.raise_for_status",
    "safe_call", "time.time", "uuid4_hex",
]
SEAMED = set(SEAM_NAMES)


def seg(src, node):
    return "\n".join(src.splitlines()[node.lineno - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    start = node.lineno
    if isinstance(node, ast.ClassDef):
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


def _seed_constants(src, tree, ns):
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass


# stub modules for in-function imports
HC_MOD = new_module("hermes_constants")
CLI_MOD = new_module("hermes_cli")
CLI_MOD.__path__ = []
AUTH_MOD = new_module("hermes_cli.auth")
CFG_MOD = new_module("hermes_cli.config")
CLI_MOD.auth = AUTH_MOD
CLI_MOD.config = CFG_MOD


def extract():
    mod = new_module("pa2_oracle_auth")
    ns = mod.__dict__
    import logging
    import contextlib
    import uuid
    import stat
    from base64 import b64encode
    from dataclasses import dataclass, field
    from typing import Any, Optional, Callable, Dict, List, Tuple
    ns.update({
        "re": re, "json": json, "logging": logging, "contextlib": contextlib,
        "uuid": uuid, "stat": stat, "dataclass": dataclass, "field": field,
        "b64encode": b64encode,
        "Any": Any, "Optional": Optional, "Callable": Callable,
        "Dict": Dict, "List": List, "Tuple": Tuple,
        "logger": _SilentLogger(), "Path": PurePosixPath,
    })
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # real classes from source (custom exception + dataclasses)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in (
                "PhotonDashboardAuthError", "DeviceCode", "_DeviceTokenCandidate"):
            exec_node(src, node, ns, SRC)
    _seed_constants(src, tree, ns)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in MODULE_TARGETS:
            exec_node(src, node, ns, SRC)
    _seed_constants(src, tree, ns)
    ns["logger"] = _SilentLogger()
    required = TARGETS + [
        "PhotonDashboardAuthError", "DeviceCode", "_DeviceTokenCandidate",
        "DEFAULT_CLIENT_ID", "DEFAULT_SCOPE", "DEFAULT_DASHBOARD_HOST",
        "DEFAULT_SPECTRUM_HOST", "DEFAULT_PROJECT_NAME", "DEFAULT_POLL_INTERVAL",
        "DEFAULT_POLL_TIMEOUT", "E164_RE",
    ]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_photon_auth_flow2: auth missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()


# ── path / file / response / CM doubles ──
def _join(base, comp):
    if not base:
        return comp or ""
    # PurePosixPath preserves EXACTLY two leading slashes ("//" + x == "//x").
    if base.startswith("//") and not base.startswith("///"):
        return base + ("" if base.endswith("/") else "/") + (comp or "")
    return base.rstrip("/") + "/" + (comp or "")


def _parent(path):
    if "/" not in path:
        return ""
    p = path.rfind("/")
    if p == 0:
        return "//" if path.startswith("//") and len(path) > 2 and path[2] == "/" else "/"
    return path[:p]


def _name(path):
    return path.rsplit("/", 1)[-1] if path else ""


class FakePath:
    def __init__(self, s, ctx=None, encoding=None):
        self.s = s or ""
        self.ctx = ctx
        self.encoding = encoding

    def __truediv__(self, comp):
        return FakePath(_join(self.s, str(comp)), self.ctx)

    def exists(self):
        return self.ctx.call("path.exists", {"path": str(self)})

    @property
    def parent(self):
        return FakePath(_parent(self.s), self.ctx)

    @property
    def name(self):
        return _name(self.s)

    def with_name(self, n):
        return FakePath(_join(_parent(self.s), str(n)), self.ctx)

    def mkdir(self, parents=False, exist_ok=False):
        return self.ctx.call("path.mkdir", {"path": str(self), "parents": parents, "exist_ok": exist_ok})

    def open(self, mode="r", encoding=None):
        return FileDouble(self, encoding)

    def unlink(self):
        return self.ctx.call("path.unlink", {"path": str(self)})

    def replace(self, dst):
        return self.ctx.call("path.replace", {"src": str(self), "dst": str(dst)})

    def __str__(self):
        return self.s


class FileDouble:
    def __init__(self, path, encoding=None, fd=None):
        self.path = path
        self.encoding = encoding
        self.fd = fd

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False

    def flush(self):
        return None

    def fileno(self):
        return self.fd


class RespDouble:
    def __init__(self, d, ctx):
        self.d = d if isinstance(d, dict) else {}
        self.ctx = ctx

    @property
    def status_code(self):
        return self.d.get("status_code", 200)

    @property
    def text(self):
        return self.d.get("text", "") or ""

    @property
    def headers(self):
        return self.d.get("headers") or {}

    def raise_for_status(self):
        return self.ctx.call("resp.raise_for_status", {})

    def json(self):
        return self.ctx.call("resp.json", {})


class CMDouble:
    def __init__(self, ctx, name):
        self.ctx = ctx
        self.name = name

    def __enter__(self):
        self.ctx.call(self.name, {})
        return self

    def __exit__(self, *a):
        return False


# ── canonical projection ──
def canon(v):
    def conv(x):
        if isinstance(x, RespDouble):
            return conv(x.d)
        if isinstance(x, FakePath):
            return str(x)
        if isinstance(x, PurePosixPath):
            return str(x)
        if dataclasses.is_dataclass(x) and not isinstance(x, type):
            return conv(dataclasses.asdict(x))
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, BaseException):
            return {"__raise__": type(x).__name__, "message": str(x)}
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception materialization ──
BUILTIN = {
    "Exception": Exception, "BaseException": BaseException, "OSError": OSError,
    "TimeoutError": TimeoutError, "asyncio.TimeoutError": TimeoutError,
    "ConnectionError": ConnectionError, "PermissionError": PermissionError,
    "FileNotFoundError": FileNotFoundError, "AttributeError": AttributeError,
    "TypeError": TypeError, "ValueError": ValueError, "RuntimeError": RuntimeError,
    "NotImplementedError": NotImplementedError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "KeyError": KeyError,
    "IndexError": IndexError, "LookupError": LookupError, "OverflowError": OverflowError,
    "StopIteration": StopIteration, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
    "UnicodeError": UnicodeError, "JSONDecodeError": json.JSONDecodeError,
    "PhotonDashboardAuthError": NS["PhotonDashboardAuthError"],
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    cls = BUILTIN.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    if cls is json.JSONDecodeError:
        return cls(msg, "", 0)
    return cls(msg)


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── per-case script backend (mirrors C pa2_call exactly) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []
        self.scripted = []
        self.last_open_path = None

    def _raise(self, name, args, spec, exc):
        self.trace.append({"name": name, "args": canon(args), "ret": spec})
        self.scripted.append((exc, spec))
        raise exc

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            msg = f"no script for {name}"
            self._raise(name, canon(args), {"__raise__": "RuntimeError", "message": msg}, RuntimeError(msg))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                msg = f"script exhausted for {name}"
                self._raise(name, canon(args), {"__raise__": "RuntimeError", "message": msg}, RuntimeError(msg))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                rs = used["raise"]
                spec = {"__raise__": rs.get("type", "RuntimeError"), "message": rs.get("message", "")}
                for k, v in rs.items():
                    if k not in ("type", "message"):
                        spec[k] = v
                self._raise(name, canon(args), spec, make_exc(rs))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": canon(args), "ret": canon(ret)})
        return ret


def rec(ctx, name, argfn):
    def f(*a, **k):
        return ctx.call(name, argfn(*a, **k))
    return f


def exc_spec(exc, ctx):
    for e, spec in ctx.scripted:
        if exc is e:
            return spec
    return proj_exc(exc)


# ── seam argument projections (one per C pa2_call name) ──
ARGMAP = {
    "get_hermes_home": lambda: {},
    "os.getenv": lambda key: {"key": key},
    "get_scoped_secret": lambda name, default=None: {"name": name},
    "config.get_env_value": lambda key: {"key": key},
    "config.save_env_value": lambda key, value: {"key": key, "value": value},
    "time.time": lambda: {},
    "uuid4_hex": lambda: {},
    "os.getpid": lambda: {},
    "os.open": lambda path, flags, mode: {"path": str(path), "flags": flags, "mode": mode},
    "os.fdopen": lambda fd, mode, encoding=None: {"fd": fd, "mode": mode, "encoding": encoding},
    "os.close": lambda fd: {"fd": fd},
    "os.fsync": lambda fd: {"fd": fd},
    "path.exists": lambda path: {"path": str(path)},
    "path.mkdir": lambda path, parents=False, exist_ok=False: {
        "path": str(path), "parents": parents, "exist_ok": exist_ok},
    "path.unlink": lambda path: {"path": str(path)},
    "path.replace": lambda src, dst: {"src": str(src), "dst": str(dst)},
    "json.load": lambda fh: {"path": str(fh.path), "encoding": fh.encoding},
    "json.dump": lambda data, fh, indent=None, sort_keys=False: {
        "data": data, "path": str(fh.path), "indent": indent, "sort_keys": sort_keys},
    "auth_store_lock": lambda: {},
    "safe_call": lambda fn: {"fn": getattr(fn, "__pa2_marker__", "fn")},
    "resp.json": lambda: {},
    "resp.raise_for_status": lambda: {},
    "httpx.get": lambda url, headers=None, timeout=None: {
        "url": url, "headers": headers, "timeout": timeout},
    "httpx.post": lambda url, json=None, headers=None, timeout=None: _httpx_post_args(
        url, json, headers, timeout),
}


def _httpx_post_args(url, json_body, headers, timeout):
    d = {"url": url}
    if json_body is not None or _CALLSITE_POST_JSON:
        d["json"] = json_body
    d["timeout"] = timeout
    if headers is not None:
        d["headers"] = headers
    return d


# httpx.post is called from three live sites with different kwargs; the C
# records exactly those kwargs.  The recorder below handles each explicitly,
# so ARGMAP["httpx.post"] is only a fallback for cross-check shape analysis.
_CALLSITE_POST_JSON = True


def install_recorders(ctx, args):
    config_avail = bool(args.get("config_available", True))
    httpx_avail = bool(args.get("httpx_available", True))

    NS["get_hermes_home"] = lambda: FakePath(ctx.call("get_hermes_home", {}), ctx)
    NS["_get_scoped_secret"] = rec(ctx, "get_scoped_secret", ARGMAP["get_scoped_secret"])

    def get_env_value(key):
        return ctx.call("config.get_env_value", ARGMAP["config.get_env_value"](key))

    def save_env_value(key, value):
        return ctx.call("config.save_env_value", ARGMAP["config.save_env_value"](key, value))

    # Import-failure gating: from pkg import x reads the parent's ATTRIBUTE.
    # Setting it to None makes the import succeed and the call TypeError;
    # DELETING it makes `from ... import x` raise ImportError (which the live
    # except ImportError/Exception handlers catch).
    for mod, nm, fn in ((CFG_MOD, "get_env_value", get_env_value),
                        (CFG_MOD, "save_env_value", save_env_value)):
        if config_avail:
            setattr(mod, nm, fn)
        elif hasattr(mod, nm):
            delattr(mod, nm)
    AUTH_MOD._auth_store_lock = lambda: CMDouble(ctx, "auth_store_lock")

    NS["time"] = types.SimpleNamespace(
        time=rec(ctx, "time.time", ARGMAP["time.time"]),
        sleep=lambda s: None)

    def uuid4():
        h = ctx.call("uuid4_hex", {})
        return types.SimpleNamespace(hex=h)
    NS["uuid"] = types.SimpleNamespace(uuid4=uuid4)

    ns_os = types.SimpleNamespace(
        getenv=rec(ctx, "os.getenv", ARGMAP["os.getenv"]),
        getpid=rec(ctx, "os.getpid", ARGMAP["os.getpid"]),
        O_WRONLY=1, O_CREAT=64, O_EXCL=128)

    def os_open(path, flags, mode):
        ctx.last_open_path = path
        return ctx.call("os.open", ARGMAP["os.open"](path, flags, mode))

    def os_fdopen(fd, mode, encoding=None):
        ctx.call("os.fdopen", ARGMAP["os.fdopen"](fd, mode, encoding))
        return FileDouble(FakePath(ctx.last_open_path, ctx), encoding, fd=fd)

    ns_os.open = os_open
    ns_os.fdopen = os_fdopen
    ns_os.close = rec(ctx, "os.close", ARGMAP["os.close"])
    ns_os.fsync = rec(ctx, "os.fsync", ARGMAP["os.fsync"])
    NS["os"] = ns_os
    NS["stat"] = types.SimpleNamespace(S_IRUSR=0o400, S_IWUSR=0o200)

    jd = types.SimpleNamespace(
        load=rec(ctx, "json.load", ARGMAP["json.load"]),
        dump=rec(ctx, "json.dump", ARGMAP["json.dump"]),
        dumps=json.dumps, JSONDecodeError=json.JSONDecodeError)
    NS["json"] = jd

    def httpx_get(url, headers=None, timeout=None):
        r = ctx.call("httpx.get", {"url": url, "headers": headers, "timeout": timeout})
        return RespDouble(r, ctx)

    def httpx_post(url, json=None, headers=None, timeout=None):
        d = {"url": url}
        if json is not None:
            d["json"] = json
        if headers is not None:
            d["headers"] = headers
        d["timeout"] = timeout
        r = ctx.call("httpx.post", d)
        return RespDouble(r, ctx)

    class _ReqErr(Exception):
        pass

    NS["httpx"] = (types.SimpleNamespace(get=httpx_get, post=httpx_post,
                                          RequestError=_ReqErr, HTTPError=_ReqErr)
                   if httpx_avail else None)
    HC_MOD.get_hermes_home = NS["get_hermes_home"]


def resp_from(ctx, v):
    return RespDouble(v, ctx) if isinstance(v, dict) else v


def path_from(ctx, v):
    return FakePath(v if isinstance(v, str) else ("" if v is None else str(v)), ctx)


def safe_callable(ctx, marker):
    def fn():
        return ctx.call("safe_call", {"fn": marker})
    fn.__pa2_marker__ = marker
    return fn


# ── per-case live dispatch ──
def live_call(func, args, ctx):
    g = lambda k, d=None: args.get(k, d)

    if func == "_bearer":
        return NS[func](g("token"))
    if func == "_basic":
        return NS[func](g("project_id"), g("project_secret"))
    if func == "_require_httpx":
        return NS[func](g("what", ""))
    if func in ("_dashboard_host", "_spectrum_host", "_auth_json_path",
                "_load_auth", "load_photon_token", "clear_photon_token",
                "load_project_credentials", "load_dashboard_project_id"):
        return NS[func]()
    if func == "_clean_bearer_token":
        return NS[func](g("value"))
    if func == "_header_value":
        return NS[func](g("headers"), g("name", ""))
    if func == "_normalize_phone":
        return NS[func](g("phone"))
    if func == "_raise_on_error_key":
        return NS[func](g("data"), g("action", ""))
    if func == "_unwrap_list":
        return NS[func](g("data"))
    if func == "user_assigned_line":
        return NS[func](g("user"))
    if func == "user_opted_in":
        return NS[func](g("user"))
    if func == "_get_config_env_value":
        return NS[func](g("key", ""))
    if func == "_configured_operator_phone":
        return NS[func]()
    if func == "_safe":
        return NS[func](safe_callable(ctx, g("fn", "fn")))
    if func == "_response_error_detail":
        return NS[func](RespDouble(g("resp") or {}, ctx))
    if func == "_raise_for_status":
        return NS[func](RespDouble(g("resp") or {}, ctx), g("action", ""))
    if func == "_pool_first":
        return NS[func](g("auth"), g("key", ""))
    if func == "_save_auth":
        return NS[func](g("data"))
    if func == "_store_pool_record":
        return NS[func](g("key", ""), g("record"))
    if func == "store_photon_token":
        return NS[func](g("token"))
    if func == "store_project_credentials":
        return NS[func](spectrum_project_id=g("spectrum_project_id"),
                        project_secret=g("project_secret"),
                        dashboard_project_id=g("dashboard_project_id"),
                        name=g("name"))
    if func == "store_user_numbers":
        return NS[func](phone_number=g("phone_number"),
                        assigned_phone_number=g("assigned_phone_number"),
                        user_id=g("user_id"),
                        dashboard_project_id=g("dashboard_project_id"))
    if func == "_persist_runtime_env":
        return NS[func](g("spectrum_project_id"), g("project_secret"))
    if func == "_device_response_token_candidates":
        return NS[func](g("body"), headers=g("headers"))
    if func == "request_device_code":
        return NS[func](client_id=g("client_id", "photon-cli"), scope=g("scope", "openid profile email"))
    if func in ("check_photon_token_valid", "validate_photon_token"):
        return NS[func](g("token"))
    if func == "_validated_dashboard_token":
        raw = g("candidates") or []
        cands = [NS["_DeviceTokenCandidate"](source=(c or {}).get("source", ""),
                                             token=(c or {}).get("token", "")) for c in raw]
        return NS[func](cands)
    if func == "_dashboard_get":
        return NS[func](g("path", ""), g("token"), what=g("what", " device login"))
    if func == "_dashboard_post":
        return NS[func](g("path", ""), g("body"), g("token"), what=g("what", ""))
    if func == "_dashboard_list":
        return NS[func](g("path", ""), g("token"))
    if func == "list_projects":
        return NS[func](g("token"))
    if func == "find_project_by_name":
        return NS[func](g("token"), g("name", ""))
    raise RuntimeError(f"unknown module func {func}")


_CUR_CTX = [None]


def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    _CUR_CTX[0] = ctx
    install_recorders(ctx, args)
    state_out = {}
    try:
        ret = live_call(func, args, ctx)
    except BaseException as exc:
        ret = exc_spec(exc, ctx)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── consistency checks ──
def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "photon_auth_flow2.c",
                encoding="utf-8").read()
    c_seams = set(re.findall(r'pa2_call\(\s*ctx,\s*"([^"]+)"', csrc))
    c_seams |= {"httpx.get", "httpx.post"}  # passed via a method variable
    if c_seams != SEAMED:
        print(f"FATAL oracle_photon_auth_flow2: C seams {sorted(c_seams)} != ARGMAP {sorted(SEAMED)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "photon_auth_flow2.jsonl"
    if fix.exists():
        fix_keys, fix_funcs = set(), set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func", ""))
        bad = fix_keys - SEAMED
        if bad:
            print(f"FATAL oracle_photon_auth_flow2: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        unbound = fix_funcs - set(TARGETS)
        if unbound:
            print(f"FATAL oracle_photon_auth_flow2: fixture funcs missing from TARGETS: {sorted(unbound)}")
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
    if shown >= 16:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:400]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:400]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:400]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:400]}")


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
    print(f"ORACLE photon_auth_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    fix = _REPO / "tests" / "fixtures" / "photon_auth_flow2.jsonl"
    n = bad = 0
    counts = {}
    for line in open(fix, encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        case = json.loads(line)
        n += 1
        func = case["func"]
        counts[func] = counts.get(func, 0) + 1
        try:
            drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
    faulthandler.cancel_dump_traceback_later()
    print(f"SELFTEST photon_auth_flow2: {n} cases, {bad} drive failures")
    missing = set(TARGETS) - set(counts)
    if missing:
        print(f"SELFTEST FUNCS WITHOUT CASES: {sorted(missing)}")
        bad += 1
    short = [f for f in counts if counts[f] < 5]
    if short:
        print(f"SELFTEST FUNCS WITH <5 CASES: {sorted(short)}")
        bad += 1
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
