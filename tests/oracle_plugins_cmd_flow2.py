#!/usr/bin/env python3
"""Oracle for plugins_cmd_flow2.c — the UNCLAIMED remainder of hermes_cli/plugins_cmd.py
(wave 2, LIVE authority): the admission/selection transaction, the name resolvers, the
enable-consent gate, the module accessor, the argparse action dispatchers and four nested
closures (cmd_enable._refuse_legacy_relay, _get_plugin_toolset_key._first_toolset /
_from_loaded_plugin / _from_manifest_on_disk).

Drives the LIVE defs with SCRIPTED I/O: defs (and the PluginOperationError /
PluginSourceMoved / PluginScanBlocked + AdmissionRefused / DependencyConflict classes)
are AST-extracted from the quarry (no heavy hermes_cli.plugins_cmd import), module
constants seeded (Assign AND AnnAssign), and every claimed sibling (_console, _fail,
_plugins_dir, _plugin_selection_version, _refuse_recalled_entry, _discover_all_plugins,
_apply_activation, _read_manifest) plus every foreign collaborator replaced by recording
stubs returning the SAME scripted results the C harness feeds its universal seam.
Ported-to-ported calls compose REAL on both sides, so their internal seam events appear
in both traces. Nested closures exec standalone with their free vars bound as per-case
globals (console / LEGACY_RELAY_PLUGIN_KEYS / RELAY_PLUGINS_CONFIG_ENV / name / registry /
_first_toolset).

Path-valued seam results materialize as PPath doubles (str keeps POSIX separators on this
Windows host) whose is_dir fires the is_dir seam. Console markers wrap their scripted
marker; console.print fires console_print. Lock markers wrap in a silent CM LockDouble
(`with lock:` is a no-op); threading.Lock() is EAGER (setdefault factory). The
_apply_activation seam's scripted ret may carry a "state" effect applied to the live
working sets (mirrored by the C port). The manager / loaded / registry doubles materialize
scripted dicts into SimpleNamespace so attribute reads raise the live py3.14 messages.

Fail-closed: a missing/empty seam script RETURNS a RuntimeError raise spec (recorded as
the event ret, then raised), matching the C harness. Uncaught raises (incl. the
BaseException trio) are caught into a ret spec; `except PluginOperationError` /
`except DependencyConflict` / `except AdmissionRefused` sites swallow/print/re-raise
exactly their live kinds. Integral floats normalize to ints on both sides.

State windows (selection_lock {locks}; set_plugin_enabled {locks, enabled, disabled}) are
echoed EVEN ON A RAISE via CUR["state"] (partial mutation history).

Input: stdin JSON lines from t_port_plugins_cmd_flow2:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...], "state":{...}?}}
Summary: ORACLE plugins_cmd_flow2: N cases, M mismatches. Fail-closed (exit 2) when
extraction misses defs or zero cases were compared.
"""
import ast
import builtins
import copy
import faulthandler
import functools
import json
import logging
import os
import sys
import threading
import types
import __future__
from pathlib import PurePath, PurePosixPath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
faulthandler.dump_traceback_later(120, exit=True)

_REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
QUARRY = os.path.join(os.path.dirname(_REPO), "hermes-agent")
if not os.path.isdir(QUARRY):
    print("ORACLE plugins_cmd_flow2: quarry not found at %s" % QUARRY, file=sys.stderr)
    sys.exit(2)
os.environ.setdefault("HOME", os.path.dirname(_REPO))
os.environ.setdefault("USERPROFILE", os.path.dirname(_REPO))
os.environ.setdefault("LOCALAPPDATA", os.path.join(os.path.dirname(_REPO), "AppData", "Local"))
os.environ.setdefault("HERMES_HOME", os.path.join(_REPO, ".testbin", "plugins_cmd_flow2_home"))
sys.path.insert(0, QUARRY)

