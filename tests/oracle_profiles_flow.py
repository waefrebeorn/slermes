#!/usr/bin/env python3
"""Oracle for profiles_flow.c — the UNCLAIMED hermes_cli/profiles.py FLOW cores +
leaves (LIVE authority): the credential-path privacy predicates, the name
suggestion + error-message builders, the previous-names cleaner, the Hermes-argv
probes, the pure path algebra, and the seam flows (_standalone_truthy,
_canon_valid, _existing_profile_dir, _wrapper_path, _is_our_wrapper,
_find_our_wrapper, _non_exportable_entries, _notify_multiplexer, _purge_identity,
_live_default_multiplexer, _served_by_running_multiplexer, _retarget_active_profile,
current_profile_name, _inside_git_checkout, _s6_runtime_manager,
_refuse_rename_without_file_tooling, _load_yaml_dict, _seed_file_if_missing,
_clone_file, _resolve_clone_source, _seed_model_config, _strip_multiplex_flag).

Drives the LIVE defs with SCRIPTED I/O: defs are AST-extracted from the quarry
(no heavy hermes_cli.profiles import), module constants seeded (Assign AND
AnnAssign, with the leaf imported constants bound first), and every claimed
sibling (normalize_profile_name, validate_profile_name, get_profile_dir,
_get_wrapper_dir, get_active_profile, set_active_profile, get_active_profile_name)
plus every foreign collaborator replaced by recording stubs returning the SAME
scripted results the C harness feeds its universal seam. Ported-to-ported calls
compose REAL on both sides (_suggest_profile_name / _canon_valid / _wrapper_path /
_is_our_wrapper / _unknown_profile_error), so their internal seam events appear
in both traces.

Path-valued seam results materialize as PPath doubles (str keeps POSIX separators
on this Windows host) whose stat/IO methods fire the is_dir / exists / is_file /
resolve / read_text / write_text / mkdir seams; pure Path() construction uses
PurePosixPath. os is a posixpath-backed double; sys.platform is patched per case
for the win32 wrapper branch. The error-message builders RETURN an exception
object live; the drive projects it to {__raise__, msg} (the C returns the same
spec); composed callers raise it.

Fail-closed: a missing/empty seam script RETURNS a RuntimeError raise spec
(recorded as the event ret, then raised), matching the C harness. Uncaught raises
(incl. the BaseException trio) are caught into a ret spec; `except Exception` /
`except OSError` / `except (OSError, RuntimeError)` sites swallow exactly their
live kinds.

Input: stdin JSON lines from t_port_profiles_flow:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...]}}
Summary: ORACLE profiles_flow: N cases, M mismatches. Fail-closed (exit 2) when
extraction misses defs or zero cases were compared.
"""
import ast
import builtins
import contextlib
import copy
import json
import logging
import os
import posixpath
import re
import stat
import sys
import types
import __future__
from pathlib import Path, PurePath, PurePosixPath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not QUARRY.is_dir():
    print("ORACLE profiles_flow: quarry not found at %s" % QUARRY, file=sys.stderr)
    sys.exit(2)
# The runner scrubs the environment; give the quarry chain a home.
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "profiles_flow_home"))
sys.path.insert(0, str(QUARRY))

SRC = str(QUARRY / "hermes_cli" / "profiles.py")
SRC_TEXT = open(SRC, encoding="utf-8").read()
ANN_FLAG = __future__.annotations.compiler_flag

# Leaf imported constants (real quarry values) needed to seed the module Assigns.
from hermes_constants import PROFILE_ID_RE
from agent.file_safety import HOME_CREDENTIAL_DIRS
from hermes_cli.home_data_layout import CREDENTIAL_STORE_DIRS, HOME_ENV_FILES, SECRET_CACHE_STORES
from hermes_cli.profiles_export_scrub import GATEWAY_CONNECTION_STORES, GATEWAY_CONNECTION_STORE_RE

