#!/usr/bin/env python3
"""Oracle for web_server_flow2 — the REMAINING UNCLAIMED web-server FLOW surface
of hermes_cli/web_server.py (LIVE authority, py3.14 quarry venv).

Ports covered: the uvicorn build / post-bind arming / server-runtime
orchestrators (_build_uvicorn_server, _on_server_started, _start_server_runtime)
plus their nested closures (_ws_ping_setting, _reap_desktop_serves,
_reap_mcp_helpers, _register_identity, _install_noise_filter, _loop_heartbeat,
_serve, _owned) and lambdas (profile_gate, profile_homes,
profile_homes_fallback, publish_rendezvous_step).

Claimed siblings (_best_effort, _gateway_owns_cron, _publish_host_rendezvous,
_configure_auth_gate, _apply_ssh_session_token, _apply_ssh_owner_nonce,
_run_serve, _port_bind_conflict, _report_port_in_use, _read_bound_port,
_start_parent_death_watchdog, _write_dashboard_ready_file,
_write_machine_sentinel_line, _maybe_open_browser, _dashboard_forwarded_allow_ips,
is_desktop_owned_backend) and foreign collaborators are SEAMED. UNCLAIMED
ported siblings composed here (_build_uvicorn_server inside
_start_server_runtime, _on_server_started inside _serve, _ws_ping_setting
inside _build_uvicorn_server) run for REAL on both sides.

Drives the LIVE functions with SCRIPTED I/O: defs AST-extracted from the quarry
(no heavy import), module constants seeded, every effect replaced by a
recording stub returning the SAME scripted results the C harness feeds its
universal seam. The async _serve closure is driven with asyncio.run. Compares
the ordered DECISION SEQUENCE (trace of seam calls) plus the return value (and
the mutated state window). Scripted {"__raise__": cls} raises a REAL exception;
drive() catches BaseException so KeyboardInterrupt/SystemExit pass through live
except-Exception clauses exactly as the C port propagates them. A MISSING seam
key fail-closes (RuntimeError) on both sides; a scripted null is a valid None.

Input: stdin JSON lines from t_port_web_server_flow2:
    {"func": ..., "in": {...}, "out": {"ret":..., "trace":[...][, "state":...]}}
Summary: ORACLE web_server_flow2: N cases, M mismatches. Fail-closed (exit 2)
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
from pathlib import Path, PurePosixPath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "web_server_flow2_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "web_server.py")
SRC_BOUNDARY = os.path.join(QUARRY, "hermes_cli", "web_server_boundary.py")
SRC_LIFECYCLE = os.path.join(QUARRY, "hermes_cli", "web_server_lifecycle.py")

# C harness func -> live python name (top-level / closure / lambda key).
PY = {
    "build_uvicorn_server": "_build_uvicorn_server",
    "ws_ping_setting": "_ws_ping_setting",
    "on_server_started": "_on_server_started",
    "reap_desktop_serves": "_reap_desktop_serves",
    "reap_mcp_helpers": "_reap_mcp_helpers",
    "register_identity": "_register_identity",
    "install_noise_filter": "_install_noise_filter",
    "loop_heartbeat": "_loop_heartbeat",
    "publish_rendezvous_step": "__lam_publish",
    "start_server_runtime": "_start_server_runtime",
    "serve": "_serve",
    "cron_owned": "_owned",
    "cron_profile_gate": "__lam_profile_gate",
    "cron_profile_homes": "__lam_profile_homes",
    "cron_own_home_fallback": "__lam_profile_homes_fallback",
}
WANTED_TOP = {"_build_uvicorn_server", "_on_server_started", "_start_server_runtime"}
# parent def -> nested closure names to extract
CLOSURES = {
    "_build_uvicorn_server": ["_ws_ping_setting"],
    "_on_server_started": ["_reap_desktop_serves", "_reap_mcp_helpers",
                           "_register_identity", "_install_noise_filter",
                           "_loop_heartbeat"],
    "_start_server_runtime": ["_serve"],
    "_start_desktop_cron_ticker": ["_owned"],
}
# lambda key -> (parent def, lineno)
LAMBDAS = {
    "__lam_profile_gate": ("_start_desktop_cron_ticker", 120),
    "__lam_profile_homes": ("_start_desktop_cron_ticker", 132),
    "__lam_profile_homes_fallback": ("_start_desktop_cron_ticker", 153),
    "__lam_publish": ("_on_server_started", 1294),
}
ASYNC = {"serve"}
STATEFUL = {"build_uvicorn_server", "on_server_started", "start_server_runtime", "serve"}

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
        if name == "SystemExit":
            raise SystemExit(r.get("code", r.get("msg", "")))
        cls = EXC_MAP.get(name)
        if cls is None:
            cls = type(name, (Exception,), {})
        if cls in (UnicodeDecodeError, UnicodeEncodeError, UnicodeTranslateError):
            e = cls.__new__(cls)
            Exception.__init__(e, r.get("msg", ""))
            raise e
        raise cls(r.get("msg", ""))
    return r


NS = {}


class PathDouble:
    def __init__(self, s):
        self.posix = PurePosixPath(s).as_posix() if s is not None else ""

    def __str__(self):
        return self.posix

    def __fspath__(self):
        return self.posix


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


class RecLogger:
    def debug(self, *a, **k): pass
    def info(self, *a, **k): pass
    def warning(self, *a, **k): pass
    def error(self, *a, **k): pass
    def exception(self, *a, **k): pass


class ConfigDouble:
    def __init__(self, fields):
        self.fields = dict(fields) if isinstance(fields, dict) else {}

    @property
    def loaded(self):
        return self.fields.get("loaded")

    def load(self):
        return seam("config_load", {})

    def lifespan_class(self, cfg):
        return seam("lifespan_class", {"config": proj(cfg)})


class ServerDouble:
    def __init__(self, fields):
        self.fields = dict(fields) if isinstance(fields, dict) else {}

    @property
    def should_exit(self):
        return self.fields.get("should_exit")

    @property
    def started(self):
        return self.fields.get("started")

    @property
    def lifespan(self):
        return self.fields.get("lifespan")

    @lifespan.setter
    def lifespan(self, v):
        self.fields["lifespan"] = v

    def capture_signals(self):
        seam("capture_signals", {})
        return ScopeCM("capture_signals_enter", "capture_signals_exit")

    async def startup(self):
        return seam("server_startup", {})

    async def main_loop(self):
        return seam("server_main_loop", {})

    async def shutdown(self):
        return seam("server_shutdown", {})


class LoopDouble:
    def __init__(self, marker):
        self.marker = marker

    def time(self):
        return seam("loop_time", {"loop": self.marker})

    def call_later(self, delay, fn, arg):
        return seam("loop_call_later", {"loop": self.marker, "delay": delay,
                                        "fn": proj(fn), "arg": arg})


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


def norm(o):
    if o is None or isinstance(o, (bool, int, str)):
        return o
    if isinstance(o, float):
        return o
    if isinstance(o, PathDouble):
        return o.posix
    if isinstance(o, Path):
        return o.as_posix()
    if isinstance(o, ConfigDouble):
        return {k: norm(v) for k, v in o.fields.items()}
    if isinstance(o, ServerDouble):
        return {k: norm(v) for k, v in o.fields.items()}
    if isinstance(o, LoopDouble):
        return norm(o.marker)
    if isinstance(o, RecLogger):
        return {"__logger__": "_log"}
    if getattr(o, "__ws2_app__", False):
        return {"__app__": "app"}
    if isinstance(o, types.SimpleNamespace):
        return {k: norm(v) for k, v in vars(o).items()}
    if isinstance(o, (frozenset, set)):
        return sorted(norm(v) for v in o)
    if callable(o):
        return {"__callable__": getattr(o, "__ws2_callable__", getattr(o, "__name__", True))}
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    return str(o)


def proj(o):
    """Project an object double to its JSON seam-arg view."""
    if isinstance(o, ConfigDouble):
        return {k: norm(v) for k, v in o.fields.items()}
    if isinstance(o, ServerDouble):
        return {k: norm(v) for k, v in o.fields.items()}
    if isinstance(o, LoopDouble):
        return norm(o.marker)
    if isinstance(o, RecLogger):
        return {"__logger__": "_log"}
    if isinstance(o, PathDouble):
        return o.posix
    if getattr(o, "__ws2_app__", False):
        return {"__app__": "app"}
    if callable(o):
        return {"__callable__": getattr(o, "__ws2_callable__", getattr(o, "__name__", True))}
    return norm(o)


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
    ns = {
        "__name__": "hermes_cli.web_server_extracted2",
        "json": json, "os": os, "re": re, "sys": sys, "time": time,
        "threading": threading, "logging": logging,
        "Path": PathDouble, "PurePosixPath": PurePosixPath,
        "Any": Any, "Dict": Dict, "Optional": Optional, "Tuple": Tuple,
        "_log": RecLogger(),
    }
    tree = ast.parse(src)
    # module constants (Assign + AnnAssign); failing ones skipped.
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
    for path, name in ((SRC_BOUNDARY, "_LOOPBACK_HOST_VALUES"),
                       (SRC_BOUNDARY, "_DESKTOP_ATTACHMENT_WS_MAX_BYTES"),
                       (SRC_LIFECYCLE, "PORT_IN_USE_EXIT_CODE")):
        if not _seed_constant(ns, path, name):
            print("FATAL missing sibling constant: %s" % name)
            sys.exit(2)
    ns["_log"] = RecLogger()   # re-set after the Assign loop

    top = {n.name: n for n in tree.body
           if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef))}
    # top-level targets
    for name in WANTED_TOP:
        if name not in top:
            print("FATAL missing top def: %s" % name)
            sys.exit(2)
        n = copy.copy(top[name])
        n.decorator_list = []
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    # nested closures (walk EVERY top-level def body, incl. claimed parents)
    found_closure = set()
    for pname, names in CLOSURES.items():
        if pname not in top:
            print("FATAL missing closure parent: %s" % pname)
            sys.exit(2)
        for sub in ast.walk(top[pname]):
            if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in names:
                n = copy.copy(sub)
                n.decorator_list = []
                exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
                found_closure.add(sub.name)
    need_closure = {nm for names in CLOSURES.values() for nm in names}
    if need_closure - found_closure:
        print("FATAL missing closures: %s" % sorted(need_closure - found_closure))
        sys.exit(2)
    # lambdas (locate by parent + lineno)
    for key, (pname, lineno) in LAMBDAS.items():
        lam = None
        for sub in ast.walk(top[pname]):
            if isinstance(sub, ast.Lambda) and sub.lineno == lineno:
                lam = sub
                break
        if lam is None:
            print("FATAL missing lambda %s at %s:%d" % (key, pname, lineno))
            sys.exit(2)
        expr = ast.Expression(body=lam)
        ast.fix_missing_locations(expr)
        ns[key] = eval(compile(expr, SRC, "eval"), ns)
    # tag the recursive heartbeat closure for callable projection
    if "_loop_heartbeat" in ns:
        ns["_loop_heartbeat"].__ws2_callable__ = "_loop_heartbeat"
    missing = (WANTED_TOP | need_closure | set(LAMBDAS)) - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    for k in ("_LOOPBACK_HOST_VALUES", "_DESKTOP_ATTACHMENT_WS_MAX_BYTES",
              "PORT_IN_USE_EXIT_CODE", "_DESKTOP_MCP_DISCOVERY_DELAY_S"):
        if k not in ns:
            print("FATAL missing constant: %s" % k)
            sys.exit(2)
    global NS
    NS = ns
    return ns


# ── recording stubs ──

def make_import_module(modname, attrs):
    """attrs: {attribute: (import_seam_name, fn_or___data__)}. getattr fires the
    import seam; '__data__' returns the seam RESULT. NO __path__ on leaf stubs."""
    class M(types.ModuleType):
        def __getattr__(self, name):
            if name in attrs:
                ise, fn = attrs[name]
                r = seam(ise, {})
                if fn == "__data__":
                    return r
                return fn
            raise AttributeError(name)
    m = M(modname)
    sys.modules[modname] = m
    return m


def _rec(name, argfn):
    def f(*a, **k):
        return seam(name, argfn(*a, **k))
    f.__name__ = name
    return f


def _uvicorn_config(app, **kw):
    args = {"app": proj(app)}
    args.update({k: proj(v) for k, v in kw.items()})
    r = seam("uvicorn_config", args)
    return ConfigDouble(r if isinstance(r, dict) else {})


def _uvicorn_server(config):
    r = seam("uvicorn_server", {"config": proj(config)})
    return ServerDouble(r if isinstance(r, dict) else {})


def install_stubs(ns):
    # claimed siblings + foreign bare globals
    ns["load_config"] = lambda: seam("load_config", {})
    ns["is_desktop_owned_backend"] = lambda: seam("is_desktop_owned_backend", {})
    ns["_gateway_owns_cron"] = lambda name, home: seam("gateway_owns_cron", {"name": name, "home": home.posix if isinstance(home, PathDouble) else home})
    ns["_publish_host_rendezvous"] = lambda host, port: seam("publish_host_rendezvous", {"host": host, "port": port})
    ns["_configure_auth_gate"] = lambda host, allow_public, tok, nonce: seam(
        "configure_auth_gate", {"host": host, "allow_public": allow_public,
                                "ssh_session_token": tok, "ssh_owner_nonce": nonce})
    ns["_apply_ssh_session_token"] = lambda token: seam("apply_ssh_session_token", {"token": token})
    ns["_apply_ssh_owner_nonce"] = lambda nonce: seam("apply_ssh_owner_nonce", {"nonce": nonce})
    ns["_port_bind_conflict"] = lambda host, port: seam("port_bind_conflict", {"host": host, "port": port})
    ns["_report_port_in_use"] = lambda host, port: seam("report_port_in_use", {"host": host, "port": port})
    ns["_read_bound_port"] = lambda server, fallback=None: seam("read_bound_port", {"server": proj(server), "fallback": fallback})
    ns["_start_parent_death_watchdog"] = lambda: seam("start_parent_death_watchdog", {})
    ns["_write_dashboard_ready_file"] = lambda port: seam("write_dashboard_ready_file", {"port": port})
    ns["_write_machine_sentinel_line"] = lambda text: seam("write_machine_sentinel_line", {"text": text})
    ns["_maybe_open_browser"] = lambda host, port, open_browser, initial_profile: seam(
        "maybe_open_browser", {"host": host, "port": port, "open_browser": open_browser,
                               "initial_profile": initial_profile})
    ns["_dashboard_forwarded_allow_ips"] = lambda dash_cfg: seam("dashboard_forwarded_allow_ips", {"dash_cfg": proj(dash_cfg)})
    ns["_uvicorn_console_logging"] = lambda: seam("uvicorn_console_logging", {})
    ns["profiles_to_serve"] = lambda multiplex=None: seam("profiles_to_serve", {"multiplex": multiplex})

    def _best_effort(what, fn):
        seam("best_effort", {"what": what, "fn": proj(fn)})
        return None
    ns["_best_effort"] = _best_effort

    def _run_serve(serve, config, host, port):
        return seam("run_serve", {"serve": proj(serve), "config": proj(config), "host": host, "port": port})
    ns["_run_serve"] = _run_serve

    def _print(text, flush=None):
        return seam("print", {"text": text, "flush": flush})
    ns["print"] = _print

    class AsyncioDouble:
        @staticmethod
        def get_running_loop():
            r = seam("get_running_loop", {})
            return LoopDouble(r)
    ns["asyncio"] = AsyncioDouble()
    ns["_log"] = RecLogger()

    # stub packages (parents carry __path__; from-target leaves do NOT)
    for pkg in ("hermes_cli", "tui_gateway", "uvicorn"):
        if pkg not in sys.modules or not isinstance(sys.modules[pkg], types.ModuleType):
            m = types.ModuleType(pkg)
            m.__path__ = []
            sys.modules[pkg] = m
        elif not hasattr(sys.modules[pkg], "__path__"):
            sys.modules[pkg].__path__ = []
    # uvicorn: plain import (silent); Config/Server are real attributes (recorders)
    sys.modules["uvicorn"].Config = _uvicorn_config
    sys.modules["uvicorn"].Server = _uvicorn_server

    make_import_module("hermes_cli.web_server_idle_exit", {
        "DEFAULT_IDLE_GRACE_S": ("import_DEFAULT_IDLE_GRACE_S", "__data__"),
        "TUNNEL_WS_PING_INTERVAL_S": ("import_TUNNEL_WS_PING_INTERVAL_S", "__data__"),
        "TUNNEL_WS_PING_TIMEOUT_S": ("import_TUNNEL_WS_PING_TIMEOUT_S", "__data__"),
        "start_idle_watchdog": ("import_start_idle_watchdog",
                                _rec("start_idle_watchdog", lambda server, tracker, grace_s=None:
                                     {"server": proj(server), "tracker": proj(tracker), "grace_s": grace_s})),
        "IdleClientTracker": ("import_IdleClientTracker",
                              _rec("idle_client_tracker", lambda: {})),
        "wrap_asgi_with_ws_tracking": ("import_wrap_asgi_with_ws_tracking",
                                       _rec("wrap_asgi_with_ws_tracking", lambda app, tracker:
                                            {"app": proj(app), "tracker": proj(tracker)})),
    })
    make_import_module("hermes_cli.web_server_skew_exit", {
        "start_code_skew_watchdog": ("import_start_code_skew_watchdog",
                                     _rec("start_code_skew_watchdog", lambda server: {"server": proj(server)})),
    })
    make_import_module("hermes_cli.web_server_owner_exit", {
        "start_owner_watchdog": ("import_start_owner_watchdog",
                                 _rec("start_owner_watchdog", lambda server, lock_path=None, nonce=None:
                                      {"server": proj(server), "lock_path": proj(lock_path), "nonce": nonce})),
    })
    make_import_module("hermes_cli.process_identity", {
        "attach_self_to_kill_on_close_job": ("import_attach_self_to_kill_on_close_job",
                                             _rec("attach_self_to_kill_on_close_job", lambda: {})),
        "register_self": ("import_register_self",
                          _rec("register_self", lambda role, detail=None: {"role": role, "detail": proj(detail)})),
        "reap_orphaned_mcp_helpers": ("import_reap_orphaned_mcp_helpers",
                                      _rec("reap_orphaned_mcp_helpers", lambda: {})),
    })
    make_import_module("hermes_cli.dashboard_procs", {
        "_reap_orphaned_desktop_local_serves": ("import_reap_orphaned_desktop_local_serves",
                                                _rec("reap_orphaned_desktop_local_serves", lambda: {})),
    })
    make_import_module("tui_gateway.loop_noise", {
        "install_loop_noise_filter": ("import_install_loop_noise_filter",
                                      _rec("install_loop_noise_filter", lambda loop: {"loop": proj(loop)})),
    })
    make_import_module("hermes_cli.url_utils", {
        "format_url_host": ("import_format_url_host", _rec("format_url_host", lambda host: {"host": host})),
    })
    make_import_module("hermes_cli.mcp_startup", {
        "defer_background_mcp_discovery": ("import_defer_background_mcp_discovery",
                                           _rec("defer_background_mcp_discovery",
                                                lambda logger=None, thread_name=None, delay=None:
                                                {"logger": proj(logger), "thread_name": thread_name, "delay": delay})),
    })
    make_import_module("hermes_cli.observability.shared_metrics_startup", {
        "record_process_ready": ("import_record_process_ready",
                                 _rec("record_process_ready", lambda name, background=None:
                                      {"name": name, "background": background})),
    })
    make_import_module("hermes_cli.update_proxy_env", {
        "restore_user_proxy_env": ("import_restore_user_proxy_env", _rec("restore_user_proxy_env", lambda: {})),
    })
    make_import_module("hermes_cli.resource_limits", {
        "apply_nofile_soft_limit": ("import_apply_nofile_soft_limit", _rec("apply_nofile_soft_limit", lambda: {})),
    })
    make_import_module("hermes_cli.nous_auth_keepalive", {
        "start_nous_auth_keepalive": ("import_start_nous_auth_keepalive", _rec("start_nous_auth_keepalive", lambda: {})),
    })
    make_import_module("hermes_cli.isolated_scope", {
        "record_launch_scope": ("import_record_launch_scope",
                                _rec("record_launch_scope", lambda isolated=None, desktop_owned=None:
                                     {"isolated": isolated, "desktop_owned": desktop_owned})),
    })
    make_import_module("tui_gateway.server", {
        "install_exit_flush_signal_handlers": ("import_install_exit_flush_signal_handlers",
                                               _rec("install_exit_flush_signal_handlers", lambda: {})),
    })
    make_import_module("tui_gateway.launch_profile_policy", {
        "activate_multi_profile_hosting_eagerly": ("import_activate_multi_profile_hosting_eagerly",
                                                   _rec("activate_multi_profile_hosting_eagerly", lambda: {})),
    })
    # observability package parent
    for pkg in ("hermes_cli.observability",):
        if pkg not in sys.modules:
            m = types.ModuleType(pkg)
            m.__path__ = []
            sys.modules[pkg] = m


# ── drive ──

def _app_double(inp):
    app = types.SimpleNamespace(state=types.SimpleNamespace(**(inp.get("app_state") or {})))
    app.__ws2_app__ = True
    return app


def _lock_path(inp):
    v = inp.get("ssh_lock_path")
    return PathDouble(v) if isinstance(v, str) else None


def drive(ns, func, inp):
    if func not in PY:
        print("FATAL unknown func: %s" % func)
        sys.exit(2)
    name = PY[func]
    ns["app"] = _app_double(inp)
    ns["_SSH_OWNER_NONCE"] = inp.get("ssh_owner_nonce")

    if func == "build_uvicorn_server":
        CUR["statewin"] = vars(ns["app"].state)
        return ns[name](inp.get("host", "127.0.0.1"), inp.get("port", 9119),
                        ssh_isolated=inp.get("ssh_isolated", False))
    if func == "ws_ping_setting":
        ns["_dash_cfg"] = inp.get("dash_cfg") or {}
        return ns[name](inp.get("key", "ws_ping_interval"), inp.get("default", 20.0))
    if func == "on_server_started":
        server = ServerDouble(inp.get("server") or {})
        CUR["statewin"] = vars(ns["app"].state)
        return ns[name](server, host=inp.get("host", "127.0.0.1"), port=inp.get("port", 9119),
                        headless=inp.get("headless", False), isolated=inp.get("isolated", False),
                        open_browser=inp.get("open_browser", False),
                        initial_profile=inp.get("initial_profile", ""),
                        start_mcp_discovery_after_bind=inp.get("start_mcp_discovery_after_bind", False),
                        ssh_lock_path=_lock_path(inp))
    if func == "reap_desktop_serves":
        return ns[name]()
    if func == "reap_mcp_helpers":
        return ns[name]()
    if func == "register_identity":
        ns["headless"] = inp.get("headless", False)
        ns["host"] = inp.get("host", "")
        ns["actual_port"] = inp.get("actual_port")
        ns["initial_profile"] = inp.get("initial_profile", "")
        ns["isolated"] = inp.get("isolated", False)
        return ns[name]()
    if func == "install_noise_filter":
        return ns[name]()
    if func == "loop_heartbeat":
        ns["_hb_loop"] = LoopDouble(inp.get("loop"))
        ns["_hb_interval"] = inp.get("interval", 2.0)
        ns["_hb_stall_threshold"] = inp.get("threshold", 5.0)
        ns["_log"] = RecLogger()
        return ns[name](inp.get("expected"))
    if func == "publish_rendezvous_step":
        ns["host"] = inp.get("host", "")
        ns["actual_port"] = inp.get("actual_port")
        return ns[name]()
    if func == "start_server_runtime":
        CUR["statewin"] = vars(ns["app"].state)
        return ns[name](host=inp.get("host", "127.0.0.1"), port=inp.get("port", 9119),
                        open_browser=inp.get("open_browser", True),
                        allow_public=inp.get("allow_public", False),
                        initial_profile=inp.get("initial_profile", ""),
                        headless=inp.get("headless", False), isolated=inp.get("isolated", False),
                        ssh_session_token=inp.get("ssh_session_token"),
                        ssh_owner_nonce=inp.get("ssh_owner_nonce"),
                        start_mcp_discovery_after_bind=inp.get("start_mcp_discovery_after_bind", False),
                        ssh_lock_path=_lock_path(inp))
    if func == "serve":
        config = ConfigDouble(inp.get("config") or {})
        server = ServerDouble(inp.get("server") or {})
        ns["config"] = config
        ns["server"] = server
        ns["host"] = inp.get("host", "127.0.0.1")
        ns["port"] = inp.get("port", 9119)
        ns["headless"] = inp.get("headless", False)
        ns["isolated"] = inp.get("isolated", False)
        ns["open_browser"] = inp.get("open_browser", False)
        ns["initial_profile"] = inp.get("initial_profile", "")
        ns["start_mcp_discovery_after_bind"] = inp.get("start_mcp_discovery_after_bind", False)
        ns["ssh_lock_path"] = _lock_path(inp)
        CUR["statewin"] = {"server": server.fields, "app_state": vars(ns["app"].state)}
        return asyncio.run(ns[name]())
    if func == "cron_owned":
        ns["own_name"] = inp.get("own_name", "default")
        ns["own_home"] = PathDouble(inp.get("own_home", ""))
        ns["_log"] = RecLogger()
        return ns[name]()
    if func == "cron_profile_gate":
        return ns[name](inp.get("name", "default"), inp.get("home", ""))
    if func == "cron_profile_homes":
        return ns[name]()
    if func == "cron_own_home_fallback":
        ns["own_name"] = inp.get("own_name", "default")
        ns["own_home"] = PathDouble(inp.get("own_home", ""))
        return ns[name]()
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
    "load_config", "is_desktop_owned_backend", "best_effort", "start_parent_death_watchdog",
    "read_bound_port", "write_dashboard_ready_file", "write_machine_sentinel_line",
    "maybe_open_browser", "format_url_host", "print", "gateway_owns_cron",
    "publish_host_rendezvous", "configure_auth_gate", "apply_ssh_session_token",
    "apply_ssh_owner_nonce", "port_bind_conflict", "report_port_in_use", "run_serve",
    "dashboard_forwarded_allow_ips", "uvicorn_console_logging", "uvicorn_config",
    "uvicorn_server", "idle_client_tracker", "wrap_asgi_with_ws_tracking",
    "start_idle_watchdog", "start_code_skew_watchdog", "start_owner_watchdog",
    "register_self", "attach_self_to_kill_on_close_job",
    "reap_orphaned_desktop_local_serves", "reap_orphaned_mcp_helpers",
    "install_loop_noise_filter", "get_running_loop", "loop_time", "loop_call_later",
    "defer_background_mcp_discovery", "record_process_ready", "profiles_to_serve",
    "restore_user_proxy_env", "apply_nofile_soft_limit", "start_nous_auth_keepalive",
    "record_launch_scope", "install_exit_flush_signal_handlers",
    "activate_multi_profile_hosting_eagerly", "config_load", "lifespan_class",
    "capture_signals", "capture_signals_enter", "capture_signals_exit",
    "server_startup", "server_main_loop", "server_shutdown",
]


def self_test(ns):
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    fixture = _REPO / "tests" / "fixtures" / "web_server_flow2.jsonl"
    cases = [json.loads(l) for l in fixture.read_text(encoding="utf-8").split("\n") if l.strip()]
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
        print("ORACLE web_server_flow2: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE web_server_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()