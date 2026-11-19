#!/usr/bin/env python3
"""Oracle for auth_flow — the UNCLAIMED auth-surface FLOW cores of
hermes_cli/auth.py (LIVE authority, py3.14 quarry venv).

Ports covered: registry/config leaves (_api_key_provider, _empty_auth_store,
_store_section, _provider_state_in, _slot_selects, _model_level_key_env,
_config_selects_provider, _explicit_env_credentials_present,
_keyless_provider_has_explicit_config, _active_provider_is, _plugin_aliases,
_config_model_provider, _credential_profile, _env_key_auto_detected,
_scoped_key_env_reader, _openrouter_auto_detected,
_logged_in_oauth_active_provider, _refuse_env_adoption_if_config_corrupt),
secret/env gates (_env_secret, _pool_entry_is_explicit,
_explicit_pool_entry_present), the lock cluster (_is_lock_contention,
_lock_holder_hint, _kernel_lock, _stamp_lock_holder_pid,
_clear_stamped_lock_holder_pid), token/TTL markers (_tls_state_from_verify,
_utc_now_z, _last_auth_error_marker, _quarantine_flat_oauth_state,
_is_terminal_refresh_error, _codex_pool_rate_limited_status) and
persistence/status (_resolved_key, _save_private_json,
_save_active_provider_state, suppressed_credential_sources,
_get_aws_sdk_auth_status, _external_process_spec).

Claimed siblings (has_usable_secret, looks_like_openrouter_key,
read_credential_pool, _load_auth_store, get_auth_status, _save_provider_state,
_save_auth_store, _auth_store_lock, _auth_file_path) are SEAMED; unclaimed
siblings are extracted and run for real.

Drives the LIVE functions with SCRIPTED I/O: defs AST-extracted from the
quarry (no heavy import), module constants seeded (Assign AND AnnAssign,
ProviderConfig/OAuthProviderFlow/AuthError classes exec'd first), every effect
replaced by a recording stub returning the SAME scripted results the C
harness feeds its universal seam. Compares the ordered DECISION SEQUENCE
(trace of seam calls) plus the return value (and the mutated state window for
store_section / quarantine_flat_oauth_state). Scripted {"__raise__": cls}
raises a REAL exception of that class; drive() catches BaseException so
KeyboardInterrupt/SystemExit pass through live except-Exception clauses
exactly as the C port propagates them. A MISSING seam key fail-closes
(RuntimeError) on both sides; a scripted null is a valid None result.

Input: stdin JSON lines from t_port_auth_flow:
    {"func": ..., "in": {...}, "out": {"ret":..., "trace":[...][, "state":...]}}
Summary: ORACLE auth_flow: N cases, M mismatches. Fail-closed (exit 2) when
extraction misses defs or zero cases were compared.
--self-test: drive every fixture case (no C) and report binding/script gaps.
"""
import ast
import copy
import json
import os
import sys
import types
from dataclasses import asdict
from pathlib import Path, PurePosixPath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "auth_flow_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "auth.py")
SRC_CONST = os.path.join(QUARRY, "hermes_cli", "auth_constants.py")

