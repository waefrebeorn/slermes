#!/usr/bin/env python3
"""Oracle for the hermes_cli/main.py UNCLAIMED-remainder FLOW port
(src/cli/main_cli_flow2.c).

Drives the LIVE main.py defs with SCRIPTED seams: every collaborator (claimed
siblings, foreign-module imports, os/sys/subprocess/threading/logging/json reads,
print, path methods, logger) is a recording stub fed the same scripts the C
harness feeds mc2_call. Compares the DECISION SEQUENCE (ordered "trace" of
{ev,args,ret} seam events), the canonical return ("ret") and the post-call
out-state ("state"). The ported functions call each other REAL (AST-extracted
into one namespace), so internal composition (_register_plugin_cli_commands ->
_first_positional_argv / _attach_plugin_cli_command /
_resolve_deferred_platform_cli_command; _resolve_chat_session_args -> _session_db)
is exercised, matching the C port's internal transcription.

Live defs are AST-extracted from the quarry (annotations kept as strings via the
__future__ flag); only the needed module constants are seeded. The heavy quarry
import chain is never imported. Every exec namespace is a registered sys.modules
module (py3.14 dataclasses _is_type needs a real module alias). Fail-closed:
0 cases => exit 1; a missing seam script raises RuntimeError("no script for X");
any harness/oracle exception per case rides as {"__raise__": cls} (BaseException,
so SystemExit/KeyboardInterrupt/GeneratorExit propagate).
Summary: 'ORACLE main_cli_flow2: N cases, M mismatches'.

Representation contract (both sides, see src/cli/main_cli_flow2.c):
- attribute doubles (args/store/reader/session/active_def/result/parser/manager)
  are types.SimpleNamespace records, ALWAYS truthy when present (even {}); plain
  dict inputs (config/registry/custom_provider_map/cmd_info/provider_labels) use
  JSON truthiness. tuple/set -> array (sets sorted); None -> null; integral floats
  collapse to ints; Path -> POSIX str; uncaught exception -> {"__raise__": cls};
  an exception passed as a logger arg projects to {"__raise__": cls, "msg": str(e)}.
- scripts (FAIL-CLOSED): absent key -> RuntimeError("no script for <name>");
  present null -> None; {"$seq": [...]} per-call queue (exhaustion raises
  RuntimeError("script exhausted: <name>")); {"__raise__": cls, "msg": m, ...}
  raises (trace ret is the WHOLE marker); any other value reused. None-valued arg
  keys dropped on both sides.
- json.loads rides the "json.loads" seam (scripted parse result).
- fixture domain: ASCII operands; setup_fn/handler_fn are string markers or null;
  object-double fields are strings/numbers/bools/null/records.
"""
import __future__
import argparse
import ast
import builtins
import contextlib
import json
import logging
import os
import sys
import types
from pathlib import Path, PurePath, PurePosixPath
from typing import Optional

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parents[1]
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "hermes_cli" / "main.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "hermes_cli" / "main.py")
# scrubbed-env safety: the quarry chain can read Path.home(); point it at scratch.
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "mc2_home"))

NAME = "main_cli_flow2"

PORTED = [
    "_warn_if_unsupervised_pid1", "_read_git_revision_fingerprint",
    "_auth_store_logged_in", "_resolve_workspace_key", "_session_db",
    "_resolve_chat_session_args", "_warn_retired_xai_models",
    "_start_chat_background_prefetch", "_forward_command",
    "_resolve_active_provider", "_pick_provider", "_dashboard_prepare_runtime",
    "_first_positional_argv", "_resolve_deferred_platform_cli_command",
    "_agent_subcommand_selected", "_chat_runs_at_gateway",
    "_apply_user_config_bypass", "_run_oneshot_from_args", "_enable_cpu_profile",
    "_light_chat_parser", "_promote_top_level_resume", "_try_fast_serve_launch",
    "_attach_plugin_cli_command", "_register_plugin_cli_commands",
]
CONSTANTS = ["_AGENT_SUBCOMMANDS"]
WANTED = set(PORTED)

logging.disable(logging.CRITICAL)

# ── AST extraction ──────────────────────────────────────────────────────────
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=_FLAGS, dont_inherit=True)


