#!/usr/bin/env python3
"""Oracle for the config migration / backend FLOW port
(src/hermes_cli/config_migration_flow.c).

Drives the LIVE defs of hermes_cli/left_core_migration.py and
hermes_cli/config_backend.py with SCRIPTED I/O: every file/env/backend/
catalog dependency is a recording stub fed the same scripts the C harness
feeds its seams. Compares the DECISION SEQUENCE (ordered {"name","args"}
seam events in "trace"), the return ("ret"), the mutated module-global
mirror ("state", restricted to the keys the case seeded) and the mutated
input config ("config", when the case seeded one).

Live defs are AST-extracted (module Assign/AnnAssign + ClassDef +
FunctionDef exec, multi-pass so LEFT_CORE / _YUANBAO_OFF_PLATFORMS resolve).
The heavy quarry import chain is never imported: in-function imports resolve
against sys.modules stubs whose attributes are the recording seams. Pure
siblings (_marked, _mark, _scope_kept, _core_carried, _read_config,
_noticed, _stamp_created, _first_cause, the FileBackend.write_path ordering,
the _route pathlib branch, the version_token repr/crc32 branch) stay REAL so
nested decision logic is exercised, matching the C port's live transcriptions.

Fail-closed: 0 cases => exit 1; a missing extracted name => exit 2.
Summary: 'ORACLE config_migration_flow: N cases, M mismatches'.

Representation contract (both sides):
- paths reach traces/results as posix strings (Path.as_posix).
- version_token elements carry explicit type tags ({"int":n}, {"float":x},
  {"str":s}, {"bool":b}, {"null":true}, {"tuple":[...]}) — a parsed JSON
  number has no int/float identity and isinstance(version[0], int) needs it.
- backends ride as descriptors: {"backend": {"name":"file"}} for the file
  backend, {"backend": {"explicit": <posix>}} for _ExplicitFile, or the
  scripted remote descriptor.
- an uncaught exception rides as {"__raise__": {type,message,code?,name?}}.
  ConfigBackendUnavailable IS a SystemExit (code == message); the per-case
  wrapper catches BaseException so it cannot kill the oracle.
- str(Path) in logger formatting is normalized to as_posix on both sides.
"""
import ast
import copy
import dataclasses
import importlib
import json
import logging
import os
import re
import sys
import threading
import types
import contextlib
import contextvars
from pathlib import Path
from typing import Any, Callable, Mapping, Optional, Protocol, Union, runtime_checkable
from dataclasses import dataclass, field

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "hermes_cli" / "left_core_migration.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
LCM_SRC = str(QUARRY / "hermes_cli" / "left_core_migration.py")
CB_SRC = str(QUARRY / "hermes_cli" / "config_backend.py")

os.environ.setdefault("HOME", str(QUARRY.parent))
os.environ.setdefault("USERPROFILE", str(QUARRY.parent))
os.environ.setdefault("LOCALAPPDATA", str(QUARRY.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_HERE.parent / ".testbin" / "cmf_home"))

OSERROR_FAMILY = {
    "OSError", "PermissionError", "FileNotFoundError", "FileExistsError",
    "BlockingIOError", "ChildProcessError", "ConnectionError", "BrokenPipeError",
    "ConnectionAbortedError", "ConnectionRefusedError", "ConnectionResetError",
    "InterruptedError", "IsADirectoryError", "NotADirectoryError",
    "ProcessLookupError", "TimeoutError",
}

# Byte-pin check: _YUANBAO_OFF_PLATFORMS as computed from the quarry's
# PLATFORMS table + bundled plugins/platforms dirs (must equal the C table).
YUANBAO_OFF_EXPECTED = (
    "a2a", "acp", "api_server", "bluebubbles", "buzz", "cli", "cron",
    "dingtalk", "discord", "email", "feishu", "google_chat", "homeassistant",
    "irc", "line", "matrix", "mattermost", "msgraph_webhook", "ntfy", "photon",
    "qqbot", "raft", "relay", "signal", "simplex", "slack", "sms", "teams",
    "telegram", "webhook", "wecom", "wecom_callback", "weixin", "whatsapp",
    "whatsapp_cloud",
)