# C harness func -> live python name.
PY = {
    "api_key_provider": "_api_key_provider",
    "empty_auth_store": "_empty_auth_store",
    "store_section": "_store_section",
    "provider_state_in": "_provider_state_in",
    "slot_selects": "_slot_selects",
    "model_level_key_env": "_model_level_key_env",
    "active_provider_is": "_active_provider_is",
    "config_selects_provider": "_config_selects_provider",
    "env_secret": "_env_secret",
    "explicit_env_credentials_present": "_explicit_env_credentials_present",
    "keyless_provider_has_explicit_config": "_keyless_provider_has_explicit_config",
    "refuse_env_adoption_if_config_corrupt": "_refuse_env_adoption_if_config_corrupt",
    "plugin_aliases": "_plugin_aliases",
    "scoped_key_env_reader": "_scoped_key_env_reader",
    "openrouter_auto_detected": "_openrouter_auto_detected",
    "logged_in_oauth_active_provider": "_logged_in_oauth_active_provider",
    "config_model_provider": "_config_model_provider",
    "credential_profile": "_credential_profile",
    "env_key_auto_detected": "_env_key_auto_detected",
    "pool_entry_is_explicit": "_pool_entry_is_explicit",
    "explicit_pool_entry_present": "_explicit_pool_entry_present",
    "is_lock_contention": "_is_lock_contention",
    "lock_holder_hint": "_lock_holder_hint",
    "kernel_lock": "_kernel_lock",
    "stamp_lock_holder_pid": "_stamp_lock_holder_pid",
    "clear_stamped_lock_holder_pid": "_clear_stamped_lock_holder_pid",
    "tls_state_from_verify": "_tls_state_from_verify",
    "utc_now_z": "_utc_now_z",
    "last_auth_error_marker": "_last_auth_error_marker",
    "quarantine_flat_oauth_state": "_quarantine_flat_oauth_state",
    "is_terminal_refresh_error": "_is_terminal_refresh_error",
    "codex_pool_rate_limited_status": "_codex_pool_rate_limited_status",
    "resolved_key": "_resolved_key",
    "save_private_json": "_save_private_json",
    "save_active_provider_state": "_save_active_provider_state",
    "suppressed_credential_sources": "suppressed_credential_sources",
    "get_aws_sdk_auth_status": "_get_aws_sdk_auth_status",
    "external_process_spec": "_external_process_spec",
}
WANTED = set(PY.values())
STATEFUL = {"store_section", "quarantine_flat_oauth_state"}

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
        if name == "AuthError":
            raise NS["AuthError"](r.get("msg", ""))
        cls = EXC_MAP.get(name)
        if cls is None:
            cls = type(name, (Exception,), {})
        raise cls(r.get("msg", ""))
    return r


NS = {}  # extraction namespace (set by extract); maybe_raise needs AuthError


def posix(p):
    if p is None:
        return None
    if isinstance(p, PathDouble):
        return p.posix
    if isinstance(p, PurePosixPath):
        return p.as_posix()
    if isinstance(p, Path):
        return p.as_posix()
    # PurePosixPath, NOT Path: on Windows Path mangles POSIX paths
    # (backslashes, UNC '//host/share' trailing-slash artifacts).
    return PurePosixPath(p).as_posix()


class PathDouble:
    """A Path stand-in: POSIX stringification + scripted resolve/read_text."""

    def __init__(self, s):
        # PurePosixPath: the port's documented POSIX path domain (Windows
        # Path would flip separators and treat '//h/x' as a UNC share).
        self.posix = PurePosixPath(s).as_posix() if s is not None else ""

    def __str__(self):
        return self.posix

    def __fspath__(self):
        return self.posix

    @property
    def name(self):
        p = self.posix.rstrip("/")
        if not p:
            return ""
        i = p.rfind("/")
        return p[i + 1:]

    @property
    def parent(self):
        # PurePosixPath parent: drop empty + '.' parts, keep '..',
        # exactly two leading slashes preserved (3+ collapse to one).
        s = self.posix
        lead = 0
        while lead < len(s) and s[lead] == "/":
            lead += 1
        root = "//" if lead == 2 else ("/" if lead >= 1 else "")
        parts = [p for p in s[lead:].split("/") if p not in ("", ".")]
        if not parts:
            return PathDouble(root if root else ".")
        parts = parts[:-1]
        if not parts:
            return PathDouble(root if root else ".")
        return PathDouble(root + "/".join(parts))

    def resolve(self, strict=False):
        return seam("path_resolve", {"path": self.posix, "strict": strict})

    def read_text(self, encoding=None, errors=None):
        return seam("path_read_text", {"path": self.posix, "encoding": encoding,
                                       "errors": errors})