DEF_CODE = {}
CONST_CODE = {}
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in WANTED:
        # keep decorators (notably @contextlib.contextmanager on _session_db, so
        # `with _session_db()` composition works); direct drive uses __wrapped__.
        DEF_CODE[_node.name] = _compile_node(_node, SRC)
    elif isinstance(_node, (ast.Assign, ast.AnnAssign)):
        tg = _node.targets[0] if isinstance(_node, ast.Assign) else _node.target
        if isinstance(tg, ast.Name) and tg.id in CONSTANTS:
            CONST_CODE[tg.id] = _compile_node(_node, SRC)
_missing = (WANTED - set(DEF_CODE)) | (set(CONSTANTS) - set(CONST_CODE))
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)


# ── canonical projection ────────────────────────────────────────────────────


def norm(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
    if isinstance(v, PathDouble):
        return v.s
    if isinstance(v, PurePath):
        return str(v)
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, (types.SimpleNamespace, StoreDouble)):
        return {k: norm(x) for k, x in vars(v).items() if not callable(x)}
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


class AuthError(Exception):
    def __init__(self, msg="", code=None):
        super().__init__(msg)
        self.code = code


def make_exc(marker):
    cls_name = marker.get("__raise__", "RuntimeError")
    msg = marker.get("msg", "")
    if cls_name == "AuthError":
        return AuthError(msg, code=marker.get("code"))
    if cls_name == "ArgumentError":
        return argparse.ArgumentError(None, msg)
    cls = getattr(builtins, cls_name, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(cls_name, (Exception,), {})
    return cls(msg)


# ── scripted seams (FAIL-CLOSED) ────────────────────────────────────────────
_SCRIPTS = None


class Scripts:
    def __init__(self, io, trace):
        self.io = io or {}
        self.trace = trace
        self.cursors = {}

    def next(self, name):
        script = self.io.get(name)
        if isinstance(script, dict) and "$seq" in script:
            seq = script["$seq"]
            i = self.cursors.get(name, 0)
            if i >= len(seq):
                return {"__raise__": "RuntimeError", "msg": f"script exhausted: {name}"}
            self.cursors[name] = i + 1
            return seq[i]
        return script


def record(name, args):
    scripts = _SCRIPTS
    args = {k: norm(v) for k, v in args.items() if v is not None}
    if name not in scripts.io:
        marker = {"__raise__": "RuntimeError", "msg": f"no script for {name}"}
        scripts.trace.append({"ev": name, "args": args, "ret": marker})
        raise make_exc(marker)
    script = scripts.next(name)
    if isinstance(script, dict) and isinstance(script.get("__raise__"), str):
        marker = dict(script)
        scripts.trace.append({"ev": name, "args": args, "ret": marker})
        raise make_exc(marker)
    result = materialize(name, script)
    scripts.trace.append({"ev": name, "args": args, "ret": norm(script)})
    return result


def _rec(name, argmap):
    def f(*a, **kw):
        return record(name, argmap(a, kw))
    return f


class SeamFn:
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **kw):
        return record(self.name, SEAM_ARGS[self.name](a, kw))


# ── object doubles ──────────────────────────────────────────────────────────


class PathDouble:
    """POSIX path double: / is pure join; filesystem methods ride seams."""
    def __init__(self, s):
        self.s = str(s)

    def __truediv__(self, other):
        return PathDouble(str(PurePosixPath(self.s) / str(other)))

    def __str__(self):
        return self.s

    def __fspath__(self):
        return self.s

    def is_file(self):
        return record("path.is_file", {"path": self.s})

    def exists(self):
        return record("path.exists", {"path": self.s})

    def is_dir(self):
        return record("path.is_dir", {"path": self.s})

    def read_text(self, encoding=None, errors=None):
        r = record("path.read_text", {"path": self.s, "encoding": encoding, "errors": errors})
        if isinstance(r, str) and encoding == "utf-8-sig" and r.startswith("\ufeff"):
            r = r[1:]
        return r

    def resolve(self):
        r = record("path.resolve", {"path": self.s})
        return PathDouble(r) if isinstance(r, str) else r

    def rglob(self, pattern):
        r = record("path.rglob", {"path": self.s, "pattern": pattern})
        return iter(r) if isinstance(r, list) else r


