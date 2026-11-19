#!/usr/bin/env python3
"""Oracle for the hermes_cli/config.py load/migration/save FLOW port
(src/hermes_cli/config_load_save_flow.c).

Drives the LIVE config.py load/save pipeline defs with SCRIPTED I/O: every
file/env/backend dependency is a recording stub fed the same scripts the C
harness feeds its seams. Compares the DECISION SEQUENCE (ordered seam names in
"trace"), the return ("ret"), the mutated module-global mirror ("state",
restricted to the keys the case seeded) and captured stderr messages
("stderr"). Pure siblings (_deep_merge, _split_key_path, _env_ref_var_name,
_is_non_env_secret_ref, _looks_structured_value, _omitted_config_paths,
_strip_dotted_keys, cfg_get, _known_top_level_keys, _env_expand_match) stay REAL
so nested decision logic is exercised, matching the C port's live transcriptions.

Live defs are AST-extracted (module Assign/AnnAssign + FunctionDef exec);
FailedConfigRead / _refuse_failed_read / _refuse_overwrite come from a wholesale
exec of config_read_errors.py. The heavy quarry import chain is never imported.
Fail-closed: 0 cases => exit 1. Summary:
'ORACLE config_load_save_flow: N cases, M mismatches'.

Representation contract (both sides):
- cache entries are lists (Python tuples); FailedConfigRead rides as
  {"__failed_read__": {"data": {...}, "error": {type,message,...}}}.
- an uncaught exception rides as {"__raise__": {type,message,code?,name?,...}}.
- paths reach traces/results as posix strings (Path.as_posix / FakePath).
"""
import ast
import copy
import json
import os
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "hermes_cli" / "config.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "hermes_cli" / "config.py")
RE_SRC = str(QUARRY / "hermes_cli" / "config_read_errors.py")
SS_SRC = str(QUARRY / "hermes_cli" / "config_set_scalars.py")

PY = {
    "worker_config_snapshot": "_worker_config_snapshot",
    "raw_config_cache_hit": "_raw_config_cache_hit",
    "raw_config_cache_store": "_raw_config_cache_store",
    "read_raw_config_impl": "_read_raw_config_impl",
    "load_config_cache_sig": "_load_config_cache_sig",
    "load_config_cache_hit": "_load_config_cache_hit",
    "last_known_good_fallback": "_last_known_good_fallback",
    "merge_managed_overlay": "_merge_managed_overlay",
    "write_config_state": "_write_config_state",
    "atomic_config_replace": "atomic_config_replace",
    "strip_managed_keys_for_save": "_strip_managed_keys_for_save",
    "seed_config_file": "seed_config_file",
    "env_ref_lookup": "_env_ref_lookup",
    "expand_env_vars": "_expand_env_vars",
    "validate_config_key": "_validate_config_key",
    "validate_schema_key": "_validate_schema_key",
    "is_wrong_prefix_suggestion": "_is_wrong_prefix_suggestion",
    "coerce_config_set_value": "_coerce_config_set_value",
}

OSERROR_FAMILY = {
    "OSError", "PermissionError", "FileNotFoundError", "FileExistsError",
    "BlockingIOError", "ChildProcessError", "ConnectionError", "BrokenPipeError",
    "ConnectionAbortedError", "ConnectionRefusedError", "ConnectionResetError",
    "InterruptedError", "IsADirectoryError", "NotADirectoryError",
    "ProcessLookupError", "TimeoutError",
}


class YAMLError(Exception):
    pass


def make_exc(spec):
    spec = spec or {}
    t = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    if t == "SystemExit":
        e = SystemExit(spec.get("code", 1))
    elif t == "ModuleNotFoundError":
        e = ModuleNotFoundError(msg)
        e.name = spec.get("name")
    elif t == "YAMLError":
        e = YAMLError(msg)
    elif t in OSERROR_FAMILY:
        cls = {"OSError": OSError, "PermissionError": PermissionError,
               "FileNotFoundError": FileNotFoundError, "FileExistsError": FileExistsError,
               "TimeoutError": TimeoutError}.get(t, OSError)
        e = cls(msg)
    elif t == "ValueError":
        e = ValueError(msg)
    elif t == "TypeError":
        e = TypeError(msg)
    elif t == "RuntimeError":
        e = RuntimeError(msg)
    else:
        e = Exception(msg)
    if "problem" in spec:
        e.problem = spec["problem"]
    if "problem_mark_line" in spec:
        e.problem_mark = types.SimpleNamespace(line=spec["problem_mark_line"])
    return e


