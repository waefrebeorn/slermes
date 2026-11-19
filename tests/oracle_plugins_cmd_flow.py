#!/usr/bin/env python3
"""Oracle for plugins_cmd_flow.c — the UNCLAIMED hermes_cli/plugins_cmd.py FLOW
cores + leaves (LIVE authority): the console/config helpers, the manifest readers,
the message builders, the selection algebra and the discovery scan.

Drives the LIVE defs with SCRIPTED I/O: defs (and the PluginOperationError /
PluginSourceMoved / PluginScanBlocked classes) are AST-extracted from the quarry
(no heavy hermes_cli.plugins_cmd import), module constants seeded (Assign AND
AnnAssign), and every claimed sibling (_scan_plugin_tree, _plugins_dir, the
_get_enabled_set / _get_disabled_set / _get_current_memory_provider partial
accessors, _get_plugin_toolset_key, left_core_migration moved_plugin_hint /
legacy_plugin_keys) plus every foreign collaborator replaced by recording stubs
returning the SAME scripted results the C harness feeds its universal seam. The
unported unclaimed sibling _set_plugin_enabled is seamed like a claimed sibling.
Ported-to-ported calls compose REAL on both sides, so their internal seam events
appear in both traces.

Path-valued seam results materialize as PPath doubles (str keeps POSIX separators
on this Windows host) whose stat/IO methods fire the is_dir / exists / is_symlink /
iterdir seams; pure Path() construction uses the PPath double. Console/Table
markers wrap their scripted marker; console.print fires console_print. logger is a
recorder firing logger_warning. sys.stdin/stdout.isatty are per-case doubles.

Fail-closed: a missing/empty seam script RETURNS a RuntimeError raise spec
(recorded as the event ret, then raised), matching the C harness. Uncaught raises
(incl. the BaseException trio) are caught into a ret spec; `except Exception` /
`except ValueError` / `except (EOFError, KeyboardInterrupt)` / `except (OSError,
PluginOperationError)` / `except PluginScanBlocked` sites swallow/convert exactly
their live kinds. Integral floats normalize to ints on both sides (input AND seam
scripts) so isinstance(int) gates agree with the C number-is-int model.

Input: stdin JSON lines from t_port_plugins_cmd_flow:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...], "state":{...}?}}
Summary: ORACLE plugins_cmd_flow: N cases, M mismatches. Fail-closed (exit 2) when
extraction misses defs or zero cases were compared.
"""
import ast
import builtins
import copy
import faulthandler
import json
import logging
import os
import sys
import types
import __future__
from pathlib import PurePath, PurePosixPath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
faulthandler.dump_traceback_later(120, exit=True)

_REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
QUARRY = os.path.join(os.path.dirname(_REPO), "hermes-agent")
if not os.path.isdir(QUARRY):
    print("ORACLE plugins_cmd_flow: quarry not found at %s" % QUARRY, file=sys.stderr)
    sys.exit(2)
os.environ.setdefault("HOME", os.path.dirname(_REPO))
os.environ.setdefault("USERPROFILE", os.path.dirname(_REPO))
os.environ.setdefault("LOCALAPPDATA", os.path.join(os.path.dirname(_REPO), "AppData", "Local"))
os.environ.setdefault("HERMES_HOME", os.path.join(_REPO, ".testbin", "plugins_cmd_flow_home"))
sys.path.insert(0, QUARRY)

SRC = os.path.join(QUARRY, "hermes_cli", "plugins_cmd.py")
SRC_TEXT = open(SRC, encoding="utf-8").read()
ANN_FLAG = __future__.annotations.compiler_flag

_REAL_STDIN = sys.stdin
_REAL_STDOUT = sys.stdout