# harness dispatch name -> live def name
PY = {
    "fold": "_fold",
    "path_is_private": "profile_path_is_private",
    "path_contains_private_store": "profile_path_contains_private_store",
    "suggest_profile_name": "_suggest_profile_name",
    "invalid_profile_name_error": "_invalid_profile_name_error",
    "missing_profile_error": "_missing_profile_error",
    "unknown_profile_error": "_unknown_profile_error",
    "profile_exists_error": "_profile_exists_error",
    "clean_previous_names": "_clean_previous_names",
    "is_hermes_argv": "_is_hermes_argv",
    "argv_profile_selectors": "_argv_profile_selectors",
    "profile_root_for_env_home": "profile_root_for_env_home",
    "parked_marker_path": "parked_marker_path",
    "export_credential_entries": "_export_credential_entries",
    "standalone_truthy": "_standalone_truthy",
    "canon_valid": "_canon_valid",
    "existing_profile_dir": "_existing_profile_dir",
    "wrapper_path": "_wrapper_path",
    "is_our_wrapper": "_is_our_wrapper",
    "find_our_wrapper": "_find_our_wrapper",
    "non_exportable_entries": "_non_exportable_entries",
    "notify_multiplexer": "_notify_multiplexer",
    "purge_identity": "_purge_identity",
    "live_default_multiplexer": "_live_default_multiplexer",
    "served_by_running_multiplexer": "_served_by_running_multiplexer",
    "retarget_active_profile": "_retarget_active_profile",
    "current_profile_name": "current_profile_name",
    "inside_git_checkout": "_inside_git_checkout",
    "s6_runtime_manager": "_s6_runtime_manager",
    "refuse_rename_without_file_tooling": "_refuse_rename_without_file_tooling",
    "load_yaml_dict": "_load_yaml_dict",
    "seed_file_if_missing": "_seed_file_if_missing",
    "clone_file": "_clone_file",
    "resolve_clone_source": "_resolve_clone_source",
    "seed_model_config": "_seed_model_config",
    "strip_multiplex_flag": "_strip_multiplex_flag",
}
TOP_TARGETS = list(PY.values())

CUR = {"scripts": {}, "idx": {}, "trace": []}


class RecLogger:
    def debug(self, *a, **k): pass
    def info(self, *a, **k): pass
    def warning(self, *a, **k): pass
    def error(self, *a, **k): pass


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


def make_exc(kind, msg):
    cls = BASE_EXC.get(kind) or BUILTIN_EXC.get(kind)
    if cls is None:
        cls = type(str(kind), (Exception,), {})
    if cls is KeyError:
        return cls(msg)
    return cls(msg)


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        raise make_exc(r["__raise__"], r.get("msg", ""))
    return r


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
    return maybe_raise(r)


def norm(o):
    if o is None or isinstance(o, (bool, int, str)):
        return o
    if isinstance(o, float):
        return o
    if isinstance(o, PPath):
        return o.s
    if isinstance(o, PurePath):
        return o.as_posix()
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
    """Path double: str keeps POSIX separators; stat/IO methods are seamed.
    Pure component algebra delegates to real PurePosixPath."""
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
    def parents(self):
        return [PPath(str(p)) for p in PurePosixPath(self.s).parents]

    @property
    def parts(self):
        return PurePosixPath(self.s).parts

    @property
    def name(self):
        return PurePosixPath(self.s).name

    def is_dir(self):
        return seam("is_dir", {"path": self.s})

    def exists(self):
        return seam("exists", {"path": self.s})

    def is_file(self):
        return seam("is_file", {"path": self.s})

    def is_symlink(self):
        return seam("is_symlink", {"path": self.s})

    def resolve(self, strict=False):
        r = seam("resolve", {"path": self.s})
        return PPath(r) if isinstance(r, str) else r

    def read_text(self, encoding=None, errors=None):
        return seam("read_text", {"path": self.s})

    def write_text(self, text, encoding=None, errors=None):
        return seam("write_text", {"path": self.s, "text": text})

    def mkdir(self, parents=False, exist_ok=False):
        return seam("mkdir", {"path": self.s})