def exc_spec(e):
    spec = {"type": type(e).__name__, "message": str(e)}
    if isinstance(e, SystemExit):
        spec["code"] = e.code
    if isinstance(e, ModuleNotFoundError):
        spec["name"] = e.name
    if getattr(e, "problem", None) is not None:
        spec["problem"] = e.problem
    mark = getattr(e, "problem_mark", None)
    if mark is not None and getattr(mark, "line", None) is not None:
        spec["problem_mark_line"] = mark.line
    return spec


class FakePath:
    """Posix path double for seed_config_file: exists()/mkdir() are scripted seams."""
    def __init__(self, s, ctx=None):
        self._s = str(s)
        self._ctx = ctx

    def __truediv__(self, other):
        base = self._s.rstrip("/") if self._s != "/" else ""
        return FakePath(base + "/" + str(other), self._ctx)

    @property
    def parent(self):
        s = self._s.rstrip("/")
        if "/" not in s:
            return FakePath(".", self._ctx)
        head = s.rsplit("/", 1)[0]
        return FakePath(head if head else "/", self._ctx)

    def exists(self):
        return CURRENT["ctx"].recorder("path_exists")(self)

    def mkdir(self, parents=False, exist_ok=False):
        return CURRENT["ctx"].recorder("mkdir_parents")(self)

    def __fspath__(self):
        return self._s

    def __str__(self):
        return self._s

    def __repr__(self):
        return f"FakePath({self._s!r})"


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.stderr = []
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

    def recorder(self, name):
        ctx = self

        def call(*args, **kwargs):
            ctx.trace.append(name)
            result = ctx.script_next(name)
            if isinstance(result, dict):
                eff = result.get("state")
                if isinstance(eff, dict):
                    ctx.state.update(eff)
                raise_spec = result.get("raise")
                if isinstance(raise_spec, dict):
                    raise make_exc(raise_spec)
                ret = result.get("ret")
            else:
                ret = result
            return convert_seam_ret(name, ret)
        return call


CURRENT = {"ctx": None}


def convert_seam_ret(name, ret):
    if name in ("config_cache_version", "file_cache_version"):
        return tuple(ret) if isinstance(ret, list) else ret
    if name == "get_managed_dir":
        return FakePath(ret, CURRENT["ctx"]) if isinstance(ret, str) else ret
    if name == "get_project_root":
        return FakePath(ret, CURRENT["ctx"]) if isinstance(ret, str) else ret
    return ret


def _import_guard_module(modname, script_name, call_names):
    """A stub module whose in-function import can be scripted to fail.

    __getattr__ PEEKS the script (no consume) for an "import_raise" entry; on a
    failed import it consumes + raises (the live import records nothing). On
    success it returns recording stubs that trace + consume at CALL time — one
    script consumption per seam, matching the C dispatcher.
    """
    m = types.ModuleType(modname)

    def getattr_(name):
        ctx = CURRENT["ctx"]
        entry = ctx.peek(script_name)
        if isinstance(entry, dict) and isinstance(entry.get("import_raise"), dict):
            ctx.consume(script_name)
            raise make_exc(entry["import_raise"])
        if name in call_names:
            return ctx.recorder(name)
        raise AttributeError(name)

    m.__getattr__ = getattr_
    return m