class StoreDouble:
    def __init__(self, d):
        self.__dict__.update(d or {})
        self.ready = _rec("store.ready", lambda a, k: {})

    def __enter__(self):
        return self

    def __exit__(self, *a):
        record("store.close", {})
        return False


class ForwardFn:
    def __init__(self, attr):
        self.attr = attr
        self.__name__ = attr

    def __call__(self, args):
        return record("forward.call", {"attr": self.attr, "args": args})


class ModDouble:
    def __init__(self, d):
        self.__dict__.update(d or {})

    def __getattr__(self, attr):
        return ForwardFn(attr)


class NamedFn:
    def __init__(self, name):
        self.__name__ = name

    def __call__(self, *a, **kw):
        raise AssertionError(f"NamedFn {self.__name__} should not be called")


class FnDouble:
    def __init__(self, name, seam, argmap):
        self.__name__ = name
        self._seam = seam
        self._argmap = argmap

    def __call__(self, *a, **kw):
        return record(self._seam, self._argmap(a, kw))


class ThreadDouble:
    def __init__(self, target=None, name=None, daemon=None):
        self.target = target
        self.name = name
        self.daemon = daemon

    def start(self):
        return record("thread.start", {"name": self.name, "daemon": self.daemon,
                                       "target": getattr(self.target, "__name__", str(self.target))})


class ThreadingDouble:
    Thread = ThreadDouble


class LoggerDouble:
    def _log(self, level, msg, args, kw):
        d = {"msg": msg, "args": list(args)}
        if kw.get("exc_info"):
            d["exc_info"] = True
        return record("logger." + level, d)

    def debug(self, msg, *args, **kw):
        return self._log("debug", msg, args, kw)

    def warning(self, msg, *args, **kw):
        return self._log("warning", msg, args, kw)

    def info(self, msg, *args, **kw):
        return self._log("info", msg, args, kw)


_LOGGER_SINGLETON = LoggerDouble()


class LoggingDouble:
    def getLogger(self, name=None):
        return _LOGGER_SINGLETON


class EnvironDouble:
    def get(self, name, default=None):
        return record("os.environ.get", {"name": name, "default": default})

    def __setitem__(self, key, value):
        return record("os.environ.set", {"key": key, "value": value})


class PathOpsDouble:
    def abspath(self, p):
        return record("os.path.abspath", {"path": p})

    def isdir(self, p):
        return record("os.path.isdir", {"path": p})

    def realpath(self, p):
        return record("os.path.realpath", {"path": p})

    def chdir(self, p):
        return record("os.chdir", {"path": p})


class OsDouble:
    def __init__(self):
        self.environ = EnvironDouble()
        self.path = PathOpsDouble()

    def getpid(self):
        return record("os.getpid", {})

    def getcwd(self):
        return record("os.getcwd", {})

    def chdir(self, p):
        return record("os.chdir", {"path": p})


class StderrDouble:
    def write(self, text):
        return record("sys.stderr.write", {"text": text})


class SysDouble:
    def __init__(self):
        self.argv = ["hermes"]
        self.stderr = StderrDouble()

    def exit(self, code=None):
        record("sys.exit", {"code": code})
        raise SystemExit(code)


class SubprocessDouble:
    run = SeamFn("subprocess.run")


class JsonDouble:
    loads = SeamFn("json.loads")


class BannerModule(types.ModuleType):
    def __setattr__(self, name, value):
        if name == "_available_skills_cache" and _SCRIPTS is not None:
            record("banner.set_cache", {})
        super().__setattr__(name, value)


def mk_parser(script):
    if isinstance(script, dict):
        ns = types.SimpleNamespace(**script)
        ns.set_defaults = _rec("parser.set_defaults",
                               lambda a, k: {"func": getattr(k["func"], "__name__", str(k["func"]))})
        ns.parse_known_args = _rec("parser.parse_known_args", lambda a, k: {"argv": a[0]})
        return ns
    return script