class OsDouble:
    def __init__(self, name):
        self.name = name

    def getpid(self):
        return seam("os_getpid", {})

    def kill(self, pid, sig):
        return seam("os_kill", {"pid": pid, "sig": sig})

    def getenv(self, name, default=None):
        return seam("os_getenv", {"name": name, "default": default})


class FcntlDouble:
    LOCK_EX = 2
    LOCK_NB = 4
    LOCK_UN = 8

    def flock(self, fd, flags):
        return seam("fcntl_flock", {"fd": fd, "flags": flags})


class MsvcrtDouble:
    LK_NBLCK = 2
    LK_UNLCK = 0

    def locking(self, fd, mode, nbytes):
        return seam("msvcrt_locking", {"fd": fd, "mode": mode, "nbytes": nbytes})


class LockFileDouble:
    def seek(self, offset):
        return seam("file_seek", {"offset": offset})

    def fileno(self):
        return seam("file_fileno", {})

    def truncate(self, size):
        return seam("file_truncate", {"size": size})

    def write(self, data):
        return seam("file_write", {"data": data})

    def flush(self):
        return seam("file_flush", {})


class PoolDouble:
    def has_credentials(self):
        return seam("has_credentials", {})


class DTDouble:
    @classmethod
    def now(cls, tz=None):
        r = seam("datetime_now", {})
        return types.SimpleNamespace(isoformat=lambda: r)


class AuthLockCM:
    def __enter__(self):
        return seam("auth_store_lock_enter", {})

    def __exit__(self, et, ev, tb):
        seam("auth_store_lock_exit", {})
        return False  # never suppress: body raises propagate after exit


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
    if isinstance(o, (frozenset, set)):
        return sorted(norm(v) for v in o)
    if callable(o):
        return {"__callable__": getattr(o, "__au_callable__", True)}
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    return str(o)


# ── extraction ──

def extract():
    import errno
    import logging
    import re
    import shlex
    import threading
    import time
    from dataclasses import dataclass, field
    from datetime import datetime, timezone, UTC
    from functools import partial
    from typing import Any, Callable, Dict, FrozenSet, Iterable, List, Optional, Tuple

    src = open(SRC, encoding="utf-8").read()
    src_const = open(SRC_CONST, encoding="utf-8").read()

    class RecLogger:
        def debug(self, *a, **k): pass
        def info(self, *a, **k): pass
        def warning(self, *a, **k): pass
        def error(self, *a, **k): pass

    ns = {
        "__name__": "hermes_cli.auth_extracted",
        "json": json, "os": os, "re": re, "sys": sys, "time": time,
        "errno": errno, "shlex": shlex, "threading": threading, "logging": logging,
        "Path": Path, "datetime": datetime, "timezone": timezone, "UTC": UTC,
        "dataclass": dataclass, "field": field, "partial": partial,
        "Any": Any, "Callable": Callable, "Dict": Dict, "FrozenSet": FrozenSet,
        "Iterable": Iterable, "List": List, "Optional": Optional, "Tuple": Tuple,
        "logger": RecLogger(),
    }
    tree = ast.parse(src)
    # classes FIRST (the OAUTH_PROVIDER_FLOWS / _REGISTRY_ROWS constants need them)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in ("ProviderConfig", "OAuthProviderFlow"):
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    const_tree = ast.parse(src_const)
    for node in const_tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "AuthError":
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC_CONST, "exec"), ns)
    # auth.py imports AUTH_STORE_VERSION / CODEX_RATE_LIMITED_CODE / ... from
    # auth_constants at module top — seed that module's literal Assigns too.
    for node in const_tree.body:
        if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC_CONST, "exec"), ns)
            except Exception:
                pass
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC_CONST, "exec"), ns)
            except Exception:
                pass
    const_nodes = []
    for node in tree.body:
        if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
            const_nodes.append(node)
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            const_nodes.append(node)
    for node in const_nodes:
        try:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        except Exception:
            pass
    # the module's `logger = logging.getLogger(__name__)` Assign overwrites the
    # seeded recorder — re-set it AFTER the constant loop.
    ns["logger"] = RecLogger()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    missing = WANTED - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    for k in ("AuthError", "ProviderConfig", "OAuthProviderFlow", "OAUTH_PROVIDER_FLOWS",
              "_PROVIDER_ALIASES", "_LOCK_CONTENTION_ERRNOS"):
        if k not in ns:
            print("FATAL missing constant/class: %s" % k)
            sys.exit(2)
    global NS
    NS = ns
    return ns


