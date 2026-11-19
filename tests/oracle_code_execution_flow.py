#!/usr/bin/env python3
"""Oracle for the execute_code FLOW + leaf port (src/tools/code_execution_flow.c).

The quarry has NO tools/code_execution.py: the surface is the
tools/code_execution_{tool,rpc,env}.py family. This oracle drives the LIVE defs
AST-extracted from those three real modules with SCRIPTED I/O: every foreign
sub-call (json.loads, time.*, env.execute, logger, strip_ansi, redact_code_output,
the retry callable) and every CLAIMED sibling (_truncate_stdout_text) is a
recording stub fed the same scripts the C harness feeds its seams. Unclaimed
siblings that are themselves ports here (_sandbox_tools_for, _sanitize_output_text,
_clean_output, _with_timeout_notice, _error_result, _execute_checked) are seamed
when composed and driven REAL by their own direct cases. Compares the DECISION
SEQUENCE (ordered seam calls {"name","args"} in "trace"), the return ("ret") and
the echoed mutable state ("state").

The heavy quarry import chain is never imported (module-top ImportFrom nodes are
not exec'd; in-function imports resolve per-case sys.modules stubs). Fail-closed:
missing defs or 0 cases => exit 2; a missing/exhausted seam script raises
RuntimeError on both sides. Catch BaseException (scripted SystemExit /
KeyboardInterrupt / GeneratorExit / CancelledError propagate through `except
Exception` sites).

Representation contract (shared with the C port header):
- Pure stdlib algorithms (shlex.quote, base64, compare_digest, str/repr/slice/sort/
  round) run REAL here; the C transcribes them. json.loads is seamed; json.dumps
  stays real (_error_result reference).
- Set returns (_sandbox_tools_for) -> sorted list; tuple returns (_clean_output)
  -> 2-element list. Integral floats normalize to ints; fixtures avoid integral-
  float literals in str/repr/dumps float positions (duration_seconds non-integral)
  and non-finite floats. ASCII scope; the U+23F0 glyph is a verbatim constant.
- Mutable inputs (_cache_probe_result cache, _apply_timeout result) ride state and
  are echoed even on a raise. A re.Match double is {"group1": str}; an exception
  input is {"__raise__":cls,"message":msg}.
Summary: 'ORACLE code_execution_flow: N cases, M mismatches'.
"""
import ast
import asyncio
import base64
import copy
import json
import math
import os
import re
import secrets
import shlex
import sys
import types
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "tools" / "code_execution_tool.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRCS = [
    str(QUARRY / "tools" / "code_execution_env.py"),
    str(QUARRY / "tools" / "code_execution_rpc.py"),
    str(QUARRY / "tools" / "code_execution_tool.py"),
]

PY = {
    "is_secret_env_name": "is_secret_env_name",
    "_cache_probe_result": "_cache_probe_result",
    "_sandbox_tools_for": "_sandbox_tools_for",
    "_missing_hermes_tools_import_hint": "_missing_hermes_tools_import_hint",
    "json_parse": "json_parse",
    "shell_quote": "shell_quote",
    "retry": "retry",
    "_sanitize_output_text": "_sanitize_output_text",
    "_clean_output": "_clean_output",
    "_with_timeout_notice": "_with_timeout_notice",
    "_error_result": "_error_result",
    "_remote_failure": "_remote_failure",
    "_remote_result": "_remote_result",
    "_apply_timeout": "_apply_timeout",
    "_private_dirs_cmd": "_private_dirs_cmd",
    "_execute_checked": "_execute_checked",
    "_remote_write": "_remote_write",
    "_rpc_token_ok": "_rpc_token_ok",
    "_result_error": "_result_error",
    "tool_errors_since": "tool_errors_since",
}
REQUIRED_CONSTS = ["SANDBOX_ALLOWED_TOOLS", "_SECRET_SUBSTRINGS", "_PROBE_CACHE_MAX"]

import __future__ as _fut
_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    exec(compile(seg(src, node), path, "exec", flags=_FLAGS, dont_inherit=True), ns)


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