def mk_reader(script):
    if not isinstance(script, dict):
        return script
    ns = types.SimpleNamespace()
    for k, v in script.items():
        if k == "persistence" and isinstance(v, dict):
            p = types.SimpleNamespace()
            for pk, pv in v.items():
                if pk == "sessions" and isinstance(pv, dict):
                    s = types.SimpleNamespace()
                    s.get = _rec("sessions.get", lambda a, kk: {"session_id": a[0]})
                    setattr(p, pk, s)
                else:
                    setattr(p, pk, pv)
            setattr(ns, k, p)
        else:
            setattr(ns, k, v)
    return ns


def materialize_cmd_info(d):
    if not isinstance(d, dict):
        return d
    c = dict(d)
    if isinstance(c.get("setup_fn"), str):
        c["setup_fn"] = FnDouble(c["setup_fn"], "plugin.setup_fn", lambda a, k: {"parser": a[0]})
    if isinstance(c.get("handler_fn"), str):
        c["handler_fn"] = NamedFn(c["handler_fn"])
    return c


def mk_registry(d):
    if not isinstance(d, dict):
        return d
    return {k: (types.SimpleNamespace(**v) if isinstance(v, dict) else v) for k, v in d.items()}


def mk_args(d):
    return types.SimpleNamespace(**(d or {}))


def materialize(name, script):
    if script is None:
        return None
    if name == "subprocess.run":
        return types.SimpleNamespace(**script) if isinstance(script, dict) else script
    if name == "open_persistence_reader":
        return StoreDouble(script) if isinstance(script, dict) else script
    if name == "SurfaceLabelReader":
        return mk_reader(script)
    if name == "sessions.get":
        return types.SimpleNamespace(**script) if isinstance(script, dict) else script
    if name == "resolve_provider_full":
        return types.SimpleNamespace(**script) if isinstance(script, dict) else script
    if name == "_build_provider_picker_rows":
        if isinstance(script, (list, tuple)) and len(script) == 2:
            ordered = [tuple(row) for row in script[0]]
            return (ordered, script[1])
        return script
    if name == "import_module":
        return ModDouble(script if isinstance(script, dict) else {})
    if name == "build_top_level_parser":
        if isinstance(script, (list, tuple)):
            return tuple(mk_parser(x) for x in script)
        return script
    if name in ("build_serve_parser", "subparsers.add_parser"):
        return mk_parser(script)
    if name == "parser.parse_known_args":
        if isinstance(script, (list, tuple)) and len(script) == 2:
            a0 = types.SimpleNamespace(**script[0]) if isinstance(script[0], dict) else script[0]
            return (a0, script[1])
        return script
    if name == "get_plugin_manager":
        if isinstance(script, dict):
            clc = {k: materialize_cmd_info(v) for k, v in (script.get("_cli_commands") or {}).items()}
            return types.SimpleNamespace(_cli_commands=clc)
        return script
    if name == "discover_plugin_cli_commands":
        if isinstance(script, list):
            return [materialize_cmd_info(d) for d in script]
        return script
    if name == "path.rglob":
        return iter(script) if isinstance(script, list) else script
    if name == "path.resolve":
        return PathDouble(script) if isinstance(script, str) else script
    return script