# ── recording stubs ──

def make_import_module(modname, attrs):
    """attrs: {attribute: (import_seam_name, fn)}; getattr fires the import seam.
    NO __path__ on leaf stubs: with __path__, CPython's _handle_fromlist probes
    hasattr() before IMPORT_FROM's getattr, firing every import seam TWICE."""
    class M(types.ModuleType):
        def __getattr__(self, name):
            if name in attrs:
                ise, fn = attrs[name]
                seam(ise, {})
                return fn
            raise AttributeError(name)
    m = M(modname)
    sys.modules[modname] = m
    return m


def list_providers_stub():
    r = seam("list_providers", {})
    if isinstance(r, list):
        return [types.SimpleNamespace(**d) for d in r]
    return r  # None/non-list: live iteration raises TypeError


def get_provider_stub(provider_id):
    r = seam("get_provider", {"provider_id": provider_id})
    if isinstance(r, dict):
        return types.SimpleNamespace(**r)
    return r


def get_provider_profile_stub(provider_id):
    r = seam("get_provider_profile", {"provider_id": provider_id})
    if isinstance(r, dict):
        return types.SimpleNamespace(**r)
    return r


def canonical_rows_stub():
    r = seam("canonical_registry_rows", {})
    if isinstance(r, list):
        # pconfig is consumed via attribute access live -> SimpleNamespace double
        return [(p[0], types.SimpleNamespace(**p[1])) for p in r]
    return r


def load_pool_stub(provider_id):
    r = seam("load_pool", {"provider_id": provider_id})
    if r is None:
        return None
    return PoolDouble()


def scoped_key_env_fn(name):
    return seam("scoped_key_env", {"name": name})


scoped_key_env_fn.__au_callable__ = "scoped_key_env"


def auth_store_lock(timeout_seconds=15.0, *, target_path=None):
    seam("auth_store_lock", {"timeout_seconds": timeout_seconds,
                             "target_path": posix(target_path)})
    return AuthLockCM()