def make_exc(spec):
    spec = spec or {}
    t = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    if t == "ConfigBackendUnavailable":
        return CB_NS["ConfigBackendUnavailable"](msg)
    if t == "ConfigWriteError":
        return CB_NS["ConfigWriteError"](msg, code=spec.get("code", "config_write_failed"))
    if t == "ConfigLockedError":
        return CB_NS["ConfigLockedError"](spec.get("path", ""), spec.get("locked_by", ""), msg or None)
    if t == "ConfigValueError":
        return CB_NS["ConfigValueError"](msg, code=spec.get("code", "config_value_invalid"))
    if t == "SystemExit":
        return SystemExit(spec.get("code", 1))
    if t == "ImportError":
        e = ImportError(msg)
        e.name = spec.get("name")
        return e
    if t in OSERROR_FAMILY:
        cls = {"OSError": OSError, "PermissionError": PermissionError,
               "FileNotFoundError": FileNotFoundError, "FileExistsError": FileExistsError,
               "TimeoutError": TimeoutError}.get(t, OSError)
        return cls(msg)
    import builtins as _b
    cls = getattr(_b, t, None)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        return cls(msg)
    return Exception(msg)


def exc_spec(e):
    spec = {"type": type(e).__name__, "message": str(e)}
    if isinstance(e, SystemExit):
        spec["code"] = e.code
    if isinstance(e, ImportError):
        spec["name"] = getattr(e, "name", None)
    return spec


# ── AST extraction ─────────────────────────────────────────────────────────
def _extract_into(ns, src_path):
    tree = ast.parse(open(src_path, encoding="utf-8").read())
    # Multi-pass: defs first so module Assigns that CALL siblings
    # (_YUANBAO_OFF_PLATFORMS, LEFT_CORE) resolve; re-seed after.
    for _pass in range(2):
        for node in tree.body:
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                try:
                    exec(compile(ast.Module(body=[node], type_ignores=[]), src_path, "exec"), ns)
                except Exception:
                    pass
        for node in tree.body:
            if isinstance(node, ast.ClassDef):
                try:
                    exec(compile(ast.Module(body=[node], type_ignores=[]), src_path, "exec"), ns)
                except Exception:
                    pass
        for node in tree.body:
            if isinstance(node, (ast.Assign, ast.AnnAssign)):
                tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
                if not isinstance(tgt, ast.Name):
                    continue
                try:
                    exec(compile(ast.Module(body=[node], type_ignores=[]), src_path, "exec"), ns)
                except Exception:
                    pass


def _register(name):
    m = types.ModuleType(name)
    sys.modules[name] = m
    return m


def extract_platforms():
    from collections import OrderedDict
    from typing import NamedTuple
    m = _register("hermes_cli.platforms")
    ns = m.__dict__
    ns.update({"OrderedDict": OrderedDict, "NamedTuple": NamedTuple})
    _extract_into(ns, str(QUARRY / "hermes_cli" / "platforms.py"))
    return ns["PLATFORMS"]