def install_stubs(ctx, inp):
    ns = NS
    rec = ctx.recorder

    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    agent = sys.modules.get("agent")
    if agent is None:
        agent = types.ModuleType("agent")
        agent.__path__ = []
        sys.modules["agent"] = agent
    hc = sys.modules.get("hermes_cli")
    if hc is None:
        hc = types.ModuleType("hermes_cli")
        hc.__path__ = []
        sys.modules["hermes_cli"] = hc

    sys.modules["agent.secret_scope"] = _import_guard_module(
        "agent.secret_scope", "current_secret_scope", {"current_secret_scope", "get_secret"})
    sys.modules["agent.safe_worker_policy"] = _import_guard_module(
        "agent.safe_worker_policy", "worker_config_snapshot", {"worker_config_snapshot"})
    mod("hermes_cli.config_backups", load_newest_good_backup=rec("load_newest_good_backup"))

    def apply_user_moa_presets(merged, raw):
        ctx.trace.append("apply_user_moa_presets")
        result = ctx.script_next("apply_user_moa_presets")
        if isinstance(result, dict):
            if isinstance(result.get("raise"), dict):
                raise make_exc(result["raise"])
            ret = result.get("ret")
            if isinstance(ret, dict):
                merged.clear()
                merged.update(copy.deepcopy(ret))
        return None
    mod("hermes_cli.moa_config", apply_user_moa_presets=apply_user_moa_presets)

    def keeping_read_base(read):
        ctx.trace.append("keeping_read_base")
        return read()
    mod("hermes_cli.config_backend", keeping_read_base=keeping_read_base)
    mod("hermes_cli.config_runtime_keys", is_runtime_read_key=rec("is_runtime_read_key"))
    mod("hermes_constants", display_hermes_home=rec("display_hermes_home"))

    ns["managed_scope"] = types.SimpleNamespace(
        get_managed_dir=rec("get_managed_dir"),
        load_managed_config=rec("load_managed_config"),
        managed_config_keys=rec("managed_config_keys"))
    ns["yaml"] = types.SimpleNamespace(safe_load=rec("yaml.safe_load"), YAMLError=YAMLError)
    ns["os"] = types.SimpleNamespace(environ=types.SimpleNamespace(get=rec("os.environ.get")))
    ns["shutil"] = types.SimpleNamespace(copy2=rec("copy2"))
    ns["logger"] = types.SimpleNamespace(warning=rec("logger.warning"),
                                         debug=rec("logger.debug"), info=rec("logger.info"))

    class LockDouble:
        def __enter__(self):
            ctx.trace.append("lock.acquire")
            return self

        def __exit__(self, *a):
            ctx.trace.append("lock.release")
            return False
    ns["_CONFIG_LOCK"] = LockDouble()

    def print_stub(*a, file=None, **k):
        ctx.trace.append("print_stderr")
        ctx.stderr.append(a[0] if a else "")
        return None
    ns["print"] = print_stub

    def exit_invalid(msg):
        ctx.trace.append("exit_invalid")
        ctx.stderr.append(msg)
        raise SystemExit(1)
    ns["_exit_invalid"] = exit_invalid

    for name in ("get_config_path", "get_project_root", "config_cache_version",
                 "file_cache_version", "read_config_doc", "probe_config_readable",
                 "write_config_document", "_warn_config_parse_failure",
                 "_canonicalize_config", "_normalize_root_model_keys",
                 "_suggest_closest_key", "_default_value_for_key",
                 "_coerce_int", "_coerce_float", "_quoted_bool",
                 "require_readable_config_before_write",
                 "refuse_redacted_config_credentials", "_secure_file", "save_config"):
        ns[name] = rec(name)

    ns["_refuse_failed_read"] = RE_NS["_refuse_failed_read"]
    ns["_refuse_overwrite"] = RE_NS["_refuse_overwrite"]
    ns["FailedConfigRead"] = RE_NS["FailedConfigRead"]


def rebuild(v):
    """JSON state -> live objects (FailedConfigRead markers, nested containers)."""
    if isinstance(v, dict):
        if "__failed_read__" in v:
            inner = v["__failed_read__"]
            return RE_NS["FailedConfigRead"](rebuild(inner.get("data", {})),
                                             error=make_exc(inner.get("error")))
        return {k: rebuild(x) for k, x in v.items()}
    if isinstance(v, list):
        return [rebuild(x) for x in v]
    return v