# ── seam arg projections (SeamFn-installed collaborators) ───────────────────
SEAM_ARGS = {
    "print": lambda a, k: {"text": a[0], "file": "stderr" if k.get("file") is not None else None},
    "_read_packed_ref": lambda a, k: {"common_dir": a[0], "ref": a[1]},
    "get_auth_status": lambda a, k: {"provider": a[0]},
    "_apply_in_dir": lambda a, k: {"args": a[0]},
    "_latest_session_id": lambda a, k: {"use_tui": a[0]},
    "_resolve_continue_arg": lambda a, k: {"args": a[0], "use_tui": k["use_tui"]},
    "import_foreign_resume": lambda a, k: {"args": a[0]},
    "_resolve_session_by_name_or_id": lambda a, k: {"name_or_id": a[0]},
    "recreate_resumed_tui_worktree": lambda a, k: {"args": a[0]},
    "load_config": lambda a, k: {},
    "find_retired_xai_refs": lambda a, k: {"config": a[0]},
    "format_issue": lambda a, k: {"ref": a[0]},
    "_termux_should_prefetch_update_check": lambda a, k: {},
    "prefetch_update_check": lambda a, k: {},
    "prefetch_banner_data": lambda a, k: {},
    "_sync_bundled_skills_for_startup": lambda a, k: {},
    "import_module": lambda a, k: {"module": a[0]},
    "_norm_base_url": lambda a, k: {"url": a[0]},
    "get_compatible_custom_providers": lambda a, k: {"config": a[0]},
    "resolve_provider_full": lambda a, k: {"provider": a[0], "providers": a[1], "compatible": a[2]},
    "custom_provider_aliases": lambda a, k: {"name": a[0], "provider_key": a[1]},
    "provider_install_hint": lambda a, k: {"provider": a[0]},
    "resolve_provider": lambda a, k: {"provider": a[0]},
    "format_auth_error": lambda a, k: {"exc": a[0]},
    "get_env_value": lambda a, k: {"name": a[0]},
    "_build_provider_picker_rows": lambda a, k: {"config": a[0], "active": a[1],
                                                 "provider_labels": a[2], "custom_provider_map": a[3]},
    "_prompt_provider_choice": lambda a, k: {"labels": a[0], "default": k["default"], "title": k.get("title")},
    "setup_logging": lambda a, k: {"mode": k["mode"]},
    "_require_dashboard_web_deps": lambda a, k: {},
    "_sync_bundled_skills_quietly": lambda a, k: {},
    "apply_terminal_config_to_env": lambda a, k: {},
    "_resolve_dashboard_web_dist": lambda a, k: {"args": a[0], "headless_backend": a[1]},
    "discover_plugins": lambda a, k: {},
    "_is_desktop_owned_backend": lambda a, k: {},
    "start_background_mcp_discovery": lambda a, k: {"logger": "logger", "thread_name": k["thread_name"]},
    "defer_background_mcp_discovery": lambda a, k: {"logger": "logger", "thread_name": k["thread_name"]},
    "command_argv": lambda a, k: {"argv": a[0]},
    "recover_for_command": lambda a, k: {"command": a[0]},
    "discover_plugin_cli_commands": lambda a, k: {},
    "get_plugin_manager": lambda a, k: {},
    "launch_gateway_chat": lambda a, k: {"args": a[0]},
    "enable_cpu_profile_from_args": lambda a, k: {"args": a[0]},
    "build_top_level_parser": lambda a, k: {},
    "get_container_exec_info": lambda a, k: {},
    "build_serve_parser": lambda a, k: {"cmd_dashboard": getattr(k["cmd_dashboard"], "__name__", str(k["cmd_dashboard"])),
                                        "add_help": k["add_help"], "exit_on_error": k["exit_on_error"]},
    "cmd_dashboard": lambda a, k: {"args": a[0]},
    "_plugin_cli_discovery_needed": lambda a, k: {},
    "get_hermes_home": lambda a, k: {},
    "open_persistence_reader": lambda a, k: {"home": a[0]},
    "SurfaceLabelReader": lambda a, k: {"store": a[0]},
    "json.loads": lambda a, k: {"text": a[0]},
    "subprocess.run": lambda a, k: {"argv": a[0], "capture_output": k["capture_output"], "text": k["text"],
                                    "encoding": k["encoding"], "errors": k["errors"],
                                    "timeout": k["timeout"], "check": k["check"]},
}


