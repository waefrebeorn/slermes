#!/usr/bin/env python3
"""Oracle for web_server_flow — the UNCLAIMED web-server core FLOW surface of
hermes_cli/web_server.py (LIVE authority, py3.14 quarry venv).

Ports covered: app-state leaves (_app_state_default,
_get_pty_active_session_files), ssh bootstrap (_apply_ssh_session_token,
_apply_ssh_owner_nonce), cron ownership leaf (_gateway_owns_cron), token guard
(_require_token), the middleware chain (host_header_middleware,
_plugin_api_runtime_gate, _dashboard_auth_gate, auth_middleware,
_token_auth_seam, _dashboard_health_middleware), health leaf
(DashboardHealth.record_selftest), CORS origin (_is_trusted_local_origin,
_LocalOriginCORSMiddleware.is_allowed_origin), startup auth gate
(_no_auth_provider_message, _configure_auth_gate), best-effort / rendezvous
(_best_effort, _reclaim_host_from_orphaned_owner, _publish_host_rendezvous) and
the serve runner (_windows_serve_loop_factory, _run_serve, start_server).

Claimed siblings (_has_valid_session_token, _has_valid_query_token,
should_require_dashboard_auth, _dashboard_public_hosts,
DashboardHealth.record_error, _port_bind_conflict, _report_port_in_use) and
foreign collaborators are SEAMED; unclaimed siblings composed here
(_app_state_default, _is_trusted_local_origin,
_reclaim_host_from_orphaned_owner, _no_auth_provider_message,
_windows_serve_loop_factory) are extracted and run for real.
_start_server_runtime is a heavy orchestrator left SEAMED in start_server.

Drives the LIVE functions with SCRIPTED I/O: defs AST-extracted from the
quarry (no heavy import), module constants seeded (Assign AND AnnAssign,
_PUBLIC_API_PATHS / _LOOPBACK_HOST_VALUES / PORT_IN_USE_EXIT_CODE pulled from
their defining sibling modules, DashboardHealth class exec'd), every effect
replaced by a recording stub returning the SAME scripted results the C
harness feeds its universal seam. Async middleware are driven with asyncio.run
over a scripted async call_next. Compares the ordered DECISION SEQUENCE (trace
of seam calls) plus the return value (and the mutated state window for the
mutation flows). Scripted {"__raise__": cls} raises a REAL exception of that
class; drive() catches BaseException so KeyboardInterrupt/SystemExit pass
through live except-Exception clauses exactly as the C port propagates them. A
MISSING seam key fail-closes (RuntimeError) on both sides; a scripted null is a
valid None result.

Input: stdin JSON lines from t_port_web_server_flow:
    {"func": ..., "in": {...}, "out": {"ret":..., "trace":[...][, "state":...]}}
Summary: ORACLE web_server_flow: N cases, M mismatches. Fail-closed (exit 2)
when extraction misses defs or zero cases were compared.
--self-test: drive every fixture case (no C) and report binding/script gaps.
"""
import ast
import asyncio
import copy
import json
import os
import sys
import types
from collections import deque
from pathlib import Path, PurePosixPath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "web_server_flow_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "web_server.py")
SRC_PUBLIC = os.path.join(QUARRY, "hermes_cli", "dashboard_auth", "public_paths.py")
SRC_BOUNDARY = os.path.join(QUARRY, "hermes_cli", "web_server_boundary.py")
SRC_LIFECYCLE = os.path.join(QUARRY, "hermes_cli", "web_server_lifecycle.py")