# harness dispatch name -> live def name
PY = {
    "console": "_console",
    "table": "_table",
    "is_tty": "_is_tty",
    "fail": "_fail",
    "ask_yes": "_ask_yes",
    "config_value": "_config_value",
    "clone_timeout_seconds": "_clone_timeout_seconds",
    "config_name_set": "_config_name_set",
    "config_str": "_config_str",
    "write_config_value": "_write_config_value",
    "native_manifest_file": "_native_manifest_file",
    "has_portable_manifest": "_has_portable_manifest",
    "load_yaml_manifest": "_load_yaml_manifest",
    "looks_like_plugin_dir": "_looks_like_plugin_dir",
    "read_manifest_info": "_read_manifest_info",
    "missing_env_specs": "_missing_env_specs",
    "clone_failure_message": "_clone_failure_message",
    "unknown_plugin_message": "_unknown_plugin_message",
    "preserved_files_note": "_preserved_files_note",
    "scan_merged_tree": "_scan_merged_tree",
    "discard_key_and_leaf": "_discard_key_and_leaf",
    "plugin_aliases": "_plugin_aliases",
    "disabled_by_update": "_disabled_by_update",
    "activate_key": "_activate_key",
    "apply_activation": "_apply_activation",
    "forget_plugin_config": "_forget_plugin_config",
    "plugin_selection_version": "_plugin_selection_version",
    "refuse_recalled_entry": "_refuse_recalled_entry",
    "tri_state_flag": "_tri_state_flag",
    "category_active_names": "_category_active_names",
    "saved_list_toolset_key": "_saved_list_toolset_key",
    "scan_level": "_scan_level",
    "discover_all_plugins": "_discover_all_plugins",
    "default_on": "_default_on",
}
TOP_TARGETS = list(PY.values())
EXC_CLASSES = ["PluginOperationError", "PluginSourceMoved", "PluginScanBlocked"]

CUR = {"scripts": {}, "idx": {}, "trace": []}


# ── exceptions ──

BASE_EXC = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
            "GeneratorExit": GeneratorExit}
BUILTIN_EXC = {
    "RuntimeError": RuntimeError, "ValueError": ValueError, "TypeError": TypeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "IndexError": IndexError,
    "Exception": Exception, "StopIteration": StopIteration, "OSError": OSError,
    "EOFError": EOFError, "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "UnicodeDecodeError": UnicodeDecodeError, "FileNotFoundError": FileNotFoundError,
    "FileExistsError": FileExistsError, "PermissionError": PermissionError,
    "ProcessLookupError": ProcessLookupError, "TimeoutError": TimeoutError,
    "NotADirectoryError": NotADirectoryError, "IsADirectoryError": IsADirectoryError,
    "ConnectionError": ConnectionError, "BrokenPipeError": BrokenPipeError,
    "InterruptedError": InterruptedError, "ChildProcessError": ChildProcessError,
    "BlockingIOError": BlockingIOError, "UnicodeError": UnicodeError,
    "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
    "NameError": NameError, "LookupError": LookupError, "ArithmeticError": ArithmeticError,
    "AssertionError": AssertionError, "SyntaxError": SyntaxError, "SystemError": SystemError,
    "ReferenceError": ReferenceError, "BufferError": BufferError, "MemoryError": MemoryError,
    "Warning": Warning, "UserWarning": UserWarning, "RuntimeWarning": RuntimeWarning,
}


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
    if kind == "PluginScanBlocked":
        sr = spec.get("scan_result") if isinstance(spec, dict) else None
        return NS_CLASSES["PluginScanBlocked"](msg, scan_result=_mat_scan_result(sr))
    if kind == "PluginOperationError":
        return NS_CLASSES["PluginOperationError"](msg)
    if kind == "PluginSourceMoved":
        return NS_CLASSES["PluginSourceMoved"](msg)
    cls = BASE_EXC.get(kind) or BUILTIN_EXC.get(kind)
    if cls is None:
        cls = type(str(kind), (Exception,), {})
    return cls(msg)


NS_CLASSES = {}


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
    if isinstance(o, (ConsoleDouble, TableDouble)):
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
    """Path double: str keeps POSIX separators; stat/IO methods are seamed."""
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

    def is_symlink(self):
        return seam("is_symlink", {"path": self.s})

    def iterdir(self):
        r = seam("iterdir", {"path": self.s})
        return [PPath(x) for x in r] if isinstance(r, list) else r


class ConsoleDouble:
    def __init__(self, marker):
        self.marker = marker

    def print(self, *a, **k):
        return seam("console_print", {"args": list(a)})


class TableDouble:
    def __init__(self, marker):
        self.marker = marker

    def add_column(self, header, style=None, **k):
        return seam("add_column", {"header": header, "style": style})


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