SRC = os.path.join(QUARRY, "hermes_cli", "plugins_cmd.py")
SRC_TEXT = open(SRC, encoding="utf-8").read()
ADM_SRC = os.path.join(QUARRY, "hermes_cli", "plugins_admission.py")
ADM_TEXT = open(ADM_SRC, encoding="utf-8").read()
ANN_FLAG = __future__.annotations.compiler_flag

# harness dispatch name -> live def name
PY = {
    "admit_and_save_plugin_sets": "_admit_and_save_plugin_sets",
    "selection_lock": "_selection_lock",
    "set_plugin_enabled": "_set_plugin_enabled",
    "catalog_installed_dir": "_catalog_installed_dir",
    "find_plugin_entry": "_find_plugin_entry",
    "enable_consent": "_enable_consent",
    "resolved_user_plugin_dir": "_resolved_user_plugin_dir",
    "catalog": "_catalog",
    "action_update": "_action_update",
    "action_auto_update": "_action_auto_update",
    "action_pack": "_action_pack",
    "action_idle_tasks": "_action_idle_tasks",
    "refuse_legacy_relay": "_refuse_legacy_relay",
    "first_toolset": "_first_toolset",
    "from_loaded_plugin": "_from_loaded_plugin",
    "from_manifest_on_disk": "_from_manifest_on_disk",
}
TOP_TARGETS = [
    "_admit_and_save_plugin_sets", "_selection_lock", "_set_plugin_enabled",
    "_catalog_installed_dir", "_find_plugin_entry", "_enable_consent",
    "_resolved_user_plugin_dir", "_catalog", "_action_update", "_action_auto_update",
    "_action_pack", "_action_idle_tasks",
]
# nested closures: parent def -> [closure names]
NESTED = {
    "cmd_enable": ["_refuse_legacy_relay"],
    "_get_plugin_toolset_key": ["_first_toolset", "_from_loaded_plugin", "_from_manifest_on_disk"],
}
NESTED_NAMES = [c for cs in NESTED.values() for c in cs]
EXC_CLASSES = ["PluginOperationError", "PluginSourceMoved", "PluginScanBlocked"]
ADM_CLASSES = ["AdmissionRefused", "DependencyConflict"]

CUR = {"scripts": {}, "idx": {}, "trace": [], "state": None}
NS_CLASSES = {}
REAL = {}

# ── exceptions ──

BASE_EXC = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
            "GeneratorExit": GeneratorExit}
BUILTIN_EXC = {
    "RuntimeError": RuntimeError, "ValueError": ValueError, "TypeError": TypeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "IndexError": IndexError,
    "Exception": Exception, "StopIteration": StopIteration, "StopAsyncIteration": StopAsyncIteration,
    "OSError": OSError, "EOFError": EOFError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "UnicodeDecodeError": UnicodeDecodeError,
    "UnicodeEncodeError": UnicodeEncodeError, "UnicodeTranslateError": UnicodeTranslateError,
    "FileNotFoundError": FileNotFoundError, "FileExistsError": FileExistsError,
    "PermissionError": PermissionError, "ProcessLookupError": ProcessLookupError,
    "TimeoutError": TimeoutError, "NotADirectoryError": NotADirectoryError,
    "IsADirectoryError": IsADirectoryError, "ConnectionError": ConnectionError,
    "BrokenPipeError": BrokenPipeError, "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError, "ConnectionResetError": ConnectionResetError,
    "InterruptedError": InterruptedError, "ChildProcessError": ChildProcessError,
    "BlockingIOError": BlockingIOError, "UnicodeError": UnicodeError,
    "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
    "PythonFinalizationError": getattr(builtins, "PythonFinalizationError", RuntimeError),
    "NameError": NameError, "UnboundLocalError": UnboundLocalError, "LookupError": LookupError,
    "ArithmeticError": ArithmeticError, "FloatingPointError": FloatingPointError,
    "OverflowError": OverflowError, "ZeroDivisionError": ZeroDivisionError,
    "AssertionError": AssertionError, "SyntaxError": SyntaxError, "IndentationError": IndentationError,
    "TabError": TabError, "SystemError": SystemError, "ReferenceError": ReferenceError,
    "BufferError": BufferError, "MemoryError": MemoryError, "Warning": Warning,
    "UserWarning": UserWarning, "RuntimeWarning": RuntimeWarning, "DeprecationWarning": DeprecationWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning, "SyntaxWarning": SyntaxWarning,
    "FutureWarning": FutureWarning, "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning,
    "BytesWarning": BytesWarning, "ResourceWarning": ResourceWarning, "EncodingWarning": EncodingWarning,
    "ExceptionGroup": ExceptionGroup, "BaseExceptionGroup": BaseExceptionGroup,
}
EXC_ALIAS = {"IOError": "OSError", "EnvironmentError": "OSError", "WindowsError": "OSError"}