# C harness func -> live python name.
PY = {
    "gateway_owns_cron": "_gateway_owns_cron",
    "app_state_default": "_app_state_default",
    "get_pty_active_session_files": "_get_pty_active_session_files",
    "apply_ssh_session_token": "_apply_ssh_session_token",
    "apply_ssh_owner_nonce": "_apply_ssh_owner_nonce",
    "require_token": "_require_token",
    "host_header_middleware": "host_header_middleware",
    "plugin_api_runtime_gate": "_plugin_api_runtime_gate",
    "dashboard_auth_gate": "_dashboard_auth_gate",
    "auth_middleware": "auth_middleware",
    "token_auth_seam": "_token_auth_seam",
    "dashboard_health_middleware": "_dashboard_health_middleware",
    "record_selftest": "record_selftest",
    "is_trusted_local_origin": "_is_trusted_local_origin",
    "is_allowed_origin": "is_allowed_origin",
    "no_auth_provider_message": "_no_auth_provider_message",
    "configure_auth_gate": "_configure_auth_gate",
    "best_effort": "_best_effort",
    "reclaim_host_from_orphaned_owner": "_reclaim_host_from_orphaned_owner",
    "publish_host_rendezvous": "_publish_host_rendezvous",
    "windows_serve_loop_factory": "_windows_serve_loop_factory",
    "run_serve": "_run_serve",
    "start_server": "start_server",
}
WANTED = {
    "_gateway_owns_cron", "_app_state_default", "_get_pty_active_session_files",
    "_apply_ssh_session_token", "_apply_ssh_owner_nonce", "_require_token",
    "host_header_middleware", "_plugin_api_runtime_gate", "_dashboard_auth_gate",
    "auth_middleware", "_token_auth_seam", "_dashboard_health_middleware",
    "_is_trusted_local_origin", "_no_auth_provider_message", "_configure_auth_gate",
    "_best_effort", "_reclaim_host_from_orphaned_owner", "_publish_host_rendezvous",
    "_windows_serve_loop_factory", "_run_serve", "start_server",
}
ASYNC = {
    "host_header_middleware", "plugin_api_runtime_gate", "dashboard_auth_gate",
    "auth_middleware", "token_auth_seam", "dashboard_health_middleware",
}
STATEFUL = {
    "app_state_default", "get_pty_active_session_files", "apply_ssh_session_token",
    "apply_ssh_owner_nonce", "record_selftest", "configure_auth_gate",
    "publish_host_rendezvous",
}

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}, "statewin": None}


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    if name not in CUR["scripts"]:
        r = {"__raise__": "RuntimeError", "msg": "unscripted seam: " + name}
    else:
        sc = CUR["scripts"][name]
        if isinstance(sc, list):
            i = CUR["idx"].get(name, 0)
            CUR["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    return maybe_raise(r)


class HTTPException(Exception):
    """fastapi.HTTPException double: str is '<status_code>: <detail>'."""
    def __init__(self, status_code, detail=None):
        self.status_code = status_code
        self.detail = detail
        super().__init__(f"{status_code}: {detail}")


EXC_MAP = {
    "OSError": OSError, "FileNotFoundError": FileNotFoundError,
    "PermissionError": PermissionError, "FileExistsError": FileExistsError,
    "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
    "TimeoutError": TimeoutError, "BlockingIOError": BlockingIOError,
    "ProcessLookupError": ProcessLookupError, "ChildProcessError": ChildProcessError,
    "InterruptedError": InterruptedError, "ConnectionError": ConnectionError,
    "BrokenPipeError": BrokenPipeError, "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError, "ConnectionResetError": ConnectionResetError,
    "ValueError": ValueError, "TypeError": TypeError, "KeyError": KeyError,
    "AttributeError": AttributeError, "RuntimeError": RuntimeError,
    "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "IndexError": IndexError, "LookupError": LookupError, "SyntaxError": SyntaxError,
    "IndentationError": IndentationError, "TabError": TabError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeError": UnicodeError, "NameError": NameError, "UnboundLocalError": UnboundLocalError,
    "AssertionError": AssertionError, "StopIteration": StopIteration,
    "ArithmeticError": ArithmeticError, "ZeroDivisionError": ZeroDivisionError,
    "OverflowError": OverflowError, "FloatingPointError": FloatingPointError,
    "MemoryError": MemoryError, "ReferenceError": ReferenceError, "SystemError": SystemError,
    "BufferError": BufferError, "EOFError": EOFError, "Exception": Exception,
    "BaseException": BaseException, "KeyboardInterrupt": KeyboardInterrupt,
    "SystemExit": SystemExit, "GeneratorExit": GeneratorExit,
    "Warning": Warning, "UserWarning": UserWarning, "DeprecationWarning": DeprecationWarning,
    "RuntimeWarning": RuntimeWarning, "FutureWarning": FutureWarning,
    "ResourceWarning": ResourceWarning,
    "BaseExceptionGroup": BaseExceptionGroup, "ExceptionGroup": ExceptionGroup,
}


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        name = r["__raise__"]
        if name == "HTTPException":
            raise HTTPException(r.get("status_code", 500), r.get("detail", r.get("msg", "")))
        if name == "SystemExit":
            raise SystemExit(r.get("code", r.get("msg", "")))
        cls = EXC_MAP.get(name)
        if cls is None:
            cls = type(name, (Exception,), {})
        raise cls(r.get("msg", ""))
    return r


NS = {}


def posix(p):
    if p is None:
        return None
    if isinstance(p, PathDouble):
        return p.posix
    if isinstance(p, PurePosixPath):
        return p.as_posix()
    if isinstance(p, Path):
        return p.as_posix()
    return PurePosixPath(p).as_posix()


class PathDouble:
    def __init__(self, s):
        self.posix = PurePosixPath(s).as_posix() if s is not None else ""

    def __str__(self):
        return self.posix

    def __fspath__(self):
        return self.posix


def norm(o):
    if o is None or isinstance(o, (bool, int, str)):
        return o
    if isinstance(o, float):
        return o
    if isinstance(o, PathDouble):
        return o.posix
    if isinstance(o, Path):
        return o.as_posix()
    if isinstance(o, types.SimpleNamespace):
        return {k: norm(v) for k, v in vars(o).items()}
    if isinstance(o, deque):
        return [norm(v) for v in o]
    if isinstance(o, (frozenset, set)):
        return sorted(norm(v) for v in o)
    if callable(o):
        return {"__callable__": getattr(o, "__ws_callable__", True)}
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    return str(o)


# ── extraction ──

def _seed_constant(ns, path, name):
    tree = ast.parse(open(path, encoding="utf-8").read())
    for node in tree.body:
        tgt = None
        if isinstance(node, ast.Assign) and len(node.targets) == 1 and isinstance(node.targets[0], ast.Name):
            tgt = node.targets[0].id
        elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
            tgt = node.target.id
        if tgt == name:
            exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec"), ns)
            return True
    return False


def extract():
    import logging
    import re
    import threading
    import time
    from typing import Any, Dict, Optional, Tuple

    src = open(SRC, encoding="utf-8").read()

    class RecLogger:
        def debug(self, *a, **k): pass
        def info(self, *a, **k): pass
        def warning(self, *a, **k): pass
        def error(self, *a, **k): pass
        def exception(self, *a, **k): pass

    ns = {
        "__name__": "hermes_cli.web_server_extracted",
        "json": json, "os": os, "re": re, "sys": sys, "time": time,
        "threading": threading, "logging": logging, "deque": deque,
        "Path": PathDouble, "PurePosixPath": PurePosixPath,
        "Any": Any, "Dict": Dict, "Optional": Optional, "Tuple": Tuple,
        "_log": RecLogger(),
    }
    tree = ast.parse(src)
    # module constants (Assign + AnnAssign); failing ones (FastAPI app, imports)
    # are skipped — the per-case doubles supply them.
    for node in tree.body:
        if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    # constants imported from sibling modules — pull the literal Assign node.
    for path, name in ((SRC_PUBLIC, "PUBLIC_API_PATHS"),
                       (SRC_BOUNDARY, "_LOOPBACK_HOST_VALUES"),
                       (SRC_LIFECYCLE, "PORT_IN_USE_EXIT_CODE")):
        if not _seed_constant(ns, path, name):
            print("FATAL missing sibling constant: %s" % name)
            sys.exit(2)
    ns["_PUBLIC_API_PATHS"] = ns["PUBLIC_API_PATHS"]
    # the module's `_log = logging.getLogger(__name__)` Assign overwrites the
    # seeded recorder — re-set it AFTER the constant loop.
    ns["_log"] = RecLogger()
    # DashboardHealth class (record_selftest is a real method on it).
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "DashboardHealth":
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    # _LocalOriginCORSMiddleware.is_allowed_origin: extract the method alone
    # (the CORSMiddleware base is heavy); it composes _is_trusted_local_origin.
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "_LocalOriginCORSMiddleware":
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name == "is_allowed_origin":
                    exec(compile(ast.Module(body=[sub], type_ignores=[]), SRC, "exec"), ns)
                    ns["is_allowed_origin"] = ns.pop("is_allowed_origin")
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANTED:
            # strip decorators (@app.middleware("http")) — app is a per-case double
            n = copy.copy(node)
            n.decorator_list = []
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    missing = WANTED - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    for k in ("DashboardHealth", "is_allowed_origin", "_PUBLIC_API_PATHS",
              "_LOOPBACK_HOST_VALUES", "PORT_IN_USE_EXIT_CODE",
              "_DASHBOARD_HEALTH_WINDOW_SECONDS"):
        if k not in ns:
            print("FATAL missing constant/class: %s" % k)
            sys.exit(2)
    global NS
    NS = ns
    return ns


# ── doubles ──

class OsDouble:
    def getpid(self):
        return seam("os_getpid", {})

    def stat(self, path):
        r = seam("os_stat", {"path": path})
        return types.SimpleNamespace(**r) if isinstance(r, dict) else r

    class path:
        @staticmethod
        def join(p, suffix):
            return seam("os_path_join", {"path": p, "suffix": suffix})


class TimeDouble:
    def time(self):
        return seam("time_time", {})

    def monotonic(self):
        return seam("time_monotonic", {})


class FileDouble:
    def __enter__(self):
        seam("marker_file_enter", {})
        return self

    def write(self, data):
        return seam("marker_file_write", {"data": data})

    def __exit__(self, et, ev, tb):
        seam("marker_file_exit", {})
        return False


def _open_stub(path, mode="r", encoding=None):
    seam("open_marker", {"path": path, "mode": mode, "encoding": encoding})
    return FileDouble()


class ScopeCM:
    def __init__(self, enter_name, exit_name):
        self.enter_name = enter_name
        self.exit_name = exit_name

    def __enter__(self):
        seam(self.enter_name, {})
        return self

    def __exit__(self, et, ev, tb):
        seam(self.exit_name, {})
        return False


class NousDouble:
    def __getattr__(self, name):
        if name == "LAST_SKIP_REASON":
            return seam("nous_last_skip_reason", {})
        raise AttributeError(name)


OUTCOMES = {t: types.SimpleNamespace(value=t)
            for t in ("acquired", "held-by-other", "could-not-open")}


class HrDouble:
    ROLE_SERVE = "serve"
    ROLE_DESKTOP_SERVE = "desktop-serve"
    HostLockOutcome = types.SimpleNamespace(
        ACQUIRED=OUTCOMES["acquired"], HELD_BY_OTHER=OUTCOMES["held-by-other"],
        COULD_NOT_OPEN=OUTCOMES["could-not-open"])

    def read_record(self, role):
        r = seam("read_record", {"role": role})
        return types.SimpleNamespace(**r) if isinstance(r, dict) else r

    def discard_dead_record(self, role):
        return seam("discard_dead_record", {"role": role})

    def claim_host_lock(self, role):
        r = seam("claim_host_lock", {"role": role})
        tok = r[0] if isinstance(r, list) and r else None
        err = r[1] if isinstance(r, list) and len(r) > 1 else None
        return (OUTCOMES.get(tok, tok), err)

    def publish_record(self, role, host=None, port=None, profiles=None, token=None):
        return seam("publish_record", {"role": role, "host": host, "port": port,
                                       "profiles": profiles, "token": token})

    def served_profiles(self):
        return seam("served_profiles", {})

    def cleanup_on_exit(self, role):
        return seam("cleanup_on_exit", {"role": role})

    def describe(self, record):
        return seam("describe", {"record": vars(record) if isinstance(record, types.SimpleNamespace) else record})


PROACTOR = lambda: None
PROACTOR.__ws_callable__ = "proactor"
SELECTOR = lambda: None
SELECTOR.__ws_callable__ = "selector"


def _loop_factory_stub():
    r = seam("get_loop_factory", {})
    if r is None:
        return None
    if isinstance(r, dict) and "__callable__" in r:
        tag = r["__callable__"]
        if tag == "proactor":
            return PROACTOR
        if tag == "selector":
            return SELECTOR
        f = lambda: None
        f.__ws_callable__ = tag
        return f
    return r


def _project_kw(kw):
    return {k: (norm(v) if callable(v) else v) for k, v in kw.items()}


def _asyncio_run(coro, **kw):
    return seam("asyncio_run", {"coro": coro, **_project_kw(kw)})


def _uvicorn_asyncio_run(coro, **kw):
    return seam("uvicorn_asyncio_run", {"coro": coro, **_project_kw(kw)})


class AsyncioDouble:
    run = staticmethod(_asyncio_run)
    ProactorEventLoop = staticmethod(PROACTOR)
    SelectorEventLoop = staticmethod(SELECTOR)

    @staticmethod
    def WindowsSelectorEventLoopPolicy():
        return seam("windows_selector_policy", {})

    @staticmethod
    def set_event_loop_policy(policy):
        return seam("set_event_loop_policy", {"policy": policy})


def req_proj(req):
    return {
        "state": dict(vars(req.state)),
        "app": {"state": dict(vars(req.app.state))},
        "headers": dict(req.headers),
        "query_params": dict(req.query_params),
        "url": {"path": req.url.path},
        "scope": dict(req.scope),
    }


def mat_request(inp):
    r = inp.get("request") or {}
    st = types.SimpleNamespace(**(r.get("state") or {}))
    appst = types.SimpleNamespace(**((r.get("app") or {}).get("state") or {}))
    app = types.SimpleNamespace(state=appst)
    headers = dict(r.get("headers") or {})
    qp = dict(r.get("query_params") or {})
    url = types.SimpleNamespace(path=(r.get("url") or {}).get("path", ""))
    scope = dict(r.get("scope") or {})
    return types.SimpleNamespace(state=st, app=app, headers=headers,
                                 query_params=qp, url=url, scope=scope)


async def _call_next(request):
    r = seam("call_next", {"request": req_proj(request)})
    if isinstance(r, dict):
        return types.SimpleNamespace(**r)
    return r


_call_next.__ws_callable__ = "call_next"


def _json_response(status_code, content):
    return {"__response__": "JSONResponse", "status_code": status_code, "content": content}


# ── recording stubs ──

def make_import_module(modname, attrs):
    """attrs: {attribute: (import_seam_name, fn_or___data__)}. getattr fires the
    import seam; '__data__' returns the seam RESULT (materialized frozenset for
    a list). NO __path__ on leaf stubs (avoids double import-seam fires)."""
    class M(types.ModuleType):
        def __getattr__(self, name):
            if name in attrs:
                ise, fn = attrs[name]
                r = seam(ise, {})
                if fn == "__data__":
                    return frozenset(r) if isinstance(r, list) else r
                return fn
            raise AttributeError(name)
    m = M(modname)
    sys.modules[modname] = m
    return m


def install_stubs(ns):
    # claimed web_server.py siblings + foreign bare globals (module-top imports)
    ns["_has_valid_session_token"] = lambda request: seam("has_valid_session_token", {"request": req_proj(request)})
    ns["_has_valid_query_token"] = lambda request, path: seam("has_valid_query_token", {"request": req_proj(request), "path": path})
    ns["should_require_dashboard_auth"] = lambda host, trusted_public_hosts=None: seam(
        "should_require_dashboard_auth", {"host": host, "trusted_public_hosts": trusted_public_hosts})
    ns["_dashboard_public_hosts"] = lambda: seam("dashboard_public_hosts", {})
    ns["_dashboard_public_origins"] = lambda: seam("dashboard_public_origins", {})
    ns["public_url_hosts"] = lambda url: seam("public_url_hosts", {"url": url})
    ns["public_url_origins"] = lambda url: seam("public_url_origins", {"url": url})
    ns["should_require_auth"] = lambda host: seam("should_require_auth", {"host": host})
    ns["_desktop_loopback_auth_exempt"] = lambda host, tok, nonce: seam(
        "desktop_loopback_auth_exempt", {"host": host, "ssh_session_token": tok, "ssh_owner_nonce": nonce})
    ns["_is_accepted_host"] = lambda host, bound, trusted: seam(
        "is_accepted_host", {"host": host, "bound_host": bound, "trusted_public_hosts": trusted})
    ns["_port_bind_conflict"] = lambda host, port: seam("port_bind_conflict", {"host": host, "port": port})
    ns["_report_port_in_use"] = lambda host, port: seam("report_port_in_use", {"host": host, "port": port})
    ns["is_desktop_owned_backend"] = lambda: seam("is_desktop_owned_backend", {})
    ns["_plugin_api_name"] = lambda scope: seam("plugin_api_name", {"scope": scope})

    async def _plugin_api_request_refused(plugin_name):
        return seam("plugin_api_request_refused", {"plugin_name": plugin_name})
    ns["_plugin_api_request_refused"] = _plugin_api_request_refused

    async def _forward_secondary_plugin_request(request, plugin_name):
        return seam("forward_secondary_plugin_request", {"request": req_proj(request), "plugin_name": plugin_name})
    ns["forward_secondary_plugin_request"] = _forward_secondary_plugin_request

    def _plugin_request_scope(scope):
        seam("plugin_request_scope", {"scope": scope})
        return ScopeCM("plugin_scope_enter", "plugin_scope_exit")
    ns["plugin_request_scope"] = _plugin_request_scope

    ns["JSONResponse"] = _json_response
    ns["HTTPException"] = HTTPException
    ns["DASHBOARD_HEALTH"] = types.SimpleNamespace(
        record_error=lambda exc_type, path: seam("record_error", {"exc_type": exc_type, "path": path}))

    # stdlib doubles
    ns["os"] = OsDouble()
    ns["time"] = TimeDouble()
    ns["sysconfig"] = types.SimpleNamespace(get_paths=lambda: seam("sysconfig_get_paths", {}))
    ns["open"] = _open_stub
    ns["asyncio"] = AsyncioDouble()
    ns["Path"] = PathDouble

    def _start_server_runtime(*a, **k):
        return seam("start_server_runtime", {"args": list(a), "kwargs": k})
    ns["_start_server_runtime"] = _start_server_runtime

    # stub packages (parents carry __path__; from-target leaves do NOT)
    for pkg in ("hermes_cli", "plugins", "uvicorn", "pm"):
        if pkg not in sys.modules or not isinstance(sys.modules[pkg], types.ModuleType):
            m = types.ModuleType(pkg)
            m.__path__ = []
            sys.modules[pkg] = m
        elif not hasattr(sys.modules[pkg], "__path__"):
            sys.modules[pkg].__path__ = []

    make_import_module("hermes_cli.profiles", {
        "_check_gateway_running": ("import_check_gateway_running",
                                   lambda home: seam("check_gateway_running", {"home": posix(home)})),
        "_served_by_running_multiplexer": ("import_served_by_running_multiplexer",
                                           lambda name: seam("served_by_running_multiplexer", {"name": name})),
    })
    make_import_module("hermes_cli.dashboard_auth.middleware", {
        "gated_auth_middleware": ("import_gated_auth_middleware", _gated_auth_middleware),
    })
    make_import_module("hermes_cli.dashboard_auth.peer_pass", {
        "is_peer_path": ("import_is_peer_path", lambda path: seam("is_peer_path", {"path": path})),
        "peer_pass_middleware": ("import_peer_pass_middleware", _peer_pass_middleware),
    })
    make_import_module("hermes_cli.dashboard_auth.native_http", {
        "authenticate_native_http": ("import_authenticate_native_http", _authenticate_native_http),
        "native_profile_scope": ("import_native_profile_scope", _native_profile_scope),
    })
    make_import_module("hermes_cli.dashboard_auth.token_auth", {
        "token_auth_middleware": ("import_token_auth_middleware", _token_auth_middleware),
    })
    make_import_module("storage_user_copy", {
        "error_type_name": ("import_error_type_name",
                            lambda exc: seam("error_type_name", {"exc": {"__exc__": type(exc).__name__, "msg": str(exc)}})),
    })
    make_import_module("hermes_cli.web_server_boundary", {
        "is_trusted_local_origin": ("import_is_trusted_local_origin",
                                    lambda origin, app_state: seam("is_trusted_local_origin",
                                                                   {"origin": origin, "app_state": vars(app_state)})),
    })
    make_import_module("plugins.dashboard_auth", {
        "nous": ("import_nous", NousDouble()),
    })
    make_import_module("hermes_cli.dashboard_auth.prefix", {
        "resolve_public_url": ("import_resolve_public_url", lambda: seam("resolve_public_url", {})),
    })
    make_import_module("hermes_cli.config", {
        "load_config": ("import_load_config", lambda: seam("load_config", {})),
    })
    make_import_module("hermes_cli.plugins_cmd", {
        "_BASIC_AUTH_PLUGIN_KEYS": ("import_BASIC_AUTH_PLUGIN_KEYS", "__data__"),
    })
    make_import_module("hermes_cli.dashboard_auth", {
        "list_providers": ("import_list_providers", _list_providers),
    })
    make_import_module("gateway", {
        "host_rendezvous": ("import_host_rendezvous", HrDouble()),
    })
    make_import_module("hermes_cli.process_identity", {
        "reap_orphaned_backend_owner": ("import_reap_orphaned_backend_owner",
                                        lambda pid, ctime: seam("reap_orphaned_backend_owner",
                                                                {"pid": pid, "create_time": ctime})),
    })
    make_import_module("uvicorn._compat", {
        "asyncio_run": ("import_asyncio_run", _uvicorn_asyncio_run),
    })
    make_import_module("pm.extras", {
        "suppress_interactive_install_prompts": ("import_suppress_interactive_install_prompts",
                                                 _suppress_prompts),
    })


async def _gated_auth_middleware(request, call_next):
    return seam("gated_auth_middleware", {"request": req_proj(request), "call_next": {"__callable__": "call_next"}})


async def _peer_pass_middleware(request, call_next):
    return seam("peer_pass_middleware", {"request": req_proj(request), "call_next": {"__callable__": "call_next"}})


async def _authenticate_native_http(request):
    return seam("authenticate_native_http", {"request": req_proj(request)})


def _native_profile_scope(request):
    seam("native_profile_scope", {"request": req_proj(request)})
    return ScopeCM("native_scope_enter", "native_scope_exit")


async def _token_auth_middleware(request, call_next):
    return seam("token_auth_middleware", {"request": req_proj(request), "call_next": {"__callable__": "call_next"}})


def _list_providers():
    r = seam("list_providers", {})
    if isinstance(r, list):
        return [types.SimpleNamespace(**d) for d in r]
    return r


def _suppress_prompts():
    seam("suppress_prompts_cm", {})
    return ScopeCM("suppress_prompts_enter", "suppress_prompts_exit")


# ── drive ──

def drive(ns, func, inp):
    if func not in PY:
        print("FATAL unknown func: %s" % func)
        sys.exit(2)
    name = PY[func]
    ns["app"] = types.SimpleNamespace(state=types.SimpleNamespace(**(inp.get("app_state") or {})))

    if func in ASYNC:
        f = ns[name]
        req = mat_request(inp)
        return asyncio.run(f(req, _call_next))

    if func == "gateway_owns_cron":
        return ns[name](inp.get("name", "default"), inp.get("home", ""))
    if func == "app_state_default":
        state = copy.deepcopy(inp.get("state") or {})
        app = types.SimpleNamespace(state=types.SimpleNamespace(**state))
        fk = (inp.get("factory") or {}).get("__callable__", "factory")
        factory = dict if fk == "dict" else (lambda: seam("app_state_factory", {}))
        CUR["statewin"] = vars(app.state)   # live reference: echoes even on raise
        return ns[name](app, inp.get("name", ""), factory)
    if func == "get_pty_active_session_files":
        state = copy.deepcopy(inp.get("state") or {})
        app = types.SimpleNamespace(state=types.SimpleNamespace(**state))
        CUR["statewin"] = vars(app.state)
        return ns[name](app)
    if func == "apply_ssh_session_token":
        state = inp.get("state") or {}
        ns["_SESSION_TOKEN"] = state.get("_SESSION_TOKEN", "")
        ns[name](inp.get("token"))
        CUR["statewin"] = {"_SESSION_TOKEN": ns["_SESSION_TOKEN"]}
        return None
    if func == "apply_ssh_owner_nonce":
        state = inp.get("state") or {}
        ns["_SSH_OWNER_NONCE"] = state.get("_SSH_OWNER_NONCE")
        ns["_SSH_RUNTIME_PURELIB"] = state.get("_SSH_RUNTIME_PURELIB")
        ns["_SSH_RUNTIME_MARKER"] = state.get("_SSH_RUNTIME_MARKER")

        def _snap():
            CUR["statewin"] = {"_SSH_OWNER_NONCE": ns["_SSH_OWNER_NONCE"],
                               "_SSH_RUNTIME_PURELIB": ns["_SSH_RUNTIME_PURELIB"],
                               "_SSH_RUNTIME_MARKER": ns["_SSH_RUNTIME_MARKER"]}
        try:
            ns[name](inp.get("nonce"))
        finally:
            _snap()   # echo the globals even when the flow raises
        return None
    if func == "require_token":
        return ns[name](mat_request(inp))
    if func == "record_selftest":
        h = ns["DashboardHealth"]()
        for k, v in (inp.get("health") or {}).items():
            setattr(h, k, v)
        CUR["statewin"] = vars(h)   # live reference: echoes even on raise
        h.record_selftest(inp.get("passed", False), inp.get("http_status"))
        return None
    if func == "is_trusted_local_origin":
        return ns[name](inp.get("origin", ""))
    if func == "is_allowed_origin":
        return ns["is_allowed_origin"](types.SimpleNamespace(), inp.get("origin", ""))
    if func == "no_auth_provider_message":
        return ns[name](inp.get("host", ""))
    if func == "configure_auth_gate":
        state = copy.deepcopy(inp.get("state") or {})
        ns["app"] = types.SimpleNamespace(state=types.SimpleNamespace(**state))
        CUR["statewin"] = vars(ns["app"].state)
        return ns[name](inp.get("host", ""), inp.get("allow_public", False),
                        inp.get("ssh_session_token"), inp.get("ssh_owner_nonce"))
    if func == "best_effort":
        ns[name](inp.get("what", ""), lambda: seam("best_effort_fn", {}))
        return None
    if func == "reclaim_host_from_orphaned_owner":
        return ns[name](inp.get("role", "serve"))
    if func == "publish_host_rendezvous":
        state = copy.deepcopy(inp.get("state") or {})
        ns["app"] = types.SimpleNamespace(state=types.SimpleNamespace(**state))
        ns["_SESSION_TOKEN"] = inp.get("session_token", "")
        CUR["statewin"] = vars(ns["app"].state)
        return ns[name](inp.get("host", ""), inp.get("port", 9119))
    if func == "windows_serve_loop_factory":
        config = types.SimpleNamespace(get_loop_factory=_loop_factory_stub)
        return norm(ns[name](config))
    if func == "run_serve":
        ns["sys"] = types.SimpleNamespace(platform=inp.get("platform", "linux"))
        serve = lambda: seam("serve_call", {})
        config = types.SimpleNamespace(get_loop_factory=_loop_factory_stub)
        ns[name](serve, config, inp.get("host", "127.0.0.1"), inp.get("port", 9119))
        return None
    if func == "start_server":
        args = inp.get("args") or []
        kwargs = inp.get("kwargs") or {}
        return ns[name](*args, **kwargs)
    raise ValueError("unhandled func: %s" % func)


def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, list):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    return json.loads(json.dumps(_numnorm(o), sort_keys=True, ensure_ascii=False, default=str))