# ── stub modules for lazy imports ───────────────────────────────────────────
def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def install_stubs():
    hc = _mod("hermes_cli")
    hc.__path__ = []
    auth = _mod("hermes_cli.auth", AuthError=AuthError,
                get_auth_status=SeamFn("get_auth_status"),
                resolve_provider=SeamFn("resolve_provider"),
                format_auth_error=SeamFn("format_auth_error"))
    cfg = _mod("hermes_cli.config", load_config=SeamFn("load_config"),
               get_env_value=SeamFn("get_env_value"),
               get_compatible_custom_providers=SeamFn("get_compatible_custom_providers"),
               apply_terminal_config_to_env=SeamFn("apply_terminal_config_to_env"),
               get_container_exec_info=SeamFn("get_container_exec_info"),
               get_hermes_home=SeamFn("get_hermes_home"))
    prov = _mod("hermes_cli.providers", resolve_provider_full=SeamFn("resolve_provider_full"),
                custom_provider_aliases=SeamFn("custom_provider_aliases"))
    lcm = _mod("hermes_cli.left_core_migration", provider_install_hint=SeamFn("provider_install_hint"),
               recover_for_command=SeamFn("recover_for_command"))
    mra = _mod("hermes_cli.main_resume_args", import_foreign_resume=SeamFn("import_foreign_resume"),
               recreate_resumed_tui_worktree=SeamFn("recreate_resumed_tui_worktree"))
    xai = _mod("hermes_cli.xai_retirement", MIGRATION_GUIDE_URL="", RETIREMENT_DATE="",
               find_retired_xai_refs=SeamFn("find_retired_xai_refs"),
               format_issue=SeamFn("format_issue"))
    banner = BannerModule("hermes_cli.banner")
    banner.prefetch_update_check = SeamFn("prefetch_update_check")
    banner.prefetch_banner_data = SeamFn("prefetch_banner_data")
    banner._available_skills_cache = "initial"
    sys.modules["hermes_cli.banner"] = banner
    plugins = _mod("hermes_cli.plugins", discover_plugins=SeamFn("discover_plugins"),
                   get_plugin_manager=SeamFn("get_plugin_manager"))
    mcp = _mod("hermes_cli.mcp_startup",
               start_background_mcp_discovery=SeamFn("start_background_mcp_discovery"),
               defer_background_mcp_discovery=SeamFn("defer_background_mcp_discovery"))
    gcs = _mod("hermes_cli.gateway_chat_startup", launch_gateway_chat=SeamFn("launch_gateway_chat"))
    cpu = _mod("hermes_cli.cpu_profile", enable_cpu_profile_from_args=SeamFn("enable_cpu_profile_from_args"))
    parser = _mod("hermes_cli._parser", command_argv=SeamFn("command_argv"),
                  build_top_level_parser=SeamFn("build_top_level_parser"))
    gss = _mod("hermes_cli.gateway_session_store", SurfaceLabelReader=SeamFn("SurfaceLabelReader"))
    for nm, mod in (("auth", auth), ("config", cfg), ("providers", prov),
                    ("left_core_migration", lcm), ("main_resume_args", mra),
                    ("xai_retirement", xai), ("banner", banner), ("plugins", plugins),
                    ("mcp_startup", mcp), ("gateway_chat_startup", gcs), ("cpu_profile", cpu),
                    ("_parser", parser), ("gateway_session_store", gss)):
        setattr(hc, nm, mod)
    _mod("hermes_logging", setup_logging=SeamFn("setup_logging"))
    _mod("persistence_assembly", open_persistence_reader=SeamFn("open_persistence_reader"))
    gw = _mod("gateway")
    gw.__path__ = []
    pr = _mod("gateway.platform_registry",
              platform_registry=types.SimpleNamespace(get=_rec("platform_registry.get", lambda a, k: {"name": a[0]})))
    gw.platform_registry = pr
    pl = _mod("plugins")
    pl.__path__ = []
    pm = _mod("plugins.memory", discover_plugin_cli_commands=SeamFn("discover_plugin_cli_commands"))
    pl.memory = pm
    _mod("importlib", import_module=SeamFn("import_module"))


# ── base namespace ──────────────────────────────────────────────────────────
_OS_DOUBLE = OsDouble()
_SYS_DOUBLE = SysDouble()
_BASE_NS = {
    "__name__": "mc2_live_main_cli",
    "__builtins__": builtins,
    "contextlib": contextlib,
    "argparse": argparse,
    "threading": ThreadingDouble(),
    "logging": LoggingDouble(),
    "logger": _LOGGER_SINGLETON,
    "os": _OS_DOUBLE,
    "sys": _SYS_DOUBLE,
    "subprocess": SubprocessDouble(),
    "json": JsonDouble(),
    "Path": PathDouble,
    "Optional": Optional,
    "print": SeamFn("print"),
}
_MOD = types.ModuleType("mc2_live_main_cli")
_MOD.__dict__.update(_BASE_NS)
sys.modules["mc2_live_main_cli"] = _MOD
_BASE_NS = _MOD.__dict__
for _nm, _code in CONST_CODE.items():
    exec(_code, _BASE_NS)