def _mat_scan_result(sr):
    if sr is None:
        return None
    if isinstance(sr, dict):
        if "findings" in sr:
            return types.SimpleNamespace(
                findings=[types.SimpleNamespace(**f) for f in sr["findings"]])
        return types.SimpleNamespace(**sr)
    return sr


def make_exc(kind, msg, spec=None):
    if kind in NS_CLASSES:
        cls = NS_CLASSES[kind]
        if kind == "DependencyConflict":
            # rewriting __init__: bypass so the scripted message stays verbatim.
            e = cls.__new__(cls)
            BaseException.__init__(e, msg)
            return e
        if kind == "PluginScanBlocked":
            sr = spec.get("scan_result") if isinstance(spec, dict) else None
            return cls(msg, scan_result=_mat_scan_result(sr))
        return cls(msg)
    kind2 = EXC_ALIAS.get(kind, kind)
    cls = BASE_EXC.get(kind2) or BUILTIN_EXC.get(kind2)
    if cls is None:
        cls = type(str(kind), (Exception,), {})
    return cls(msg)


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    scripts = CUR["scripts"]
    # Distinguish a MISSING script (fail-closed) from a scripted null (None).
    if name not in scripts:
        r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
    else:
        sc = scripts[name]
        if isinstance(sc, list):
            if not sc:
                r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
            else:
                i = CUR["idx"].get(name, 0)
                CUR["idx"][name] = i + 1
                r = sc[min(i, len(sc) - 1)]
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        raise make_exc(r["__raise__"], r.get("msg", ""), r)
    return copy.deepcopy(r)


def norm(o):
    if o is None or isinstance(o, (bool, int, str)):
        return o
    if isinstance(o, float):
        return o
    if isinstance(o, PPath):
        return o.s
    if isinstance(o, PurePath):
        return o.as_posix()
    if isinstance(o, (ConsoleDouble, LockDouble, ModuleDouble)):
        return norm(o.marker)
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    if isinstance(o, (set, frozenset)):
        return sorted(norm(v) for v in o)
    return str(o)


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


# ── path / object doubles ──

class PPath:
    """Path double: str keeps POSIX separators; stat methods are seamed."""
    def __init__(self, s):
        self.s = str(s)

    def __str__(self):
        return self.s

    def __fspath__(self):
        return self.s

    def __truediv__(self, o):
        return PPath(str(PurePosixPath(self.s) / str(o)))

    @property
    def parent(self):
        return PPath(str(PurePosixPath(self.s).parent))

    @property
    def name(self):
        return PurePosixPath(self.s).name

    def __eq__(self, o):
        if isinstance(o, PPath):
            return PurePosixPath(self.s) == PurePosixPath(o.s)
        if isinstance(o, PurePath):
            return PurePosixPath(self.s) == o
        return NotImplemented

    def __hash__(self):
        return hash(PurePosixPath(self.s))

    def __lt__(self, o):
        return self.s < (o.s if isinstance(o, PPath) else str(o))

    def is_dir(self):
        return seam("is_dir", {"path": self.s})

    def exists(self):
        return seam("exists", {"path": self.s})