def extract():
    hc = sys.modules.get("hermes_cli")
    if hc is None:
        hc = _register("hermes_cli")
        hc.__path__ = []
    agent = sys.modules.get("agent")
    if agent is None:
        agent = _register("agent")
        agent.__path__ = []

    platforms = extract_platforms()
    sf = _register("hermes_cli._startup_fast")
    sf.project_root_str = lambda: str(QUARRY)

    lcm = _register("hermes_cli.left_core_migration")
    lcm_ns = lcm.__dict__
    lcm_ns.update({
        "__name__": "hermes_cli.left_core_migration",
        "logging": logging, "logger": logging.getLogger("hermes_cli.left_core_migration"),
        "os": os, "sys": sys, "contextlib": contextlib,
        "ContextVar": contextvars.ContextVar,
        "dataclass": dataclass, "field": field, "Path": Path,
        "Callable": Callable, "Optional": Optional,
        "project_root_str": lambda: str(QUARRY),
    })
    _extract_into(lcm_ns, LCM_SRC)

    cb = _register("hermes_cli.config_backend")
    cb_ns = cb.__dict__
    cb_ns.update({
        "__name__": "hermes_cli.config_backend",
        "contextlib": contextlib, "contextvars": contextvars, "importlib": importlib,
        "os": os, "re": re, "sys": sys, "threading": threading,
        "dataclass": dataclass, "field": field, "Path": Path,
        "Any": Any, "Mapping": Mapping, "Optional": Optional,
        "Protocol": Protocol, "Union": Union, "runtime_checkable": runtime_checkable,
    })
    _extract_into(cb_ns, CB_SRC)

    lcm_need = ["_marked", "_mark", "_scope_kept", "_core_carried", "_predates_split",
                "record_core_use", "_turn_off_replaced", "_keep_legacy_disable",
                "_record_migration", "_first_cause", "_install_one", "migrate_home",
                "_read_config", "_noticed", "_stamp_created", "LEFT_CORE",
                "_SCOPED_KEY", "_INSTALLED_KEY", "_USED_KEY", "_YUANBAO_OFF_PLATFORMS"]
    cb_need = ["version_token", "_backend_for", "get_config_backend", "_bootstrap",
               "_ensure_bootstrap_env", "_route", "read_config_doc", "write_config_keys",
               "config_cache_version_or_none", "FileBackend", "_ExplicitFile", "Changes",
               "ConfigBackendUnavailable", "ConfigWriteError", "ConfigLockedError",
               "ConfigValueError", "CONFIG_FILENAME", "BACKEND_ENV", "_KIND_RE",
               "_BACKENDS_PACKAGE", "_FILE_BACKEND"]
    missing = [n for n in lcm_need if n not in lcm_ns] + [n for n in cb_need if n not in cb_ns]
    if missing:
        print("FATAL config_migration_flow: extraction missing names:", sorted(set(missing)))
        sys.exit(2)
    if tuple(lcm_ns["_YUANBAO_OFF_PLATFORMS"]) != YUANBAO_OFF_EXPECTED:
        print("FATAL config_migration_flow: _YUANBAO_OFF_PLATFORMS drifted from the C table:")
        print("  live:", tuple(lcm_ns["_YUANBAO_OFF_PLATFORMS"]))
        sys.exit(2)
    return lcm_ns, cb_ns


LCM_NS, CB_NS = extract()
LCM_BASE_LEFT_CORE = LCM_NS["LEFT_CORE"]
CB_REAL = {name: CB_NS[name] for name in
           ("read_config_doc", "config_exists", "write_config_keys",
            "config_cache_version_or_none", "_route", "get_config_backend",
            "_backend_for", "_bootstrap", "_ensure_bootstrap_env", "version_token")}
CB_MOD = sys.modules["hermes_cli.config_backend"]


# ── per-case scripted seam machinery ───────────────────────────────────────
CURRENT = {"ctx": None}


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.io = inp.get("io", {}) or {}
        self.cursors = {}
        self.state = copy.deepcopy(inp.get("state", {}) or {})

    def peek(self, name):
        entry = self.io.get(name)
        if entry is None:
            return None
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            return entry[idx] if idx < len(entry) else None
        return entry

    def consume(self, name):
        entry = self.io.get(name)
        if isinstance(entry, list):
            self.cursors[name] = self.cursors.get(name, 0) + 1

    def script_next(self, name):
        entry = self.peek(name)
        self.consume(name)
        return entry