def seed_state(ctx, inp):
    ns = NS
    st = ctx.state
    for cache_key in ("_RAW_CONFIG_CACHE", "_LOAD_CONFIG_CACHE"):
        cache = st.get(cache_key)
        if isinstance(cache, dict):
            st[cache_key] = {k: tuple(rebuild(x) for x in v) for k, v in cache.items()}
        else:
            st[cache_key] = {}
        ns[cache_key] = st[cache_key]
    lkg = st.get("_LAST_EXPANDED_CONFIG_BY_PATH")
    st["_LAST_EXPANDED_CONFIG_BY_PATH"] = {k: rebuild(v) for k, v in lkg.items()} if isinstance(lkg, dict) else {}
    ns["_LAST_EXPANDED_CONFIG_BY_PATH"] = st["_LAST_EXPANDED_CONFIG_BY_PATH"]
    if not isinstance(st.get("_CONFIG_PARSE_FAILURES"), dict):
        st["_CONFIG_PARSE_FAILURES"] = {}
    ns["_CONFIG_PARSE_FAILURES"] = st["_CONFIG_PARSE_FAILURES"]
    if not isinstance(st.get("DEFAULT_CONFIG"), dict):
        st["DEFAULT_CONFIG"] = {}
    ns["DEFAULT_CONFIG"] = st["DEFAULT_CONFIG"]
    ns["_PLATFORM_CONTAINER_KEYS"] = frozenset(st.get("_PLATFORM_CONTAINER_KEYS", ["platforms"]))
    ns["_OPEN_SUBKEY_TOP_LEVEL_KEYS"] = frozenset(st.get("_OPEN_SUBKEY_TOP_LEVEL_KEYS", []))
    ns["_EXTRA_KNOWN_ROOT_KEYS"] = set(st.get("_EXTRA_KNOWN_ROOT_KEYS", []))
    ns["LEFT_CORE_TERMINAL_KEYS"] = {k: "" for k in st.get("LEFT_CORE_TERMINAL_KEYS", [])}


def proj(v):
    if isinstance(v, RE_NS["FailedConfigRead"]):
        return {"__failed_read__": {"data": proj(dict(v)), "error": exc_spec(v.read_error)}}
    if isinstance(v, dict):
        return {k: proj(x) for k, x in v.items()}
    if isinstance(v, (tuple, list)):
        return [proj(x) for x in v]
    if isinstance(v, Path):
        return v.as_posix()
    if isinstance(v, FakePath):
        return str(v)
    if isinstance(v, (set, frozenset)):
        return sorted(v)
    return v


def extract():
    import re as _re
    import copy as _copy
    import logging as _logging
    import threading as _threading
    import platform as _platform
    import difflib as _difflib
    import shlex as _shlex
    import shutil as _shutil
    import stat as _stat
    import subprocess as _subprocess
    import tempfile as _tempfile
    import contextlib as _contextlib
    from dataclasses import dataclass, field
    from typing import Dict, Any, Literal, Optional, List, Tuple, Set
    ns = {
        "__name__": "hermes_cli.config",
        "re": _re, "os": os, "sys": sys, "json": json, "copy": _copy,
        "logging": _logging, "logger": _logging.getLogger("hermes_cli.config"),
        "threading": _threading, "platform": _platform, "difflib": _difflib,
        "shlex": _shlex, "shutil": _shutil, "stat": _stat,
        "subprocess": _subprocess, "tempfile": _tempfile, "contextlib": _contextlib,
        "dataclass": dataclass, "field": field, "Path": Path,
        "Dict": Dict, "Any": Any, "Literal": Literal, "Optional": Optional,
        "List": List, "Tuple": Tuple, "Set": Set,
    }
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
            if not isinstance(tgt, ast.Name):
                continue
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    missing = [v for v in PY.values() if v not in ns]
    for sib in ("_deep_merge", "_split_key_path", "_env_expand_match", "_env_ref_var_name",
                "_is_non_env_secret_ref", "_looks_structured_value", "_omitted_config_paths",
                "_strip_dotted_keys", "cfg_get", "_known_top_level_keys"):
        if sib not in ns:
            missing.append(sib)
    if missing:
        print("FATAL config_load_save_flow: extraction missing names:", sorted(set(missing)))
        sys.exit(2)
    return ns


def extract_read_errors():
    import logging as _logging
    from typing import Any, Optional
    ns = {
        "__name__": "hermes_cli.config_read_errors",
        "logging": _logging, "logger": _logging.getLogger("hermes_cli.config_read_errors"),
        "sys": sys, "Path": Path, "Any": Any, "Optional": Optional,
    }
    src = open(RE_SRC, encoding="utf-8").read()
    exec(compile(src, RE_SRC, "exec"), ns)
    return ns


def extract_scalar_words():
    from decimal import Decimal, InvalidOperation
    from typing import Optional
    ns = {"__name__": "hermes_cli.config_set_scalars",
          "Decimal": Decimal, "InvalidOperation": InvalidOperation, "Optional": Optional}
    src = open(SS_SRC, encoding="utf-8").read()
    exec(compile(src, SS_SRC, "exec"), ns)
    return ns["_SCALAR_WORDS"]


RE_NS = extract_read_errors()
NS = extract()
NS["_SCALAR_WORDS"] = extract_scalar_words()