class ConsoleDouble:
    def __init__(self, marker):
        self.marker = marker

    def print(self, *a, **k):
        return seam("console_print", {"args": list(a)})


class LockDouble:
    """Opaque lock marker that is also a silent context manager."""
    def __init__(self, marker):
        self.marker = marker

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class ModuleDouble:
    def __init__(self, marker):
        self.marker = marker


def _cproj(c):
    if isinstance(c, ConsoleDouble):
        return c.marker
    return c


def _pstr(v):
    if isinstance(v, (PPath, PurePath)):
        return str(v)
    return v


def _log_proj(x):
    if isinstance(x, (PPath, PurePath)):
        return str(x)
    if isinstance(x, BaseException):
        return str(x)
    return x


class LoggerDouble:
    def warning(self, fmt, *a, **k):
        return seam("logger_warning", {"fmt": fmt, "args": [_log_proj(x) for x in a]})

    def debug(self, *a, **k):
        pass

    def info(self, *a, **k):
        pass

    def error(self, *a, **k):
        pass

    def exception(self, *a, **k):
        pass


def mk(name, proj):
    def f(*a, **k):
        return seam(name, proj(a, k))
    f.__name__ = name
    return f


def path_rec(name, proj):
    def f(*a, **k):
        r = seam(name, proj(a, k))
        return PPath(r) if isinstance(r, str) else r
    f.__name__ = name
    return f


def Console_rec(*a, **k):
    return ConsoleDouble(seam("Console", {}))


def mat_entry(r):
    return types.SimpleNamespace(**r) if isinstance(r, dict) else r


def registry_get_entry_rec(t):
    return mat_entry(seam("registry_get_entry", {"name": t}))


def mat_loaded(v):
    if not isinstance(v, dict):
        return v
    d = dict(v)
    if "manifest" in d and isinstance(d["manifest"], dict):
        d["manifest"] = types.SimpleNamespace(**d["manifest"])
    return types.SimpleNamespace(**d)


def mat_manager(r):
    if not isinstance(r, dict):
        return r
    d = dict(r)
    if "_plugins" in d and isinstance(d["_plugins"], dict):
        d["_plugins"] = {k: mat_loaded(v) for k, v in d["_plugins"].items()}
    return types.SimpleNamespace(**d)


def manager_rec():
    return mat_manager(seam("get_plugin_manager", {}))


def apply_activation_rec(enabled, disabled, key, aliases, *, enable):
    # Capture the LIVE internal working sets so the state echo reflects them even
    # when the seam fail-closes (the capture precedes the raise).
    if CUR["state"] is not None:
        CUR["state"]["enabled"] = enabled
        CUR["state"]["disabled"] = disabled
    r = seam("apply_activation", {"enabled": sorted(enabled), "disabled": sorted(disabled),
                                  "name": key, "aliases": list(aliases), "enable": enable})
    if isinstance(r, dict) and isinstance(r.get("state"), dict):
        st = r["state"]
        if "enabled" in st:
            enabled.clear()
            enabled.update(st["enabled"])
        if "disabled" in st:
            disabled.clear()
            disabled.update(st["disabled"])
    return r


def admit_proj(a, k):
    return {"enabled": sorted(a[0]), "disabled": sorted(a[1]),
            "active_plugins_dir": _pstr(k.get("active_plugins_dir")),
            "extra_dirs": list(k.get("extra_dirs") or []),
            "expected_config": k.get("expected_config"), "plugin": k.get("plugin"),
            "settled": list(k.get("settled") or [])}


# ── extraction ──