def set_rec(name, proj):
    """Claimed set-valued accessor: the seam records the raw scripted list, but
    the live code receives a SET (the real functools.partial(_config_name_set)
    returns a set; `aliases & other` needs set operands)."""
    def f(*a, **k):
        r = seam(name, proj(a, k))
        return set(r) if isinstance(r, list) else r
    f.__name__ = name
    return f


def Console_rec(*a, **k):
    return ConsoleDouble(seam("Console", {}))


def Table_rec(*a, **k):
    return TableDouble(seam("Table", dict(k)))


def reader_rec(prompt):
    return seam("reader", {"prompt": prompt})


def stdin_isatty_rec():
    return seam("stdin_isatty", {})


def stdout_isatty_rec():
    return seam("stdout_isatty", {})


def ep_manifests_rec():
    r = seam("discover_entrypoint_manifests", {})
    if isinstance(r, list):
        return [types.SimpleNamespace(**m) for m in r]
    return r


# ── extraction ──

def seam_bindings():
    return {
        # bare globals (module-top imports / claimed siblings / unported seam)
        "cfg_get": mk("cfg_get", lambda a, k: {"config": a[0], "keys": list(a[1:]), "default": k.get("default")}),
        "get_hermes_home": path_rec("get_hermes_home", lambda a, k: {}),
        "_plugins_dir": path_rec("_plugins_dir", lambda a, k: {}),
        "_scan_plugin_tree": mk("_scan_plugin_tree", lambda a, k: dict({"plugin_dir": _pstr(a[0]), "identifier": a[1]}, **k)),
        "_get_enabled_set": set_rec("get_enabled_set", lambda a, k: {}),
        "_get_disabled_set": set_rec("get_disabled_set", lambda a, k: {}),
        "_get_current_memory_provider": mk("get_current_memory_provider", lambda a, k: {}),
        "_get_plugin_toolset_key": mk("get_plugin_toolset_key", lambda a, k: {"name": a[0]}),
        "_set_plugin_enabled": mk("set_plugin_enabled", lambda a, k: {
            "name": a[0], "enable": k.get("enable"),
            "aliases": sorted(k.get("aliases") or []), "console": _cproj(k.get("console"))}),
        "logger": LoggerDouble(),
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
    cfg = mod("hermes_cli.config",
              load_config=mk("load_config", lambda a, k: {}),
              save_config=mk("save_config", lambda a, k: {"config": copy.deepcopy(a[0])}),
              get_env_value=mk("get_env_value", lambda a, k: {"name": a[0]}),
              cfg_get=mk("cfg_get", lambda a, k: {"config": a[0], "keys": list(a[1:]), "default": k.get("default")}))
    hcli.config = cfg
    tv = mod("hermes_cli.toolset_validation",
             parse_platform_toolsets_value=mk("parse_platform_toolsets_value", lambda a, k: {"value": a[0]}))
    hcli.toolset_validation = tv
    ap = mod("hermes_cli.agent_plugins",
             read_agent_plugin_manifest=mk("read_agent_plugin_manifest", lambda a, k: {"plugin_dir": _pstr(a[0])}))
    hcli.agent_plugins = ap
    lcm = mod("hermes_cli.left_core_migration",
              moved_plugin_hint=mk("moved_plugin_hint", lambda a, k: {"name": a[0]}),
              legacy_plugin_keys=mk("legacy_plugin_keys", lambda a, k: {"key": a[0]}))
    hcli.left_core_migration = lcm
    rs = mod("hermes_cli.runtime_state", _digest=mk("_digest", lambda a, k: {"path": _pstr(a[0])}))
    hcli.runtime_state = rs
    pcc = mod("hermes_cli.plugins_cmd_catalog",
              refuse_if_installed_removed=mk("refuse_if_installed_removed",
                                             lambda a, k: {"name": a[0], "plugin_dir": _pstr(a[1])}))
    hcli.plugins_cmd_catalog = pcc
    pl = mod("hermes_cli.plugins",
             get_bundled_plugins_dir=path_rec("get_bundled_plugins_dir", lambda a, k: {}),
             discover_entrypoint_manifests=ep_manifests_rec)
    hcli.plugins = pl

    pm = mod("pm")
    pm.__path__ = []
    pd = mod("pm.plugin_declarations",
             native_manifest_file=path_rec("native_manifest_file", lambda a, k: {"plugin_dir": _pstr(a[0])}),
             read_native_manifest=mk("read_native_manifest", lambda a, k: {"manifest_file": _pstr(a[0])}))
    pm.plugin_declarations = pd
    ps = mod("pm.plugins_state", RECORD="disabled_by_update",
             read_home_selection=mk("read_home_selection", lambda a, k: {"home": _pstr(a[0])}))
    pm.plugins_state = ps

    rich = mod("rich")
    rich.__path__ = []
    rc = mod("rich.console", Console=Console_rec)
    rich.console = rc
    rt = mod("rich.table", Table=Table_rec)
    rich.table = rt
    rm = mod("rich.markup", escape=mk("markup_escape", lambda a, k: {"text": a[0]}))
    rich.markup = rm

    prov = mod("providers", _declares_model_provider_kind=mk("_declares_model_provider_kind",
                                                            lambda a, k: {"path": _pstr(a[0])}))
    prov.__path__ = []


def extract():
    ns = {
        "__name__": "hermes_cli.plugins_cmd_flow_extracted",
        "__builtins__": builtins.__dict__,
        "sys": sys, "json": json, "logging": logging, "os": os,
        "Path": PPath, "PurePosixPath": PurePosixPath,
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
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in TOP_TARGETS:
            exec_node(node)
    # re-bind the seam overrides the Assign seeding may have clobbered
    ns.update(seam_bindings())

    missing = [n for n in TOP_TARGETS if n not in ns]
    missing += [c for c in EXC_CLASSES if c not in NS_CLASSES]
    if missing:
        print("ORACLE plugins_cmd_flow: FATAL missing defs: %s" % sorted(missing), file=sys.stderr)
        sys.exit(2)
    return ns


# ── drive ──

def _pp(inp, key):
    return PPath(inp.get(key, ""))


def drive(ns, func, inp):
    if func == "console":
        return ns["_console"](), None
    if func == "table":
        cols = [tuple(c) for c in (inp.get("columns") or [])]
        return ns["_table"](cols, **(inp.get("kwargs") or {})), None
    if func == "is_tty":
        return ns["_is_tty"](), None
    if func == "fail":
        console = ConsoleDouble(inp["console"]) if inp.get("console") is not None else None
        return ns["_fail"](console, inp.get("message", "")), None
    if func == "ask_yes":
        return ns["_ask_yes"](inp.get("prompt", ""), reader=reader_rec), None
    if func == "config_value":
        return ns["_config_value"](*(inp.get("keys") or []), default=inp.get("default")), None
    if func == "clone_timeout_seconds":
        return ns["_clone_timeout_seconds"](), None
    if func == "config_name_set":
        return ns["_config_name_set"](*(inp.get("keys") or [])), None
    if func == "config_str":
        return ns["_config_str"](*(inp.get("keys") or []), default=inp.get("default")), None
    if func == "write_config_value":
        return ns["_write_config_value"](inp.get("section", ""), inp.get("key", ""), inp.get("value")), None
    if func == "native_manifest_file":
        return ns["_native_manifest_file"](_pp(inp, "plugin_dir")), None
    if func == "has_portable_manifest":
        return ns["_has_portable_manifest"](_pp(inp, "plugin_dir")), None
    if func == "load_yaml_manifest":
        return ns["_load_yaml_manifest"](_pp(inp, "manifest_file")), None
    if func == "looks_like_plugin_dir":
        return ns["_looks_like_plugin_dir"](_pp(inp, "target")), None
    if func == "read_manifest_info":
        return ns["_read_manifest_info"](_pp(inp, "d"), inp.get("prefix", "")), None
    if func == "missing_env_specs":
        return ns["_missing_env_specs"](inp.get("manifest")), None
    if func == "clone_failure_message":
        return ns["_clone_failure_message"](inp.get("git_url", ""), inp.get("git_error", "")), None
    if func == "unknown_plugin_message":
        return ns["_unknown_plugin_message"](inp.get("name", ""),
                                             downloaded_only=inp.get("downloaded_only", False)), None
    if func == "preserved_files_note":
        e = inp.get("exc") or {}
        exc = NS_CLASSES["PluginScanBlocked"](e.get("message", ""),
                                              scan_result=_mat_scan_result(e.get("scan_result")))
        return ns["_preserved_files_note"](exc, inp.get("merged") or []), None
    if func == "scan_merged_tree":
        return ns["_scan_merged_tree"](_pp(inp, "plugin_dir"), inp.get("identifier", ""),
                                       inp.get("merged"), **(inp.get("kwargs") or {})), None
    if func == "discard_key_and_leaf":
        names = set(inp.get("names") or [])
        ns["_discard_key_and_leaf"](names, inp.get("key", ""))
        return None, {"names": sorted(names)}
    if func == "plugin_aliases":
        return ns["_plugin_aliases"](inp.get("key", ""), inp.get("entries")), None
    if func == "disabled_by_update":
        return ns["_disabled_by_update"](inp.get("key", ""), set(inp.get("aliases") or [])), None
    if func == "activate_key":
        console = ConsoleDouble(inp["console"]) if inp.get("console") is not None else None
        return ns["_activate_key"](inp.get("key", ""), enable=inp.get("enable", False), console=console), None
    if func == "apply_activation":
        enabled = set(inp.get("enabled") or [])
        disabled = set(inp.get("disabled") or [])
        ns["_apply_activation"](enabled, disabled, inp.get("key", ""),
                                set(inp.get("aliases") or []), enable=inp.get("enable", False))
        return None, {"enabled": sorted(enabled), "disabled": sorted(disabled)}
    if func == "forget_plugin_config":
        return ns["_forget_plugin_config"](set(inp.get("aliases") or [])), None
    if func == "plugin_selection_version":
        return ns["_plugin_selection_version"](), None
    if func == "refuse_recalled_entry":
        entry = inp.get("entry")
        return ns["_refuse_recalled_entry"](tuple(entry) if entry is not None else None), None
    if func == "tri_state_flag":
        args = types.SimpleNamespace(**(inp.get("args") or {}))
        return ns["_tri_state_flag"](args, inp.get("yes_attr", ""), inp.get("no_attr", "")), None
    if func == "category_active_names":
        return ns["_category_active_names"](), None
    if func == "saved_list_toolset_key":
        return ns["_saved_list_toolset_key"](inp.get("name", "")), None
    if func == "scan_level":
        seen = dict(inp.get("seen") or {})
        ns["_scan_level"](_pp(inp, "base"), inp.get("source", ""), set(inp.get("skip_names") or []),
                          inp.get("prefix", ""), inp.get("depth", 0), seen)
        return None, {"seen": seen}
    if func == "discover_all_plugins":
        return ns["_discover_all_plugins"](), None
    if func == "default_on":
        return ns["_default_on"](inp.get("dir_path", ""), inp.get("source", "")), None
    raise ValueError("unknown func: %s" % func)


def main():
    ns = extract()
    install_module_stubs()
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
        # The live drive catches a raised exception and projects it to
        # {__raise__, msg}; the C ret raise spec may carry materialization fields
        # (e.g. scan_result) that are NOT part of the observable ret. Project both
        # sides to the same {__raise__, msg} shape.
        gr = got.get("ret")
        if isinstance(gr, dict) and "__raise__" in gr:
            got["ret"] = {"__raise__": gr["__raise__"], "msg": gr.get("msg", "")}
        cases += 1
        per_func[func] = per_func.get(func, 0) + 1
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        if func not in PY:
            mism += 1
            print("MISMATCH %s: unknown func (not in PY map)" % func)
            continue
        old_stdin, old_stdout = sys.stdin, sys.stdout
        sys.stdin = types.SimpleNamespace(isatty=stdin_isatty_rec)
        sys.stdout = types.SimpleNamespace(isatty=stdout_isatty_rec)
        try:
            ret, state = drive(ns, func, inp)
        except BaseException as ex:
            ret = {"__raise__": type(ex).__name__, "msg": str(ex)}
            state = None
        finally:
            sys.stdin, sys.stdout = old_stdin, old_stdout
        want = {"ret": norm(ret), "trace": CUR["trace"]}
        if state is not None:
            want["state"] = norm(state)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE plugins_cmd_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE plugins_cmd_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()