def extract():
    ns = {
        "__name__": "tools.code_execution_tool",
        "re": re, "os": os, "sys": sys, "json": json, "math": math,
        "base64": base64, "shlex": shlex, "secrets": secrets,
        "logging": __import__("logging"), "threading": __import__("threading"),
        "time": __import__("time"), "types": types, "platform": __import__("platform"),
        "socket": __import__("socket"), "tempfile": __import__("tempfile"),
        "uuid": __import__("uuid"), "subprocess": __import__("subprocess"),
        "ast": ast,
        "Any": Any, "Dict": Dict, "List": List, "Optional": Optional, "Tuple": Tuple,
        "logger": _SilentLogger(),
        # neutralize the registration-time schema build (an Assign that CALLS a
        # quarry function with a heavy import chain) during constant seeding.
        "build_execute_code_schema": lambda *a, **k: {},
        "_load_config": lambda: {},
        "_get_execution_mode": lambda: "project",
        # imported collaborators (seamed per case; placeholders for constant exec)
        "tool_error": lambda *a, **k: "",
        "registry": types.SimpleNamespace(register=lambda *a, **k: None),
        "head_tail_split": lambda *a, **k: (0, 0),
        "truncation_notice": lambda *a, **k: "",
        "get_timezone_name": lambda: "",
        "propagate_context_to_thread": lambda f: f,
    }
    for path in SRCS:
        src = open(path, encoding="utf-8").read()
        tree = ast.parse(src)

        def consts():
            for node in tree.body:
                if isinstance(node, (ast.Assign, ast.AnnAssign)):
                    tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
                    if not all(isinstance(t, ast.Name) for t in tgts):
                        continue
                    try:
                        exec_node(src, node, ns, path)
                    except Exception:
                        pass

        consts()
        for node in tree.body:
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                try:
                    exec_node(src, node, ns, path)
                except Exception:
                    pass
        # logger = logging.getLogger(...) is an Assign that clobbers the seeded
        # silent logger; restore it (install_seams re-binds the recorder per case).
        ns["logger"] = _SilentLogger()
    # json_parse / shell_quote / retry are NOT top-level defs: they live inside the
    # _COMMON_HELPERS template string (embedded in the generated hermes_tools module).
    # Exec that string into the same namespace so their globals resolve the per-case
    # json/shlex/time doubles at call time.
    helpers_src = ns.get("_COMMON_HELPERS")
    if isinstance(helpers_src, str):
        try:
            exec(compile(helpers_src, "<common_helpers>", "exec", flags=_FLAGS, dont_inherit=True), ns)
        except Exception:
            pass
    missing = [t for t in PY.values() if t not in ns]
    for c in REQUIRED_CONSTS:
        if c not in ns:
            missing.append(c)
    if missing:
        print(f"FATAL oracle_code_execution_flow: missing defs/consts: {missing}")
        sys.exit(2)
    return ns


NS = extract()
REAL = {nm: NS[nm] for nm in set(PY.values()) if nm in NS}
CURRENT = {"ctx": None}