def seam_bindings():
    return {
        "_plugins_dir": path_rec("_plugins_dir", lambda a, k: {}),
        "get_hermes_home": path_rec("get_hermes_home", lambda a, k: {}),
        "hermes_home_key": mk("hermes_home_key", lambda a, k: {}),
        "_plugin_selection_version": mk("plugin_selection_version", lambda a, k: {}),
        "_refuse_recalled_entry": mk("refuse_recalled_entry", lambda a, k: {"entry": a[0]}),
        "_discover_all_plugins": mk("discover_all_plugins", lambda a, k: {}),
        "_apply_activation": apply_activation_rec,
        "_declared_capabilities_for_key": mk("declared_capabilities_for_key", lambda a, k: {"key": a[0]}),
        "_run_capability_consent": mk("run_capability_consent", lambda a, k: {
            "console": _cproj(a[0]), "key": a[1], "declared_caps": a[2], "context": k.get("context")}),
        "_resolve_tool_override_grant": mk("resolve_tool_override_grant", lambda a, k: {
            "console": _cproj(a[0]), "key": a[1], "allow_tool_override": a[2]}),
        "_console": Console_rec,
        "_fail": mk("fail", lambda a, k: {"console": _cproj(a[0]), "message": a[1]}),
        "cmd_update": mk("cmd_update", lambda a, k: {"name": a[0], "force": k.get("force")}),
        "_read_install_metadata": mk("read_install_metadata", lambda a, k: {}),
        "_read_manifest": mk("read_manifest", lambda a, k: {"plugin_dir": _pstr(a[0])}),
        "logger": LoggerDouble(),
        "threading": threading,
        "functools": functools,
        "Path": PPath,
    }


def install_module_stubs():
    """sys.modules stubs for every in-function import the targets execute."""
    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    hcli = mod("hermes_cli")
    hcli.__path__ = []
    pa = mod("hermes_cli.plugins_admission",
             AdmissionRefused=NS_CLASSES["AdmissionRefused"],
             DependencyConflict=NS_CLASSES["DependencyConflict"],
             admit_plugin_set_change=mk("admit_plugin_set_change", admit_proj))
    hcli.plugins_admission = pa
    pcc = mod("hermes_cli.plugins_cmd_catalog",
              catalog_install_record=mk("catalog_install_record", lambda a, k: {"plugin_dir": _pstr(a[0])}))
    hcli.plugins_cmd_catalog = pcc
    ua = mod("hermes_cli.plugins_cmd_update_all",
             cmd_update_all=mk("cmd_update_all", lambda a, k: {"yes": k.get("yes")}))
    hcli.plugins_cmd_update_all = ua
    au = mod("hermes_cli.plugins_auto_update",
             auto_update_state=mk("auto_update_state", lambda a, k: {}),
             set_auto_update=mk("set_auto_update", lambda a, k: {"on": a[0]}))
    hcli.plugins_auto_update = au
    pp = mod("hermes_cli.plugin_packs",
             pack_command=mk("pack_command", lambda a, k: {
                 "args": vars(a[0]) if isinstance(a[0], types.SimpleNamespace) else a[0]}))
    hcli.plugin_packs = pp
    it = mod("hermes_cli.plugins_idle_tasks",
             cmd_idle_tasks=mk("cmd_idle_tasks", lambda a, k: {
                 "name": a[0], "pause": k.get("pause"), "resume": k.get("resume")}))
    hcli.plugins_idle_tasks = it
    lcm = mod("hermes_cli.left_core_migration",
              legacy_plugin_alias=mk("legacy_plugin_alias", lambda a, k: {"name": a[0]}))
    hcli.left_core_migration = lcm
    pl = mod("hermes_cli.plugins",
             discover_plugins=mk("discover_plugins", lambda a, k: {}),
             get_plugin_manager=manager_rec,
             get_bundled_plugins_dir=path_rec("get_bundled_plugins_dir", lambda a, k: {}))
    hcli.plugins = pl

    pm = mod("pm")
    pm.__path__ = []
    ps = mod("pm.plugins_state", RECORD="disabled_by_update",
             read_home_selection=mk("read_home_selection", lambda a, k: {"home": _pstr(a[0])}))
    pm.plugins_state = ps

    rich = mod("rich")
    rich.__path__ = []
    rm = mod("rich.markup", escape=mk("markup_escape", lambda a, k: {"text": a[0]}))
    rich.markup = rm