def proj(v):
    if isinstance(v, BackendDouble):
        return {"backend": v.descriptor}
    if isinstance(v, CB_NS["_ExplicitFile"]):
        return {"backend": {"explicit": v._path.as_posix()}}
    if isinstance(v, Path):
        return v.as_posix()
    if isinstance(v, CB_NS["Changes"]):
        return {"document": proj(v.document),
                "set": {k: proj(x) for k, x in v.set.items()},
                "unset": [proj(x) for x in v.unset],
                "extra_content_on_create": proj(v.extra_content_on_create)}
    if isinstance(v, dict):
        return {k: proj(x) for k, x in v.items()}
    if isinstance(v, (tuple, list)):
        return [proj(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted(v)
    if isinstance(v, LCM_NS["LeftCoreFeature"]):
        return v.plugin
    return v


def record_call(name, args, ret_fn=None):
    ctx = CURRENT["ctx"]
    ctx.trace.append({"name": name, "args": proj(args)})
    result = ctx.script_next(name)
    if isinstance(result, dict):
        eff = result.get("state")
        if isinstance(eff, dict):
            ctx.state.update(eff)
        rs = result.get("raise")
        if isinstance(rs, dict):
            raise make_exc(rs)
        ret = result.get("ret")
    else:
        ret = result
    # Scripted objects are reused across calls; LIVE mutates config in place.
    # Return a fresh copy so earlier traced writes keep their own state,
    # matching the C harness (which copies each scripted ret).
    if isinstance(ret, (dict, list)):
        ret = copy.deepcopy(ret)
    return ret_fn(ret) if ret_fn else ret


class BackendDouble:
    """The selected backend: every surface method is a recording seam."""
    def __init__(self, descriptor):
        self.descriptor = descriptor if isinstance(descriptor, dict) else {}
        self.name = self.descriptor.get("name", "")

    def read_user_doc(self, home):
        return record_call("backend_read_user_doc", {"home": home})

    def read_user_doc_readonly(self, home):
        return record_call("backend_read_user_doc_readonly", {"home": home})

    def version(self, home):
        return record_call("backend_version", {"home": home},
                           ret_fn=lambda r: tuple(r) if isinstance(r, list) else r)

    def cache_version(self, home, version):
        return record_call("backend_cache_version", {"home": home, "version": version},
                           ret_fn=lambda r: tuple(r) if isinstance(r, list) else r)

    def exists(self, home):
        return record_call("backend_exists", {"home": home})

    def probe_readable(self, home):
        return record_call("backend_probe_readable", {"home": home})

    def write_changes(self, home, changes):
        return record_call("backend_write_changes", {"home": home, "changes": changes})

    def destination(self, home):
        return record_call("backend_destination", {"home": home})

    def supports_file_tooling(self):
        return record_call("backend_supports_file_tooling", {})

    def deployment_env_names(self):
        return record_call("backend_deployment_env_names", {},
                           ret_fn=lambda r: frozenset(r) if isinstance(r, list) else r)

    def bootstrap_env_names(self):
        return record_call("backend_bootstrap_env_names", {},
                           ret_fn=lambda r: frozenset(r) if isinstance(r, list) else r)


def _module_double(desc, name):
    if not isinstance(desc, dict):
        return None
    m = types.SimpleNamespace()
    if "get_backend" in desc:
        v = desc["get_backend"]
        if v == "fn":
            def get_backend():
                return record_call("get_backend_call", {"name": name},
                                   ret_fn=lambda d: BackendDouble(d) if isinstance(d, dict) else d)
            m.get_backend = get_backend
        else:
            m.get_backend = v
    return m


class ModulesDouble:
    def get(self, name):
        desc = record_call("sys_modules_get", {"name": name})
        return _module_double(desc, name)


class DefaultConfigDouble:
    def __getitem__(self, key):
        return record_call("default_config_version", {})


def _names_to_features(ret):
    if not isinstance(ret, list):
        return ret
    return [next(f for f in LCM_NS["LEFT_CORE"] if f.plugin == n) for n in ret]


def _logger_debug(*args):
    def norm(a):
        if isinstance(a, Path):
            return a.as_posix()
        if isinstance(a, BaseException):
            return str(a)
        return str(a)
    message = args[0] % tuple(norm(a) for a in args[1:]) if len(args) > 1 else args[0]
    return record_call("logger_debug", {"message": message})


def _note_first_stamp(home, on_disk, to_write):
    ret = record_call("note_first_stamp", {"home": home, "on_disk": on_disk, "config": to_write})
    if isinstance(ret, dict):
        to_write.update(ret)
    return ret


def install_stubs(ctx, inp):
    def seam(name, args_fn, ret_fn=None):
        def call(*a, **k):
            return record_call(name, args_fn(*a, **k), ret_fn)
        return call

    def stub_mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    # ── LCM namespace overrides (seamed helpers) ──
    LCM_NS["feature_in_use"] = lambda feature, home, *, process_env=False: record_call(
        "feature_in_use", {"plugin": feature.plugin, "home": home, "process_env": process_env})
    LCM_NS["_pending"] = lambda home, *, say, process_env=False, backoff=False: record_call(
        "pending", {"home": home, "process_env": process_env}, ret_fn=_names_to_features)
    LCM_NS["_install_command"] = seam("install_command", lambda plugin, home: {"plugin": plugin, "home": home})
    LCM_NS["_note_failure"] = seam("note_failure", lambda home, plugin: {"home": home, "plugin": plugin})
    LCM_NS["plugin_present"] = seam("plugin_present", lambda plugin, home: {"plugin": plugin, "home": home})
    LCM_NS["logger"] = types.SimpleNamespace(debug=_logger_debug,
                                             warning=lambda *a, **k: None,
                                             info=lambda *a, **k: None)

    def notice_rec(plugin):
        def call(home, *, process_env=False):
            return record_call("feature_notice",
                               {"plugin": plugin, "home": home, "process_env": process_env})
        return call
    LCM_NS["LEFT_CORE"] = tuple(dataclasses.replace(f, notice=notice_rec(f.plugin))
                                for f in LCM_BASE_LEFT_CORE)

    # ── LCM in-function import stubs ──
    stub_mod("hermes_cli.config",
             atomic_config_write=seam("atomic_config_write", lambda path, config: {"path": path, "config": config}),
             read_user_config_raw=seam("read_user_config_raw", lambda path: {"path": path}))
    CB_MOD.config_exists = seam("config_exists", lambda path: {"path": path})
    CB_MOD.read_config_doc = seam("read_config_doc", lambda path: {"path": path})
    stub_mod("hermes_cli.toolset_validation",
             parse_platform_toolsets_value=seam("parse_platform_toolsets_value", lambda value: {"value": value}))
    stub_mod("agent.skill_utils",
             parse_config_string_list=seam("parse_config_string_list", lambda value: {"value": value}))
    stub_mod("hermes_cli.config_defaults", DEFAULT_CONFIG=DefaultConfigDouble())
    stub_mod("hermes_cli.config_version_stamp", note_first_stamp=_note_first_stamp)
    stub_mod("providers", refresh_bound_home=seam("refresh_bound_home", lambda: {}))
    stub_mod("utils",
             atomic_roundtrip_yaml_save=seam("atomic_roundtrip_yaml_save",
                 lambda path, document, extra_content_on_create=None:
                     {"path": path, "document": document, "extra_content_on_create": extra_content_on_create}),
             atomic_roundtrip_yaml_update=seam("atomic_roundtrip_yaml_update",
                 lambda path, key, value: {"path": path, "key": key, "value": value}))

    env_loader = types.ModuleType("hermes_cli.env_loader")

    def env_loader_getattr(name):
        entry = ctx.peek("apply_config_bootstrap_env")
        if isinstance(entry, dict) and isinstance(entry.get("import_raise"), dict):
            ctx.consume("apply_config_bootstrap_env")
            raise make_exc(entry["import_raise"])
        if name == "apply_config_bootstrap_env":
            return lambda names_fn, home, project_env: record_call(
                "apply_config_bootstrap_env", {"home": home, "project_env": project_env})
        raise AttributeError(name)
    env_loader.__getattr__ = env_loader_getattr
    sys.modules["hermes_cli.env_loader"] = env_loader

    # ── CB namespace overrides ──
    CB_NS["_FILE_BACKEND"] = BackendDouble({"name": "file"})
    CB_NS["sys"] = types.SimpleNamespace(modules=ModulesDouble())

    def import_module_rec(name):
        return record_call("import_module", {"name": name},
                           ret_fn=lambda d: _module_double(d, name))
    CB_NS["importlib"] = types.SimpleNamespace(import_module=import_module_rec)
    CB_NS["os"] = types.SimpleNamespace(environ=types.SimpleNamespace(
        get=lambda key, default=None: record_call("os_environ_get", {"key": key, "default": default})))
    CB_NS["threading"] = types.SimpleNamespace(
        get_ident=lambda: record_call("threading_get_ident", {}))
    CB_NS["drop_other_access"] = seam("drop_other_access", lambda path: {"path": path})
    CB_NS["config_cache_version"] = seam("config_cache_version",
        lambda config_path: {"config_path": config_path},
        ret_fn=lambda r: tuple(r) if isinstance(r, list) else r)
    CB_NS["_refuse_redacted_sets"] = seam("refuse_redacted_sets",
        lambda config_path, sets: {"config_path": config_path, "sets": sets})
    FB = CB_NS["FileBackend"]
    FB.read_user_doc = staticmethod(lambda home: record_call("backend_read_user_doc", {"home": home}))
    FB.write_changes = lambda self, home, changes: record_call(
        "backend_write_changes", {"home": home, "changes": changes})


def seed_state(ctx):
    st = ctx.state
    if "_BOOTSTRAPPED" not in st:
        st["_BOOTSTRAPPED"] = False
    if "_BOOTSTRAP_OWNER" not in st:
        st["_BOOTSTRAP_OWNER"] = None
    CB_NS["_BOOTSTRAPPED"] = st["_BOOTSTRAPPED"]
    CB_NS["_BOOTSTRAP_OWNER"] = st["_BOOTSTRAP_OWNER"]


def build_version(tagged):
    if tagged is None:
        return None
    def conv(e):
        if not isinstance(e, dict):
            return e
        if "int" in e:
            return int(e["int"])
        if "float" in e:
            return float(e["float"])
        if "str" in e:
            return e["str"]
        if "bool" in e:
            return bool(e["bool"])
        if "null" in e:
            return None
        if "tuple" in e:
            return tuple(conv(x) for x in e["tuple"])
        return None
    return tuple(conv(e) for e in tagged)


def feature_of(inp):
    return next(f for f in LCM_NS["LEFT_CORE"] if f.plugin == inp["feature"])


def run_case(func, inp):
    ctx = CaseCtx(inp)
    CURRENT["ctx"] = ctx
    install_stubs(ctx, inp)
    seed_state(ctx)
    out = {"ret": None, "trace": ctx.trace}
    config = None
    home = Path(inp.get("home", ""))
    install_rec = lambda name: record_call("install", {"plugin": name})
    say_rec = lambda message: record_call("say", {"message": message})
    try:
        if func == "marked":
            config = copy.deepcopy(inp.get("config") or {})
            out["ret"] = LCM_NS["_marked"](config, inp.get("key", ""), feature_of(inp))
        elif func == "mark":
            config = copy.deepcopy(inp.get("config") or {})
            LCM_NS["_mark"](config, inp.get("key", ""), feature_of(inp))
        elif func == "core_carried":
            out["ret"] = LCM_NS["_core_carried"](feature_of(inp), inp.get("selection"))
        elif func == "predates_split":
            out["ret"] = LCM_NS["_predates_split"](home, feature_of(inp))
        elif func == "record_core_use":
            config = copy.deepcopy(inp.get("config") or {})
            out["ret"] = LCM_NS["record_core_use"](home, config, int(inp.get("version", 0)))
        elif func == "turn_off_replaced":
            config = copy.deepcopy(inp.get("config") or {})
            LCM_NS["_turn_off_replaced"](config, feature_of(inp))
        elif func == "keep_legacy_disable":
            config = copy.deepcopy(inp.get("config") or {})
            LCM_NS["_keep_legacy_disable"](config, feature_of(inp))
        elif func == "record_migration":
            LCM_NS["_record_migration"](home, feature_of(inp))
        elif func == "first_cause":
            out["ret"] = LCM_NS["_first_cause"](inp.get("error", ""))
        elif func == "install_one":
            out["ret"] = LCM_NS["_install_one"](home, feature_of(inp), install=install_rec,
                                                say=say_rec, process_env=inp.get("process_env", False))
        elif func == "migrate_home":
            out["ret"] = LCM_NS["migrate_home"](home, install=install_rec, say=say_rec,
                                                process_env=inp.get("process_env", False))
        elif func == "version_token":
            out["ret"] = CB_REAL["version_token"](build_version(inp.get("version")))
        elif func == "backend_for":
            out["ret"] = CB_REAL["_backend_for"](inp.get("kind", ""))
        elif func == "get_config_backend":
            out["ret"] = CB_REAL["get_config_backend"]()
        elif func == "bootstrap":
            h = Path(inp["home"]) if inp.get("home") is not None else None
            pe = Path(inp["project_env"]) if inp.get("project_env") is not None else None
            CB_REAL["_bootstrap"](h, pe, once=inp.get("once", False))
        elif func == "ensure_bootstrap_env":
            CB_REAL["_ensure_bootstrap_env"]()
        elif func == "route":
            out["ret"] = CB_REAL["_route"](inp.get("config_path", ""))
        elif func == "read_config_doc":
            out["ret"] = CB_REAL["read_config_doc"](inp.get("config_path", ""))
        elif func == "write_path":
            ch = inp.get("changes") or {}
            changes = CB_NS["Changes"](document=ch.get("document"),
                                       set=dict(ch.get("set") or {}),
                                       unset=tuple(ch.get("unset") or ()),
                                       extra_content_on_create=ch.get("extra_content_on_create"))
            CB_NS["FileBackend"].write_path(Path(inp.get("path", "")), changes)
        elif func == "config_cache_version_or_none":
            out["ret"] = CB_REAL["config_cache_version_or_none"](inp.get("config_path", ""))
        elif func == "write_config_keys":
            CB_REAL["write_config_keys"](inp.get("config_path", ""), dict(inp.get("updates") or {}))
        else:
            raise KeyError(func)
    except BaseException as e:   # SystemExit is BaseException, NOT Exception
        out["ret"] = {"__raise__": exc_spec(e)}
    out["ret"] = proj(out["ret"])
    if config is not None:
        out["config"] = proj(config)
    ctx.state["_BOOTSTRAPPED"] = CB_NS["_BOOTSTRAPPED"]
    ctx.state["_BOOTSTRAP_OWNER"] = CB_NS["_BOOTSTRAP_OWNER"]
    out["state"] = {k: proj(ctx.state[k]) for k in (inp.get("state") or {}) if k in ctx.state}
    return out


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
        try:
            want = run_case(func, c.get("in") or {})
        except Exception as e:
            mism += 1
            print(f"PYERR {func}: {type(e).__name__}: {e}")
            continue
        got = c.get("out") or {}
        ok = (got.get("trace") == want.get("trace")
              and got.get("ret") == want.get("ret")
              and got.get("state") == want.get("state")
              and got.get("config") == want.get("config"))
        if not ok:
            mism += 1
            print(f"MISMATCH {func} case={c.get('case', '?')}")
            print(f"  C  ={json.dumps(got, sort_keys=True, default=str)}")
            print(f"  PY ={json.dumps(want, sort_keys=True, default=str)}")
    print(f"ORACLE config_migration_flow: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())