# ── per-case script backend (fail-closed presence) ─────────────────────────
EXC_CLASSES = {
    "OSError": OSError, "ValueError": ValueError, "TypeError": TypeError,
    "RuntimeError": RuntimeError, "KeyError": KeyError, "AttributeError": AttributeError,
    "Exception": Exception, "FileNotFoundError": FileNotFoundError,
    "UnicodeDecodeError": UnicodeDecodeError, "IsADirectoryError": IsADirectoryError,
    "IndexError": IndexError, "LookupError": LookupError, "NameError": NameError,
    "ArithmeticError": ArithmeticError, "ZeroDivisionError": ZeroDivisionError,
    "StopIteration": StopIteration, "AssertionError": AssertionError,
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
    "CancelledError": asyncio.CancelledError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    if kind == "SystemExit":
        return SystemExit(msg)
    if kind == "KeyboardInterrupt":
        return KeyboardInterrupt(msg)
    if kind == "GeneratorExit":
        return GeneratorExit(msg)
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    return cls(msg)


class Ctx:
    def __init__(self, inp):
        self.io = inp.get("io") or {}
        self.cursors = {}
        self.trace = []

    def next_entry(self, name):
        if name not in self.io:
            raise RuntimeError(f"no script for {name}")
        entry = self.io[name]
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                raise RuntimeError(f"script exhausted for {name}")
            self.cursors[name] = idx + 1
            return entry[idx]
        return entry

    def call(self, name, args):
        self.trace.append({"name": name, "args": args})
        entry = self.next_entry(name)
        if isinstance(entry, dict) and entry.get("__script__"):
            if isinstance(entry.get("raise"), dict):
                raise make_exc(entry["raise"])
            return entry.get("ret")
        return entry


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── input materialization doubles ──────────────────────────────────────────
class MatchDouble:
    def __init__(self, d):
        self._d = d or {}

    def group(self, n):
        return self._d.get("group1") if n == 1 else None


class EnvDouble:
    def __init__(self, ctx):
        self._ctx = ctx

    def execute(self, cmd, cwd=None, timeout=None, **kw):
        return self._ctx.call("env.execute", {"cmd": cmd, "cwd": cwd, "timeout": timeout, **kw})


def install_seams(ctx, skip):
    CURRENT["ctx"] = ctx
    ns = NS
    for nm, obj in REAL.items():
        ns[nm] = obj
    ns["base64"] = base64
    ns["shlex"] = shlex
    ns["secrets"] = secrets

    def json_loads_rec(text, **kw):
        return ctx.call("json_loads", {"text": text, "strict": kw.get("strict", True)})

    ns["json"] = types.SimpleNamespace(
        loads=json_loads_rec, dumps=json.dumps, JSONDecodeError=json.JSONDecodeError)
    ns["time"] = types.SimpleNamespace(
        monotonic=lambda: ctx.call("time.monotonic", {}),
        sleep=lambda seconds: ctx.call("time.sleep", {"seconds": seconds}))
    silent = _SilentLogger()
    ns["logger"] = types.SimpleNamespace(
        error=lambda fmt, *a, **k: ctx.call("logger.error", {
            "fmt": fmt,
            "args": [str(x) if isinstance(x, BaseException) else x for x in a],
            **k}),
        debug=silent.debug, info=silent.info, warning=silent.warning)

    # composed siblings (seamed unless being driven real)
    siblings = {
        "_sandbox_tools_for": lambda enabled_tools: ctx.call("sandbox_tools_for", {"enabled_tools": enabled_tools}),
        "_sanitize_output_text": lambda text: ctx.call("sanitize_output_text", {"text": text}),
        "_clean_output": lambda stdout_text, env=None: ctx.call("clean_output", {"stdout_text": stdout_text, "env": env}),
        "_with_timeout_notice": lambda stdout_text, timeout_msg: ctx.call("with_timeout_notice", {"stdout_text": stdout_text, "timeout_msg": timeout_msg}),
        "_error_result": lambda error, *, tool_calls_made=0, duration=0, user_summary=None: ctx.call("error_result", {"error": error, "tool_calls_made": tool_calls_made, "duration": duration, "user_summary": user_summary}),
        "_execute_checked": lambda env, cmd, what, *, timeout, **kw: ctx.call("execute_checked", {"cmd": cmd, "what": what, "timeout": timeout, **kw}),
        "_truncate_stdout_text": lambda stdout_text, env=None: ctx.call("truncate_stdout_text", {"stdout_text": stdout_text, "env": env}),
    }
    for nm, fn in siblings.items():
        if nm == skip:
            continue
        ns[nm] = fn

    # in-function imports: from tools.ansi_strip import strip_ansi /
    # from agent.redact import redact_code_output -> per-case sys.modules stubs.
    tools_mod = types.ModuleType("tools")
    tools_mod.__path__ = []
    ansi_mod = types.ModuleType("tools.ansi_strip")
    ansi_mod.strip_ansi = lambda text: ctx.call("strip_ansi", {"text": text})
    tools_mod.ansi_strip = ansi_mod
    sys.modules["tools"] = tools_mod
    sys.modules["tools.ansi_strip"] = ansi_mod
    agent_mod = types.ModuleType("agent")
    agent_mod.__path__ = []
    redact_mod = types.ModuleType("agent.redact")
    redact_mod.redact_code_output = lambda text: ctx.call("redact_code_output", {"text": text})
    agent_mod.redact = redact_mod
    sys.modules["agent"] = agent_mod
    sys.modules["agent.redact"] = redact_mod


def canon(v):
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, float):
        if math.isnan(v):
            return {"__float__": "nan"}
        if math.isinf(v):
            return {"__float__": "-inf" if v < 0 else "inf"}
        if v == int(v) and abs(v) < 1e16:
            return int(v)
        return v
    if isinstance(v, dict):
        return {k: canon(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [canon(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted(canon(x) for x in v)
    return v


def run_case(func, inp):
    ctx = Ctx(inp)
    install_seams(ctx, PY[func])
    ns = NS
    stwin = copy.deepcopy(inp.get("state") or {})
    out = {}
    ret = None
    try:
        if func == "is_secret_env_name":
            ret = ns[func](inp.get("name", ""))
        elif func == "_cache_probe_result":
            cache = stwin.setdefault("cache", {})
            ns[func](cache, inp.get("key", ""), inp.get("value"))
            ret = None
        elif func == "_sandbox_tools_for":
            ret = sorted(ns[func](inp.get("enabled_tools")))
        elif func == "_missing_hermes_tools_import_hint":
            ret = ns[func](MatchDouble(inp.get("m")), inp.get("enabled_tools"))
        elif func == "json_parse":
            ret = ns[func](inp.get("text"))
        elif func == "shell_quote":
            ret = ns[func](inp.get("s", ""))
        elif func == "retry":
            fn = lambda: ctx.call("fn", {})
            ret = ns[func](fn, max_attempts=inp.get("max_attempts", 3), delay=inp.get("delay", 2))
        elif func == "_sanitize_output_text":
            ret = ns[func](inp.get("text", ""))
        elif func == "_clean_output":
            ret = list(ns[func](inp.get("stdout_text", ""), inp.get("env")))
        elif func == "_with_timeout_notice":
            ret = ns[func](inp.get("stdout_text", ""), inp.get("timeout_msg", ""))
        elif func == "_error_result":
            ret = ns[func](inp.get("error", ""), tool_calls_made=inp.get("tool_calls_made", 0),
                           duration=inp.get("duration", 0), user_summary=inp.get("user_summary"))
        elif func == "_remote_failure":
            exc_spec = inp.get("exc") or {}
            exc = make_exc({"type": exc_spec.get("__raise__", "RuntimeError"),
                            "message": exc_spec.get("message", "")})
            ret = ns[func](exc, inp.get("exec_start", 0), inp.get("tool_calls_made", 0))
        elif func == "_remote_result":
            ret = ns[func](inp.get("status", ""), inp.get("raw_stdout", ""), inp.get("exec_start", 0),
                           inp.get("fields", {}), kernel=inp.get("kernel"), env=inp.get("env"))
        elif func == "_apply_timeout":
            result = stwin.setdefault("result", {})
            ns[func](result, inp.get("timeout_msg", ""))
            ret = None
        elif func == "_private_dirs_cmd":
            ret = ns[func](inp.get("root", ""), *inp.get("subdirs", []))
        elif func == "_execute_checked":
            env = EnvDouble(ctx)
            ret = ns[func](env, inp.get("cmd", ""), inp.get("what", ""),
                           timeout=inp.get("timeout", 0), **inp.get("kwargs", {}))
        elif func == "_remote_write":
            env = EnvDouble(ctx)
            ret = ns[func](env, inp.get("remote_path", ""), inp.get("content", ""),
                           atomic=inp.get("atomic", False), timeout=inp.get("timeout", 30),
                           check=inp.get("check", False))
        elif func == "_rpc_token_ok":
            ret = ns[func](inp.get("request", {}), inp.get("rpc_token", ""))
        elif func == "_result_error":
            ret = ns[func](inp.get("result"))
        elif func == "tool_errors_since":
            ret = ns[func](inp.get("tool_call_log", []), start=inp.get("start", 0))
        else:
            out["error"] = "unknown func"
            return out
    except BaseException as exc:  # noqa: BLE001 - raise sentinel parity
        ret = proj_exc(exc)
    out["ret"] = ret
    out["trace"] = ctx.trace
    out["state"] = stwin
    return out


def deep_eq(a, b):
    return canon(a) == canon(b)


def main():
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line or not line.startswith("{"):
            continue
        try:
            rec = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = rec.get("func")
        inp = rec.get("in", {}) or {}
        c_out = rec.get("out")
        cases += 1
        if func not in PY:
            mism += 1
            print(f"PYERR {func}: unknown func")
            continue
        try:
            py_out = run_case(func, inp)
        except BaseException as exc:  # noqa: BLE001
            mism += 1
            print(f"PYERR {func}: {type(exc).__name__}: {exc}")
            continue
        if c_out is None:
            mism += 1
            print(f"MISMATCH {func}: missing C out")
            continue
        bad = []
        for key, want in py_out.items():
            got = c_out.get(key)
            if not deep_eq(got, want):
                bad.append(key)
        if bad:
            mism += 1
            print(f"MISMATCH {func} keys={bad}")
            for key in bad:
                print(f"  C.{key}={json.dumps(canon(c_out.get(key)), default=str)[:600]}")
                print(f"  PY.{key}={json.dumps(canon(py_out.get(key)), default=str)[:600]}")
    if cases == 0:
        print("FATAL oracle_code_execution_flow: 0 cases")
        print("ORACLE code_execution_flow: 0 cases, 0 mismatches")
        return 2
    print(f"ORACLE code_execution_flow: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


def seam_names():
    return ["json_loads", "strip_ansi", "redact_code_output", "fn", "time.sleep",
            "time.monotonic", "env.execute", "logger.error", "execute_checked",
            "clean_output", "sanitize_output_text", "truncate_stdout_text",
            "sandbox_tools_for", "with_timeout_notice", "error_result"]


def self_test(fixpath):
    """Drive every fixture through run_case; flag drive failures and suspicious
    uniform AttributeError families naming a ported method/seam (binding bugs)."""
    import faulthandler
    faulthandler.dump_traceback_later(60, exit=True)
    names = set(PY.values()) | set(seam_names())
    seen_funcs = set()
    problems = []
    n = 0
    for line in open(fixpath, encoding="utf-8"):
        line = line.strip()
        if not line.startswith("{"):
            continue
        rec = json.loads(line)
        func = rec["func"]
        seen_funcs.add(func)
        n += 1
        try:
            out = run_case(func, rec.get("in") or {})
        except BaseException as exc:  # noqa: BLE001
            problems.append(f"DRIVE-FAIL {func}: {type(exc).__name__}: {exc}")
            continue
        ret = out.get("ret")
        if isinstance(ret, dict) and ret.get("__raise__") == "AttributeError":
            msg = ret.get("message", "")
            for nm in names:
                if f"'{nm}'" in msg:
                    problems.append(f"SUSPECT-ATTR {func}: {msg}")
                    break
    missing = set(PY) - seen_funcs
    if missing:
        problems.append(f"FIXTURE missing funcs: {sorted(missing)}")
    faulthandler.cancel_dump_traceback_later()
    print(f"SELF-TEST code_execution_flow: {n} cases, {len(problems)} problems")
    for p in problems[:40]:
        print("  " + p)
    return 0 if not problems else 1


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        fix = sys.argv[2] if len(sys.argv) > 2 else str(_HERE.parent / "fixtures" / "code_execution_flow.jsonl")
        sys.exit(self_test(fix))
    sys.exit(main())