def extract():
    ns = {
        "__name__": "hermes_cli.plugins_cmd_flow2_extracted",
        "__builtins__": builtins.__dict__,
        "sys": sys, "json": json, "logging": logging, "os": os, "threading": threading,
        "functools": functools, "Path": PPath, "PurePosixPath": PurePosixPath,
        "Optional": object, "Any": object, "NoReturn": object,
        "logger": LoggerDouble(),
    }
    ns.update(seam_bindings())
    tree = ast.parse(SRC_TEXT)

    def exec_node(node):
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                     flags=ANN_FLAG), ns)

    # seed module constants (Assign + AnnAssign, plain-Name targets, source order)
    for node in tree.body:
        if isinstance(node, ast.Assign):
            if not all(isinstance(t, ast.Name) for t in node.targets):
                continue
        elif isinstance(node, ast.AnnAssign):
            if not isinstance(node.target, ast.Name):
                continue
        else:
            continue
        try:
            exec_node(node)
        except Exception:
            pass
    # exception classes first (the defs reference them)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in EXC_CLASSES:
            exec_node(node)
            NS_CLASSES[node.name] = ns[node.name]
    # admission classes (DependencyConflict subclasses AdmissionRefused)
    adm_tree = ast.parse(ADM_TEXT)
    for node in adm_tree.body:
        if isinstance(node, ast.ClassDef) and node.name in ADM_CLASSES:
            exec(compile(ast.Module(body=[node], type_ignores=[]), ADM_SRC, "exec",
                         flags=ANN_FLAG), ns)
            NS_CLASSES[node.name] = ns[node.name]
    # top-level targets
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in TOP_TARGETS:
            exec_node(node)
    # nested closures (exec standalone; free vars become per-case globals)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in NESTED:
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name in NESTED[node.name]:
                    exec_node(sub)
    # re-bind the seam overrides the Assign seeding may have clobbered
    ns.update(seam_bindings())

    missing = [n for n in TOP_TARGETS + NESTED_NAMES if n not in ns]
    missing += [c for c in EXC_CLASSES + ADM_CLASSES if c not in NS_CLASSES]
    if missing:
        print("ORACLE plugins_cmd_flow2: FATAL missing defs: %s" % sorted(missing), file=sys.stderr)
        sys.exit(2)
    return ns


# ── drive ──