class WrapperFile:
    def __init__(self, path):
        self.path = str(path)

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False

    def read(self, n=-1):
        return seam("wrapper_read", {"path": self.path})


def open_rec(path, *a, **k):
    return WrapperFile(path)


class MgrDouble:
    def __init__(self, marker):
        self.marker = marker

    def __str__(self):
        return self.marker

    def supports_runtime_registration(self):
        return seam("supports_runtime_registration", {})


class BackendDouble:
    def __init__(self, name):
        self.name = name

    def supports_file_tooling(self):
        return seam("supports_file_tooling", {})


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


def _pstr(v):
    if isinstance(v, (PPath, PurePath)):
        return str(v)
    return v


def lstat_rec(path):
    r = seam("lstat", {"path": _pstr(path)})
    if isinstance(r, dict):
        return types.SimpleNamespace(**r)
    return r


def get_service_manager_rec():
    r = seam("get_service_manager", {})
    if isinstance(r, str):
        return MgrDouble(r)
    return r


def get_config_backend_rec():
    r = seam("get_config_backend", {})
    if isinstance(r, dict) and "name" in r:
        return BackendDouble(r["name"])
    return r


def copy2_rec(src, dst):
    return seam("copy2", {"src": _pstr(src), "dst": _pstr(dst)})


def chmod_rec(path, mode):
    return seam("chmod", {"path": _pstr(path), "mode": mode})


# ── extraction ──

def seam_bindings():
    return {
        "print": lambda *a, **k: seam("print", {"args": list(a)}),
        "normalize_profile_name": mk("normalize_profile_name", lambda a, k: {"name": a[0]}),
        "validate_profile_name": mk("validate_profile_name", lambda a, k: {"name": a[0]}),
        "get_profile_dir": path_rec("get_profile_dir", lambda a, k: {"name": a[0]}),
        "_get_wrapper_dir": path_rec("_get_wrapper_dir", lambda a, k: {}),
        "get_active_profile": mk("get_active_profile", lambda a, k: {}),
        "set_active_profile": mk("set_active_profile", lambda a, k: {"name": a[0]}),
        "get_active_profile_name": mk("get_active_profile_name", lambda a, k: {}),
        "require_file_tooling": mk("require_file_tooling", lambda a, k: {"label": a[0]}),
        "config_exists": mk("config_exists", lambda a, k: {"path": _pstr(a[0])}),
        "_config_backend": types.SimpleNamespace(get_config_backend=get_config_backend_rec),
        "open": open_rec,
    }


def install_module_stubs():
    """sys.modules stubs for every in-function import the targets execute."""
    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    gw = mod("gateway")
    gw.__path__ = []
    mod("gateway.config", _bool_token=mk("_bool_token", lambda a, k: {"value": a[0]}))

    hcli = mod("hermes_cli")
    hcli.__path__ = []
    gms = mod("hermes_cli.gateway_multiplex_served",
              notify_multiplexer_profiles_changed=mk("notify_multiplexer_profiles_changed",
                                                     lambda a, k: {"canon": a[0]}),
              recorded_served_profiles=mk("recorded_served_profiles", lambda a, k: {}))
    hcli.gateway_multiplex_served = gms
    pi = mod("hermes_cli.profile_identity",
             purge_profile_identity=mk("purge_profile_identity", lambda a, k: {"canon": a[0]}))
    hcli.profile_identity = pi
    g = mod("hermes_cli.gateway",
            named_profile_served_by_running_multiplexer=mk(
                "named_profile_served_by_running_multiplexer", lambda a, k: {"profile_name": a[0]}))
    hcli.gateway = g
    sm = mod("hermes_cli.service_manager",
             detect_service_manager=mk("detect_service_manager", lambda a, k: {}),
             get_service_manager=get_service_manager_rec)
    hcli.service_manager = sm
    cfg = mod("hermes_cli.config",
              read_user_config_raw=mk("read_user_config_raw", lambda a, k: {"path": _pstr(a[0])}),
              atomic_config_write=mk("atomic_config_write",
                                     lambda a, k: {"path": _pstr(a[0]), "config": copy.deepcopy(a[1])}),
              atomic_config_replace=mk("atomic_config_replace",
                                       lambda a, k: {"path": _pstr(a[0]), "config": copy.deepcopy(a[1])}))
    hcli.config = cfg
    pms = mod("hermes_cli.profile_model_seed",
              launch_model_seed=mk("launch_model_seed", lambda a, k: {"config": copy.deepcopy(a[0])}))
    hcli.profile_model_seed = pms

    mod("hermes_constants",
        get_hermes_home_override=mk("get_hermes_home_override", lambda a, k: {}),
        get_hermes_home=path_rec("get_hermes_home", lambda a, k: {}))
    mod("hermes_yaml", safe_load=mk("safe_load", lambda a, k: {"text": a[0]}))