SEAM_NAMES = [
    "check_gateway_running", "served_by_running_multiplexer", "app_state_factory",
    "sysconfig_get_paths", "os_path_join", "open_marker", "marker_file_enter",
    "marker_file_write", "marker_file_exit", "os_getpid", "os_stat",
    "has_valid_session_token", "has_valid_query_token", "is_accepted_host",
    "call_next", "plugin_api_name", "plugin_request_scope", "plugin_scope_enter",
    "plugin_scope_exit", "plugin_api_request_refused",
    "forward_secondary_plugin_request", "gated_auth_middleware", "is_peer_path",
    "peer_pass_middleware", "authenticate_native_http", "native_profile_scope",
    "native_scope_enter", "native_scope_exit", "token_auth_middleware",
    "error_type_name", "record_error", "time_time", "is_trusted_local_origin",
    "nous_last_skip_reason", "resolve_public_url", "load_config",
    "import_BASIC_AUTH_PLUGIN_KEYS", "dashboard_public_hosts", "public_url_hosts",
    "dashboard_public_origins", "public_url_origins", "desktop_loopback_auth_exempt",
    "should_require_auth", "should_require_dashboard_auth", "list_providers",
    "best_effort_fn", "read_record", "discard_dead_record",
    "reap_orphaned_backend_owner", "is_desktop_owned_backend", "claim_host_lock",
    "describe", "publish_record", "served_profiles", "cleanup_on_exit",
    "get_loop_factory", "serve_call", "asyncio_run", "uvicorn_asyncio_run",
    "windows_selector_policy", "set_event_loop_policy", "port_bind_conflict",
    "report_port_in_use", "suppress_prompts_cm", "suppress_prompts_enter",
    "suppress_prompts_exit", "start_server_runtime",
]