def install_stubs(ns):
    # claimed auth.py siblings (inert recorders; mutation flows echo no state)
    ns["has_usable_secret"] = lambda value, min_length=4: seam("has_usable_secret", {"value": value})
    ns["looks_like_openrouter_key"] = lambda value: seam("looks_like_openrouter_key", {"value": value})
    ns["read_credential_pool"] = lambda provider_id=None: seam("read_credential_pool", {"provider_id": provider_id})
    ns["_load_auth_store"] = lambda auth_file=None: seam("load_auth_store", {})
    ns["get_auth_status"] = lambda provider_id=None: seam("get_auth_status", {"provider_id": provider_id})
    ns["_save_provider_state"] = lambda auth_store, provider_id, state: seam(
        "save_provider_state", {"auth_store": auth_store, "provider_id": provider_id, "state": state})
    ns["_save_auth_store"] = lambda auth_store, target_path=None: seam(
        "save_auth_store", {"auth_store": auth_store, "target_path": posix(target_path)})
    ns["_auth_store_lock"] = auth_store_lock
    ns["_auth_file_path"] = lambda: seam("auth_file_path", {})
    # bare globals (module-top imports in auth.py)
    ns["get_config_path"] = lambda: seam("get_config_path", {})
    ns["_canonical_registry_rows"] = canonical_rows_stub
    ns["_codex_pool_rate_limit_status"] = lambda: seam("codex_pool_rate_limit_status", {})
    ns["provider_env_base_url"] = lambda pconfig: seam(
        "provider_env_base_url",
        {"pconfig": vars(pconfig) if isinstance(pconfig, types.SimpleNamespace) else pconfig})
    ns["secure_parent_dir"] = lambda target: seam("secure_parent_dir", {"path": posix(target)})

    def atomic_json_write(target, data, mode=0o600, fsync_dir=False, **kw):
        return seam("atomic_json_write", {"path": posix(target), "data": data,
                                          "mode": mode, "fsync_dir": fsync_dir, **kw})
    ns["atomic_json_write"] = atomic_json_write

    # stdlib doubles
    ns["datetime"] = DTDouble
    ns["shlex"] = types.SimpleNamespace(split=lambda raw: seam("shlex_split", {"raw": raw}))

    # stub packages (every intermediate package registered)
    for pkg in ("hermes_cli", "agent"):
        if pkg not in sys.modules or not isinstance(sys.modules[pkg], types.ModuleType):
            m = types.ModuleType(pkg)
            m.__path__ = []
            sys.modules[pkg] = m
        elif not hasattr(sys.modules[pkg], "__path__"):
            sys.modules[pkg].__path__ = []

    make_import_module("hermes_cli.config", {
        "load_config": ("import_load_config", lambda: seam("load_config", {})),
        "get_env_value_prefer_dotenv": ("import_get_env_value_prefer_dotenv",
                                        lambda name: seam("get_env_value_prefer_dotenv", {"name": name})),
    })
    make_import_module("hermes_cli.providers", {
        "get_provider": ("import_get_provider", get_provider_stub),
    })
    make_import_module("agent.vertex_adapter", {
        "has_explicit_vertex_config": ("import_has_explicit_vertex_config",
                                       lambda: seam("has_explicit_vertex_config", {})),
    })
    make_import_module("hermes_cli.config_read_errors", {
        "get_active_config_parse_failure": ("import_get_active_config_parse_failure",
                                            lambda: seam("get_active_config_parse_failure", {})),
    })
    make_import_module("providers", {
        "list_providers": ("import_list_providers", list_providers_stub),
        "get_provider_profile": ("import_get_provider_profile", get_provider_profile_stub),
    })
    make_import_module("agent.auxiliary_client", {
        "_scoped_key_env": ("import_scoped_key_env", scoped_key_env_fn),
    })
    make_import_module("agent.credential_pool", {
        "load_pool": ("import_load_pool", load_pool_stub),
    })
    make_import_module("hermes_cli.anon_auth", {
        "free_inference_active": ("import_free_inference_active",
                                  lambda: seam("free_inference_active", {})),
        "has_guest": ("import_has_guest", lambda: seam("has_guest", {})),
    })
    make_import_module("hermes_cli.runtime_provider_custom", {
        "has_named_custom_provider": ("import_has_named_custom_provider",
                                      lambda provider: seam("has_named_custom_provider", {"provider": provider})),
    })
    make_import_module("hermes_cli.runtime_provider", {
        "_config_base_url_trustworthy_for_bare_custom": (
            "import_config_base_url_trustworthy_for_bare_custom",
            lambda base_url, provider: seam("config_base_url_trustworthy_for_bare_custom",
                                            {"base_url": base_url, "provider": provider})),
    })
    make_import_module("agent.bedrock_adapter", {
        "has_aws_credentials": ("import_has_aws_credentials", lambda: seam("has_aws_credentials", {})),
    })
    make_import_module("hermes_cli.auth_external_process", {
        "external_process_command": (
            "import_external_process_command",
            lambda profile: seam("external_process_command",
                                 {"profile": vars(profile) if isinstance(profile, types.SimpleNamespace) else profile})),
        "resolve_external_process_command": (
            "import_resolve_external_process_command",
            lambda command: seam("resolve_external_process_command", {"command": command})),
    })
    make_import_module("hermes_constants", {
        "mkdir_under_hermes_home": ("import_mkdir_under_hermes_home",
                                    lambda path: seam("mkdir_under_hermes_home", {"path": posix(path)})),
    })