for _nm, _code in DEF_CODE.items():
    exec(_code, _BASE_NS)
_BASE_NS["logger"] = _LOGGER_SINGLETON

# claimed siblings + top-level imported collaborators -> SeamFns
_COLLABS = [
    "_apply_in_dir", "_latest_session_id", "_resolve_continue_arg",
    "_resolve_session_by_name_or_id", "_norm_base_url", "_read_packed_ref",
    "_termux_should_prefetch_update_check", "_sync_bundled_skills_for_startup",
    "_require_dashboard_web_deps", "_sync_bundled_skills_quietly",
    "_resolve_dashboard_web_dist", "_is_desktop_owned_backend",
    "_build_provider_picker_rows", "_prompt_provider_choice",
    "_plugin_cli_discovery_needed", "build_serve_parser", "get_hermes_home",
]
for _nm in _COLLABS:
    _BASE_NS[_nm] = SeamFn(_nm)
# cmd_chat / cmd_dashboard are claimed siblings passed as VALUES (projected by name)
_BASE_NS["cmd_chat"] = NamedFn("cmd_chat")
_cd = SeamFn("cmd_dashboard")
_cd.__name__ = "cmd_dashboard"
_BASE_NS["cmd_dashboard"] = _cd
install_stubs()


def build_ns(rec):
    global _SCRIPTS
    inp = rec["in"]
    env = inp.get("env") or {}
    trace = []
    scripts = Scripts(inp.get("io") or {}, trace)
    _SCRIPTS = scripts
    _SYS_DOUBLE.argv = ["hermes"] + list(env.get("argv") or [])
    xai = sys.modules["hermes_cli.xai_retirement"]
    xai.MIGRATION_GUIDE_URL = env.get("MIGRATION_GUIDE_URL", "")
    xai.RETIREMENT_DATE = env.get("RETIREMENT_DATE", "")
    return _BASE_NS, trace, scripts


# ── live driver ─────────────────────────────────────────────────────────────
def drive(rec, ns, scripts, holder):
    func = rec["func"]
    inp = rec["in"]
    if func == "_warn_if_unsupervised_pid1":
        ns[func](inp.get("pid"))
        return None
    if func == "_read_git_revision_fingerprint":
        return ns[func](PathDouble(inp.get("repo_root", "")))
    if func == "_auth_store_logged_in":
        return ns[func](PathDouble(inp.get("auth_file", "")), mk_registry(inp.get("registry") or {}),
                        inp.get("strict_profile_scope", False))
    if func == "_resolve_workspace_key":
        return ns[func]()
    if func == "_session_db":
        gen = ns[func].__wrapped__()
        reader = next(gen)
        br = inp.get("body_raise")
        if br:
            try:
                gen.throw(make_exc(br))
            except StopIteration:
                pass
            return reader
        try:
            next(gen)
        except StopIteration:
            pass
        return reader
    if func == "_resolve_chat_session_args":
        args = mk_args(inp.get("args"))
        ns[func](args, inp.get("use_tui", False))
        holder["state"] = {"args": norm(args)}
        return None
    if func == "_warn_retired_xai_models":
        ns[func]()
        return None
    if func == "_start_chat_background_prefetch":
        ns[func]()
        return None
    if func == "_forward_command":
        fn = ns[func](inp.get("name", ""), inp.get("module", ""), inp.get("attr", ""),
                      forward_return=inp.get("forward_return", False), doc=inp.get("doc", ""))
        meta = {"name": fn.__name__, "qualname": fn.__qualname__, "doc": fn.__doc__}
        r2 = fn(mk_args(inp.get("call_args")))
        meta["call_ret"] = norm(r2)
        return meta
    if func == "_resolve_active_provider":
        return ns[func](inp.get("config") or {}, inp.get("model_cfg"),
                        inp.get("effective_provider", ""), inp.get("custom_provider_map") or {})
    if func == "_pick_provider":
        return ns[func](inp.get("config") or {}, inp.get("active"),
                        inp.get("provider_labels") or {}, inp.get("custom_provider_map") or {})
    if func == "_dashboard_prepare_runtime":
        return ns[func](mk_args(inp.get("args")), inp.get("headless_backend", False))
    if func == "_first_positional_argv":
        return ns[func]()
    if func == "_resolve_deferred_platform_cli_command":
        ns[func](inp.get("command_name"))
        return None
    if func == "_agent_subcommand_selected":
        return ns[func](mk_args(inp.get("args")))
    if func == "_chat_runs_at_gateway":
        return ns[func](mk_args(inp.get("args")))
    if func == "_apply_user_config_bypass":
        ns[func](mk_args(inp.get("args")))
        return None
    if func == "_run_oneshot_from_args":
        ns[func](mk_args(inp.get("args")))
        return None
    if func == "_enable_cpu_profile":
        ns[func](mk_args(inp.get("args")))
        return None
    if func == "_light_chat_parser":
        return ns[func]()
    if func == "_promote_top_level_resume":
        args = mk_args(inp.get("args"))
        ns[func](args)
        holder["state"] = {"args": norm(args)}
        return None
    if func == "_try_fast_serve_launch":
        return ns[func]()
    if func == "_attach_plugin_cli_command":
        sp = types.SimpleNamespace()
        sp.add_parser = _rec("subparsers.add_parser", lambda a, k: {
            "name": a[0], "help": k["help"], "description": k["description"],
            "formatter_class": getattr(k["formatter_class"], "__name__", str(k["formatter_class"]))})
        ns[func](sp, materialize_cmd_info(inp.get("cmd_info") or {}))
        return None
    if func == "_register_plugin_cli_commands":
        sp = types.SimpleNamespace()
        sp.add_parser = _rec("subparsers.add_parser", lambda a, k: {
            "name": a[0], "help": k["help"], "description": k["description"],
            "formatter_class": getattr(k["formatter_class"], "__name__", str(k["formatter_class"]))})
        ns[func](sp)
        return None
    raise KeyError(f"unknown func {func}")