def self_test(ns):
    """Drive every fixture case live (no C): surface binding bugs / script gaps."""
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    fixture = _REPO / "tests" / "fixtures" / "web_server_flow.jsonl"
    cases = [json.loads(l) for l in fixture.read_text(encoding="utf-8").splitlines() if l.strip()]
    fixture_funcs = {c["func"] for c in cases}
    if fixture_funcs != set(PY):
        print("SELFTEST FATAL func set drift: fixture-only=%s map-only=%s"
              % (sorted(fixture_funcs - set(PY)), sorted(set(PY) - fixture_funcs)))
        sys.exit(2)
    counts = {}
    for c in cases:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {k: v for k, v in counts.items() if v < 5}
    if thin:
        print("SELFTEST FATAL <5 cases for: %s" % thin)
        sys.exit(2)
    install_stubs(ns)
    bad = 0
    failclosed = 0
    names = set(PY.values()) | set(PY) | set(SEAM_NAMES) | {"import_" + n for n in SEAM_NAMES}
    for c in cases:
        inp = c["in"]
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        CUR["statewin"] = None
        try:
            drive(ns, c["func"], inp)
        except BaseException as ex:
            spec = "%s: %s" % (type(ex).__name__, ex)
            if type(ex).__name__ == "AttributeError" and any(("'%s'" % n) in str(ex) for n in names):
                print("SELFTEST BINDING %s: %s" % (c["func"], spec))
                bad += 1
            elif type(ex).__name__ == "NameError":
                print("SELFTEST NAMEERROR %s: %s" % (c["func"], spec))
                bad += 1
        if any(e.get("ret", {}).get("msg", "").startswith("unscripted seam:")
               for e in CUR["trace"] if isinstance(e.get("ret"), dict)):
            failclosed += 1
    faulthandler.cancel_dump_traceback_later()
    print("SELFTEST %d cases, %d binding/name errors, %d cases hit fail-closed seams"
          % (len(cases), bad, failclosed))
    sys.exit(1 if bad else 0)


def main():
    if "--self-test" in sys.argv:
        self_test(extract())
        return
    ns = extract()
    install_stubs(ns)
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        CUR["statewin"] = None
        try:
            ret = drive(ns, func, inp)
            want = {"ret": norm(ret), "trace": norm(CUR["trace"])}
        except BaseException as ex:
            er = {"__raise__": type(ex).__name__, "msg": str(ex)}
            if isinstance(ex, SystemExit):
                er["code"] = ex.code
            want = {"ret": er, "trace": norm(CUR["trace"])}
        if func in STATEFUL:
            want["state"] = norm(CUR["statewin"]) if CUR["statewin"] is not None else {}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE web_server_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE web_server_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