# ── materialization ──

def mat_exc(d):
    if d is None:
        return None
    kind = d.get("kind", "AuthError")
    if kind == "AuthError":
        return NS["AuthError"](d.get("message", ""), provider=d.get("provider", ""),
                               code=d.get("code"), relogin_required=d.get("relogin_required", False))
    cls = EXC_MAP.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    if issubclass(cls, OSError):
        return cls(d.get("errno"), d.get("message", ""))
    return cls(d.get("message", ""))


def mat_keyenv(d):
    kind = (d or {}).get("__callable__", "scoped_key_env")
    if kind == "os_getenv":
        def fn(name):
            return seam("os_getenv", {"name": name}) or ""
        fn.__au_callable__ = "os_getenv"
    else:
        def fn(name):
            return seam("scoped_key_env", {"name": name})
        fn.__au_callable__ = "scoped_key_env"
    return fn


# ── drive ──

def drive(ns, func, inp):
    if func not in PY:
        print("FATAL unknown func: %s" % func)
        sys.exit(2)
    name = PY[func]
    f = ns[name]
    # per-case platform / registry doubles
    backend = inp.get("lock_backend", "msvcrt")
    ns["os"] = OsDouble(inp.get("os_name", "nt"))
    if backend == "fcntl":
        ns["fcntl"] = FcntlDouble()
        ns["msvcrt"] = None
    elif backend == "msvcrt":
        ns["fcntl"] = None
        ns["msvcrt"] = MsvcrtDouble()
    else:
        ns["fcntl"] = None
        ns["msvcrt"] = None
    ns["PROVIDER_REGISTRY"] = {k: types.SimpleNamespace(**v)
                               for k, v in (inp.get("registry") or {}).items()}

    if func == "api_key_provider":
        return norm(asdict(f(inp.get("id", ""), inp.get("name", ""),
                             inp.get("inference_base_url", ""),
                             tuple(inp.get("api_key_env_vars") or ()),
                             base_url_env_var=inp.get("base_url_env_var", ""),
                             auth_type=inp.get("auth_type", "api_key"))))
    if func == "empty_auth_store":
        return f()
    if func == "store_section":
        state = copy.deepcopy(inp.get("state") or {})
        CUR["statewin"] = state
        return f(state, inp.get("key", ""))
    if func == "provider_state_in":
        return f(inp.get("store"), inp.get("provider_id", ""))
    if func == "slot_selects":
        return f(inp.get("slot"), inp.get("normalized", ""))
    if func == "model_level_key_env":
        return f(inp.get("provider_id", ""))
    if func == "active_provider_is":
        return f(inp.get("normalized", ""))
    if func == "config_selects_provider":
        return f(inp.get("normalized", ""))
    if func == "env_secret":
        return f(inp.get("name", ""))
    if func == "explicit_env_credentials_present":
        return f(inp.get("normalized", ""))
    if func == "keyless_provider_has_explicit_config":
        return f(inp.get("normalized", ""))
    if func == "refuse_env_adoption_if_config_corrupt":
        f()
        return None
    if func == "plugin_aliases":
        return f()
    if func == "scoped_key_env_reader":
        r = f()
        if callable(r) and not hasattr(r, "__au_callable__"):
            # the live ImportError fallback lambda reads os.getenv
            r.__au_callable__ = "os_getenv"
        return norm(r)
    if func == "openrouter_auto_detected":
        return f(mat_keyenv(inp.get("keyenv")))
    if func == "logged_in_oauth_active_provider":
        return f(skip_free_tier=inp.get("skip_free_tier", False))
    if func == "config_model_provider":
        return norm(f())
    if func == "credential_profile":
        return norm(f(inp.get("provider_id", "")))
    if func == "env_key_auto_detected":
        return f(mat_keyenv(inp.get("keyenv")), inp.get("oauth_active"))
    if func == "pool_entry_is_explicit":
        return f(inp.get("entry"))
    if func == "explicit_pool_entry_present":
        return f(inp.get("normalized", ""))
    if func == "is_lock_contention":
        return f(mat_exc(inp.get("exc")))
    if func == "lock_holder_hint":
        return f(PathDouble(inp.get("path", "")))
    if func == "kernel_lock":
        f(LockFileDouble(), inp.get("acquire", True))
        return None
    if func == "stamp_lock_holder_pid":
        f(LockFileDouble())
        return None
    if func == "clear_stamped_lock_holder_pid":
        f(LockFileDouble())
        return None
    if func == "tls_state_from_verify":
        return f(inp.get("verify"))
    if func == "utc_now_z":
        return f()
    if func == "last_auth_error_marker":
        return f(inp.get("provider", ""), mat_exc(inp.get("error")),
                 reason=inp.get("reason", ""), default_code=inp.get("default_code"))
    if func == "quarantine_flat_oauth_state":
        state = copy.deepcopy(inp.get("state") or {})
        CUR["statewin"] = state
        f(state, inp.get("provider", ""), mat_exc(inp.get("error")))
        return None
    if func == "is_terminal_refresh_error":
        return f(mat_exc(inp.get("exc")), inp.get("provider", ""))
    if func == "codex_pool_rate_limited_status":
        return f()
    if func == "resolved_key":
        return f(PathDouble(inp.get("path", "")))
    if func == "save_private_json":
        f(PathDouble(inp.get("target", "")), inp.get("data"),
          fsync_dir=inp.get("fsync_dir", False), **(inp.get("dump_kwargs") or {}))
        return None
    if func == "save_active_provider_state":
        return f(inp.get("provider_id", ""), copy.deepcopy(inp.get("state") or {}))
    if func == "suppressed_credential_sources":
        return norm(f(inp.get("provider_id", "")))
    if func == "get_aws_sdk_auth_status":
        return f(inp.get("target", ""))
    if func == "external_process_spec":
        return norm(f(types.SimpleNamespace(**(inp.get("pconfig") or {}))))
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
    "has_usable_secret", "looks_like_openrouter_key", "read_credential_pool",
    "load_auth_store", "get_auth_status", "save_provider_state", "save_auth_store",
    "auth_store_lock", "auth_store_lock_enter", "auth_store_lock_exit",
    "auth_file_path", "get_config_path", "canonical_registry_rows",
    "codex_pool_rate_limit_status", "provider_env_base_url", "secure_parent_dir",
    "atomic_json_write", "datetime_now", "path_resolve", "path_read_text",
    "os_getpid", "os_kill", "os_getenv", "file_seek", "file_fileno",
    "file_truncate", "file_write", "file_flush", "fcntl_flock", "msvcrt_locking",
    "shlex_split", "load_config", "get_env_value_prefer_dotenv", "get_provider",
    "has_explicit_vertex_config", "get_active_config_parse_failure",
    "list_providers", "scoped_key_env", "load_pool", "has_credentials",
    "free_inference_active", "has_guest", "has_named_custom_provider",
    "config_base_url_trustworthy_for_bare_custom", "get_provider_profile",
    "has_aws_credentials", "external_process_command",
    "resolve_external_process_command", "mkdir_under_hermes_home",
]


def self_test(ns):
    """Drive every fixture case live (no C): surface binding bugs / script gaps."""
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    fixture = _REPO / "tests" / "fixtures" / "auth_flow.jsonl"
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
            # an AttributeError naming a ported method or seam stub is a binding
            # bug, not a legitimate scripted raise
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
            want = {"ret": norm(ret), "trace": CUR["trace"]}
        except BaseException as ex:
            want = {"ret": {"__raise__": type(ex).__name__, "msg": str(ex)},
                    "trace": CUR["trace"]}
        if func in STATEFUL:
            want["state"] = norm(CUR["statewin"]) if CUR["statewin"] is not None else {}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE auth_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE auth_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()