def main():
    n = mism = 0
    shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        holder = {}
        try:
            ns, trace, scripts = build_ns(rec)
            try:
                ret = drive(rec, ns, scripts, holder)
                out = {"ret": norm(ret), "trace": trace, "state": holder.get("state", {})}
            except BaseException as e:  # noqa: BLE001 - fail-closed per case
                out = {"ret": {"__raise__": type(e).__name__}, "trace": trace,
                       "state": holder.get("state", {})}
        except BaseException as e:  # noqa: BLE001 - seeding failure is a mismatch
            out = {"ret": {"__raise__": type(e).__name__}, "trace": [], "state": {}}
        got = rec.get("out")
        if got != out:
            mism += 1
            if shown < 10:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print(f"  C={json.dumps(got, ensure_ascii=False)[:800]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:800]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


# ── self-test: drive every fixture, flag drive failures / suspicious raises ──
def selftest(fixtures):
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    bad = 0
    names = set(PORTED)
    fx_funcs = {r["func"] for r in fixtures}
    if fx_funcs != names:
        print(f"SELFTEST {NAME}: func set drift: missing={sorted(names - fx_funcs)} extra={sorted(fx_funcs - names)}")
        bad += 1
    for i, rec in enumerate(fixtures):
        holder = {}
        try:
            ns, trace, scripts = build_ns(rec)
            try:
                ret = drive(rec, ns, scripts, holder)
                spec = norm(ret)
            except BaseException as e:  # noqa: BLE001
                spec = {"__raise__": type(e).__name__, "msg": str(e)}
            if isinstance(spec, dict) and spec.get("__raise__") == "AttributeError":
                msg = spec.get("msg", "")
                if any(f"'{nm}'" in msg for nm in names):
                    bad += 1
                    print(f"SELFTEST case {i} {rec['func']}: AttributeError naming ported fn: {msg}")
        except BaseException as e:  # noqa: BLE001
            bad += 1
            print(f"SELFTEST case {i} {rec['func']}: DRIVE FAILURE {type(e).__name__}: {e}")
    faulthandler.cancel_dump_traceback_later()
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = Path(__file__).resolve().parent / "fixtures" / "main_cli_flow2.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()