def drive(ns, func, inp):
    # restore every target def to real (composition uses real ported siblings)
    for nm in REAL:
        ns[nm] = REAL[nm]
    g = inp.get
    if func == "admit_and_save_plugin_sets":
        enabled = set(g("enabled") or [])
        disabled = set(g("disabled") or [])
        console = ConsoleDouble(inp["console"]) if g("console") is not None else None
        return ns["_admit_and_save_plugin_sets"](
            enabled, disabled, extra_dirs=g("extra_dirs") or [], console=console,
            action=g("action", "enable"), expected_config=g("expected_config"),
            plugin=g("plugin"), settled=g("settled") or []), None
    if func == "selection_lock":
        locks = {k: LockDouble(v) for k, v in (g("locks") or {}).items()}
        ns["_SELECTION_LOCKS"] = locks
        ns["threading"] = types.SimpleNamespace(Lock=lambda: LockDouble(seam("threading_lock", {})))
        CUR["state"] = {"locks": locks}
        return ns["_selection_lock"](), None
    if func == "set_plugin_enabled":
        locks = {k: LockDouble(v) for k, v in (g("locks") or {}).items()}
        ns["_SELECTION_LOCKS"] = locks
        ns["threading"] = types.SimpleNamespace(Lock=lambda: LockDouble(seam("threading_lock", {})))
        CUR["state"] = {"locks": locks, "enabled": set(), "disabled": set()}
        console = ConsoleDouble(inp["console"]) if g("console") is not None else None
        return ns["_set_plugin_enabled"](g("name", ""), enable=g("enable", False),
                                         aliases=g("aliases") or [], console=console), None
    if func == "catalog_installed_dir":
        return ns["_catalog_installed_dir"](g("name", "")), None
    if func == "find_plugin_entry":
        return ns["_find_plugin_entry"](g("name", "")), None
    if func == "enable_consent":
        console = ConsoleDouble(inp["console"]) if g("console") is not None else None
        return ns["_enable_consent"](console, g("key", ""), g("allow_tool_override")), None
    if func == "resolved_user_plugin_dir":
        return ns["_resolved_user_plugin_dir"](g("name", "")), None
    if func == "catalog":
        marker = seam("catalog_module", {})
        sys.modules["hermes_cli"].plugins_cmd_catalog = ModuleDouble(marker)
        return ns["_catalog"](), None
    if func == "action_update":
        args = types.SimpleNamespace(**(g("args") or {}))
        return ns["_action_update"](args), None
    if func == "action_auto_update":
        args = types.SimpleNamespace(**(g("args") or {}))
        return ns["_action_auto_update"](args), None
    if func == "action_pack":
        args = types.SimpleNamespace(**(g("args") or {}))
        return ns["_action_pack"](args), None
    if func == "action_idle_tasks":
        args = types.SimpleNamespace(**(g("args") or {}))
        return ns["_action_idle_tasks"](args), None
    if func == "refuse_legacy_relay":
        console = ConsoleDouble(inp["console"]) if g("console") is not None else None
        ns["console"] = console
        ns["LEGACY_RELAY_PLUGIN_KEYS"] = set(g("legacy_keys") or [])
        ns["RELAY_PLUGINS_CONFIG_ENV"] = g("env", "")
        return ns["_refuse_legacy_relay"](g("plugin", "")), None
    if func == "first_toolset":
        ns["registry"] = types.SimpleNamespace(get_entry=registry_get_entry_rec)
        return ns["_first_toolset"](g("tool_names") or []), None
    if func == "from_loaded_plugin":
        ns["name"] = g("name", "")
        ns["registry"] = types.SimpleNamespace(get_entry=registry_get_entry_rec)
        return ns["_from_loaded_plugin"](), None
    if func == "from_manifest_on_disk":
        ns["name"] = g("name", "")
        ns["registry"] = types.SimpleNamespace(get_entry=registry_get_entry_rec)
        return ns["_from_manifest_on_disk"](), None
    raise ValueError("unknown func: %s" % func)


def main():
    ns = extract()
    install_module_stubs()
    REAL.update({nm: ns[nm] for nm in TOP_TARGETS + NESTED_NAMES})
    cases = 0
    mism = 0
    per_func = {}
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = _numnorm(record.get("in") or {})
        got = record.get("out") or {}
        # Project the C ret raise spec to the same {__raise__, msg} shape the live
        # drive's BaseException catch builds (drop materialization-only extras).
        gr = got.get("ret")
        if isinstance(gr, dict) and "__raise__" in gr:
            got["ret"] = {"__raise__": gr["__raise__"], "msg": gr.get("msg", "")}
        cases += 1
        per_func[func] = per_func.get(func, 0) + 1
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        CUR["state"] = None
        if func not in PY:
            mism += 1
            print("MISMATCH %s: unknown func (not in PY map)" % func)
            continue
        try:
            ret, _ = drive(ns, func, inp)
        except BaseException as ex:
            ret = {"__raise__": type(ex).__name__, "msg": str(ex)}
        state = CUR["state"]
        want = {"ret": norm(ret), "trace": CUR["trace"]}
        if state is not None:
            want["state"] = norm(state)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE plugins_cmd_flow2: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE plugins_cmd_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()