def extract():
    ns = {
        "__name__": "hermes_cli.profiles_flow_extracted",
        "__builtins__": builtins.__dict__,
        "re": re, "stat": stat, "sys": sys, "json": json, "logging": logging,
        "contextlib": contextlib, "copy": copy,
        "Path": PurePosixPath, "PurePosixPath": PurePosixPath,
        "Optional": object, "Dict": dict, "List": list, "Tuple": tuple,
        "shutil": types.SimpleNamespace(copy2=copy2_rec),
        "os": types.SimpleNamespace(path=posixpath, lstat=lstat_rec, chmod=chmod_rec,
                                    environ={}, name="posix"),
        "logger": RecLogger(),
        # leaf imported constants for the module Assigns
        "PROFILE_ID_RE": PROFILE_ID_RE,
        "HOME_CREDENTIAL_DIRS": HOME_CREDENTIAL_DIRS,
        "HOME_ENV_FILES": HOME_ENV_FILES,
        "CREDENTIAL_STORE_DIRS": CREDENTIAL_STORE_DIRS,
        "SECRET_CACHE_STORES": SECRET_CACHE_STORES,
        "_GATEWAY_CONNECTION_STORES": GATEWAY_CONNECTION_STORES,
        "_GATEWAY_CONNECTION_STORE_RE": GATEWAY_CONNECTION_STORE_RE,
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
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in TOP_TARGETS:
            exec_node(node)
    ns["logger"] = RecLogger()   # logger = logging.getLogger(...) is an Assign

    missing = [n for n in TOP_TARGETS if n not in ns]
    if missing:
        print("ORACLE profiles_flow: FATAL missing defs: %s" % sorted(missing), file=sys.stderr)
        sys.exit(2)
    return ns


# ── drive ──

def _pp(inp, key):
    return PPath(inp.get(key, ""))


def drive(ns, func, inp):
    ns["os"].environ = inp.get("environ") or {}
    if func == "fold":
        return ns["_fold"](tuple(inp.get("parts") or [])), None
    if func == "path_is_private":
        return ns["profile_path_is_private"](tuple(inp.get("parts") or [])), None
    if func == "path_contains_private_store":
        return ns["profile_path_contains_private_store"](tuple(inp.get("parts") or [])), None
    if func == "suggest_profile_name":
        return ns["_suggest_profile_name"](inp.get("name", "")), None
    if func == "invalid_profile_name_error":
        e = ns["_invalid_profile_name_error"](inp.get("name", ""))
        return {"__raise__": type(e).__name__, "msg": str(e)}, None
    if func == "missing_profile_error":
        e = ns["_missing_profile_error"](inp.get("canon", ""))
        return {"__raise__": type(e).__name__, "msg": str(e)}, None
    if func == "unknown_profile_error":
        e = ns["_unknown_profile_error"](inp.get("canon", ""))
        return {"__raise__": type(e).__name__, "msg": str(e)}, None
    if func == "profile_exists_error":
        e = ns["_profile_exists_error"](inp.get("canon", ""))
        return {"__raise__": type(e).__name__, "msg": str(e)}, None
    if func == "clean_previous_names":
        return ns["_clean_previous_names"](inp.get("raw")), None
    if func == "is_hermes_argv":
        return ns["_is_hermes_argv"](inp.get("argv") or []), None
    if func == "argv_profile_selectors":
        return list(ns["_argv_profile_selectors"](inp.get("argv") or [])), None
    if func == "profile_root_for_env_home":
        return ns["profile_root_for_env_home"](inp.get("env_home", ""),
                                               PurePosixPath(inp.get("default_root", ""))), None
    if func == "parked_marker_path":
        return ns["parked_marker_path"](PurePosixPath(inp.get("home", ""))), None
    if func == "export_credential_entries":
        return ns["_export_credential_entries"](inp.get("directory", ""),
                                                inp.get("contents") or []), None
    if func == "standalone_truthy":
        return ns["_standalone_truthy"](inp.get("value")), None
    if func == "canon_valid":
        return ns["_canon_valid"](inp.get("name", "")), None
    if func == "existing_profile_dir":
        return ns["_existing_profile_dir"](inp.get("name", "")), None
    if func == "wrapper_path":
        return ns["_wrapper_path"](inp.get("alias", "")), None
    if func == "is_our_wrapper":
        return ns["_is_our_wrapper"](_pp(inp, "path")), None
    if func == "find_our_wrapper":
        return ns["_find_our_wrapper"](inp.get("alias", "")), None
    if func == "non_exportable_entries":
        return ns["_non_exportable_entries"](inp.get("directory", ""),
                                             inp.get("contents") or []), None
    if func == "notify_multiplexer":
        return ns["_notify_multiplexer"](inp.get("canon", "")), None
    if func == "purge_identity":
        return ns["_purge_identity"](inp.get("canon", "")), None
    if func == "live_default_multiplexer":
        return ns["_live_default_multiplexer"](), None
    if func == "served_by_running_multiplexer":
        return ns["_served_by_running_multiplexer"](inp.get("profile_name", "")), None
    if func == "retarget_active_profile":
        return ns["_retarget_active_profile"](inp.get("old", ""), inp.get("new", ""),
                                              inp.get("message", "")), None
    if func == "current_profile_name":
        return ns["current_profile_name"](inp.get("default")), None
    if func == "inside_git_checkout":
        return ns["_inside_git_checkout"](_pp(inp, "path")), None
    if func == "s6_runtime_manager":
        return ns["_s6_runtime_manager"](), None
    if func == "refuse_rename_without_file_tooling":
        return ns["_refuse_rename_without_file_tooling"](inp.get("old_canon", "")), None
    if func == "load_yaml_dict":
        return ns["_load_yaml_dict"](_pp(inp, "path")), None
    if func == "seed_file_if_missing":
        return ns["_seed_file_if_missing"](_pp(inp, "path"), inp.get("text", ""),
                                           inp.get("mode")), None
    if func == "clone_file":
        return ns["_clone_file"](_pp(inp, "source_dir"), _pp(inp, "profile_dir"),
                                 inp.get("relpath", "")), None
    if func == "resolve_clone_source":
        return ns["_resolve_clone_source"](inp.get("clone_from")), None
    if func == "seed_model_config":
        return ns["_seed_model_config"](_pp(inp, "profile_dir")), None
    if func == "strip_multiplex_flag":
        return ns["_strip_multiplex_flag"](_pp(inp, "config_path")), None
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
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        per_func[func] = per_func.get(func, 0) + 1
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        if func not in PY:
            mism += 1
            print("MISMATCH %s: unknown func (not in PY map)" % func)
            continue
        oldplat = sys.platform
        sys.platform = "win32" if (inp.get("env") or {}).get("win32") else "linux"
        try:
            ret, state = drive(ns, func, inp)
        except BaseException as ex:
            ret = {"__raise__": type(ex).__name__, "msg": str(ex)}
            state = None
        finally:
            sys.platform = oldplat
        want = {"ret": norm(ret), "trace": CUR["trace"]}
        if state is not None:
            want["state"] = norm(state)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE profiles_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE profiles_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