def run_case(func, inp):
    ctx = CaseCtx(inp)
    CURRENT["ctx"] = ctx
    install_stubs(ctx, inp)
    seed_state(ctx, inp)
    ns = NS
    out = {"ret": None, "trace": ctx.trace}
    try:
        if func == "worker_config_snapshot":
            out["ret"] = ns["_worker_config_snapshot"]()
        elif func == "raw_config_cache_hit":
            out["ret"] = ns["_raw_config_cache_hit"](inp.get("path_key", ""), tuple(inp.get("cache_key", [])))
        elif func == "raw_config_cache_store":
            ns["_raw_config_cache_store"](inp.get("path_key", ""), tuple(inp.get("cache_key", [])),
                                          copy.deepcopy(inp.get("data")))
            out["ret"] = None
        elif func == "read_raw_config_impl":
            out["ret"] = ns["_read_raw_config_impl"](want_deepcopy=inp.get("want_deepcopy", True))
        elif func == "load_config_cache_sig":
            out["ret"] = ns["_load_config_cache_sig"](FakePath(inp.get("config_path", ""), ctx))
        elif func == "load_config_cache_hit":
            cs = inp.get("cache_sig")
            out["ret"] = ns["_load_config_cache_hit"](inp.get("path_key", ""), tuple(cs) if cs is not None else None)
        elif func == "last_known_good_fallback":
            cs = inp.get("cache_sig")
            out["ret"] = ns["_last_known_good_fallback"](
                FakePath(inp.get("config_path", ""), ctx), inp.get("path_key", ""),
                tuple(cs) if cs is not None else None, make_exc(inp.get("exc")))
        elif func == "merge_managed_overlay":
            out["ret"] = ns["_merge_managed_overlay"](copy.deepcopy(inp.get("expanded")))
        elif func == "write_config_state":
            out["ret"] = ns["_write_config_state"](
                FakePath(inp.get("config_path", ""), ctx), rebuild(inp.get("data")),
                allow_omissions=inp.get("allow_omissions", False),
                extra_content_on_create=inp.get("extra_content_on_create"))
        elif func == "atomic_config_replace":
            out["ret"] = ns["atomic_config_replace"](
                FakePath(inp.get("config_path", ""), ctx), rebuild(inp.get("data")),
                extra_content_on_create=inp.get("extra_content_on_create"))
        elif func == "strip_managed_keys_for_save":
            out["ret"] = ns["_strip_managed_keys_for_save"](copy.deepcopy(inp.get("config")))
        elif func == "seed_config_file":
            tmpl = inp.get("template")
            out["ret"] = ns["seed_config_file"](FakePath(inp.get("config_path", ""), ctx),
                                                FakePath(tmpl, ctx) if tmpl else None)
        elif func == "env_ref_lookup":
            out["ret"] = ns["_env_ref_lookup"](inp.get("name", ""))
        elif func == "expand_env_vars":
            out["ret"] = ns["_expand_env_vars"](copy.deepcopy(inp.get("obj")))
        elif func == "validate_config_key":
            out["ret"] = ns["_validate_config_key"](inp.get("key", ""))
        elif func == "validate_schema_key":
            out["ret"] = ns["_validate_schema_key"](inp.get("key", ""))
        elif func == "is_wrong_prefix_suggestion":
            out["ret"] = ns["_is_wrong_prefix_suggestion"](inp.get("key", ""), inp.get("suggestion"))
        elif func == "coerce_config_set_value":
            out["ret"] = ns["_coerce_config_set_value"](inp.get("key", ""), inp.get("value", ""))
        else:
            raise KeyError(func)
    except Exception as e:   # includes SystemExit
        out["ret"] = {"__raise__": exc_spec(e)}
    out["ret"] = proj(out["ret"])
    out["state"] = {k: proj(ctx.state[k]) for k in (inp.get("state") or {}) if k in ctx.state}
    out["stderr"] = ctx.stderr
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
        if func not in PY:
            mism += 1
            print(f"PYERR {func}: unknown func")
            continue
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
              and got.get("stderr") == want.get("stderr"))
        if not ok:
            mism += 1
            print(f"MISMATCH {func}")
            print(f"  C  ={json.dumps(got, sort_keys=True, default=str)}")
            print(f"  PY ={json.dumps(want, sort_keys=True, default=str)}")
    print(f"ORACLE config_load_save_flow: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
