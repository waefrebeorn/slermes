#!/usr/bin/env python3
"""Oracle for web_server_flow5 — the FINAL UNCLAIMED web-server FLOW surface of
hermes_cli/web_server.py (LIVE authority, py3.14 quarry venv).

Port covered: _dashboard_selftest_loop (L615), the async self-test driver that
opens the lifespan's periodic probe. It is the only one of web_server.py's 50
defs with no PoP annotation (CRLF-aware tree-wide grep); its sibling
_dashboard_selftest_once is CLAIMED (port__ watercup gate) and is SEAMED here.

Drives the LIVE function with SCRIPTED I/O: the def is AST-extracted from the
quarry (no heavy import), module constants seeded, every effect replaced by a
recording stub returning the SAME scripted results the C harness feeds its
universal seam. The live `while True` is terminated by scripting the
asyncio_sleep seam to raise a control-flow exception (_LoopBreak) on a chosen
call index; that raise propagates out of asyncio.run exactly as in live Python.
Compares the ordered DECISION SEQUENCE (trace of seam calls) plus the return
value (raise envelopes included). Scripted {"__raise__": cls} raises a REAL
exception; drive() catches BaseException so KeyboardInterrupt/SystemExit pass
through live `except ImportError` exactly as the C port propagates them. A
MISSING seam key fail-closes (RuntimeError) on both sides; a scripted null is
a valid None.

Input: stdin JSON lines from t_port_web_server_flow5:
    {"func": ..., "in": {...}, "out": {"ret":..., "trace":[...]}}
Summary: ORACLE web_server_flow5: N cases, M mismatches. Fail-closed (exit 2)
when extraction misses the def or zero cases were compared.
--self-test: drive every fixture case (no C) and report binding/script gaps.
"""
import ast
import asyncio
import builtins
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
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "web_server_flow5_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "web_server.py")

PY = {"dashboard_selftest_loop": "_dashboard_selftest_loop"}
WANTED = {"_dashboard_selftest_loop"}

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}}


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


class _LoopBreak(Exception):
    """Control-flow signal used by fixtures to terminate the live while True."""


# Not a CPython builtin (lives in zipimport); provide a safe fallback so the
# full-hierarchy table imports cleanly on every interpreter.
ZipImportError = getattr(builtins, "ZipImportError",
                         type("ZipImportError", (ImportError,), {}))


# FULL CPython 3.14 builtin exception hierarchy (identity matters for catches).
EXC_MAP = {
    "BaseException": BaseException, "Exception": Exception,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit,
    "StopIteration": StopIteration, "StopAsyncIteration": StopAsyncIteration,
    "ArithmeticError": ArithmeticError, "AssertionError": AssertionError,
    "AttributeError": AttributeError, "BufferError": BufferError, "EOFError": EOFError,
    "ImportError": ImportError, "LookupError": LookupError, "MemoryError": MemoryError,
    "NameError": NameError, "OSError": OSError, "ReferenceError": ReferenceError,
    "RuntimeError": RuntimeError, "SyntaxError": SyntaxError, "SystemError": SystemError,
    "TypeError": TypeError, "ValueError": ValueError, "Warning": Warning,
    "FloatingPointError": FloatingPointError, "OverflowError": OverflowError,
    "ZeroDivisionError": ZeroDivisionError,
    "ModuleNotFoundError": ModuleNotFoundError, "ZipImportError": ZipImportError,
    "IndexError": IndexError, "KeyError": KeyError,
    "BlockingIOError": BlockingIOError, "ChildProcessError": ChildProcessError,
    "ConnectionError": ConnectionError, "FileExistsError": FileExistsError,
    "FileNotFoundError": FileNotFoundError, "InterruptedError": InterruptedError,
    "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
    "PermissionError": PermissionError, "ProcessLookupError": ProcessLookupError,
    "TimeoutError": TimeoutError, "BrokenPipeError": BrokenPipeError,
    "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError,
    "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
    "PythonFinalizationError": PythonFinalizationError,
    "IndentationError": IndentationError, "TabError": TabError,
    "_IncompleteInputError": _IncompleteInputError,
    "UnicodeError": UnicodeError, "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeTranslateError": UnicodeTranslateError,
    "JSONDecodeError": json.JSONDecodeError,
    "UserWarning": UserWarning, "DeprecationWarning": DeprecationWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning, "SyntaxWarning": SyntaxWarning,
    "RuntimeWarning": RuntimeWarning, "FutureWarning": FutureWarning,
    "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning,
    "BytesWarning": BytesWarning, "EncodingWarning": EncodingWarning,
    "ResourceWarning": ResourceWarning,
    "UnboundLocalError": UnboundLocalError,
    "BaseExceptionGroup": BaseExceptionGroup, "ExceptionGroup": ExceptionGroup,
    "_LoopBreak": _LoopBreak,
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


class RecLogger:
    def debug(self, *a, **k): pass
    def info(self, *a, **k): pass
    def warning(self, *a, **k): pass
    def error(self, *a, **k): pass
    def exception(self, *a, **k): pass


class AsyncioDouble:
    @staticmethod
    async def sleep(seconds):
        return seam("asyncio_sleep", {"seconds": seconds})


def norm(o):
    if o is None or isinstance(o, (bool, int, str, float)):
        return o
    if isinstance(o, RecLogger):
        return {"__logger__": "_log"}
    if isinstance(o, types.SimpleNamespace):
        return {k: norm(v) for k, v in vars(o).items()}
    if isinstance(o, (frozenset, set)):
        return sorted(norm(v) for v in o)
    if callable(o):
        return {"__callable__": getattr(o, "__ws5_callable__", getattr(o, "__name__", True))}
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    return str(o)


def _seed_constant(ns, name):
    tree = ast.parse(open(SRC, encoding="utf-8").read())
    for node in tree.body:
        tgt = None
        if isinstance(node, ast.Assign) and len(node.targets) == 1 and isinstance(node.targets[0], ast.Name):
            tgt = node.targets[0].id
        elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
            tgt = node.target.id
        if tgt == name:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            return True
    return False


def extract():
    import logging
    import re
    import time
    from typing import Any, Dict, Optional, Tuple

    src = open(SRC, encoding="utf-8").read()
    ns = {
        "__name__": "hermes_cli.web_server_extracted5",
        "json": json, "os": os, "re": re, "sys": sys, "time": time,
        "logging": logging,
        "Any": Any, "Dict": Dict, "Optional": Optional, "Tuple": Tuple,
        "_log": RecLogger(),
    }
    tree = ast.parse(src)
    if not _seed_constant(ns, "_DASHBOARD_SELFTEST_INTERVAL_SECONDS"):
        print("FATAL missing constant: _DASHBOARD_SELFTEST_INTERVAL_SECONDS")
        sys.exit(2)
    top = {n.name: n for n in tree.body
           if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef))}
    for name in WANTED:
        if name not in top:
            print("FATAL missing top def: %s" % name)
            sys.exit(2)
        n = copy.copy(top[name])
        n.decorator_list = []
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    if not WANTED.issubset(set(ns)):
        print("FATAL missing defs: %s" % sorted(WANTED - set(ns)))
        sys.exit(2)
    global NS
    NS = ns
    return ns


_REAL_IMPORT = builtins.__import__


def install_stubs(ns):
    ns["_log"] = RecLogger()
    ns["asyncio"] = AsyncioDouble()

    async def _dashboard_selftest_once():
        return seam("dashboard_selftest_once", {})
    _dashboard_selftest_once.__ws5_callable__ = "_dashboard_selftest_once"
    ns["_dashboard_selftest_once"] = _dashboard_selftest_once

    def _fake_import(name, globals=None, locals=None, fromlist=(), level=0):
        if name == "httpx" and not level:
            r = seam("import_httpx", {})
            if r is None:
                return types.SimpleNamespace(__name__="httpx")
            if isinstance(r, dict):
                return types.SimpleNamespace(**r)
            return r
        return _REAL_IMPORT(name, globals, locals, fromlist, level)
    builtins.__import__ = _fake_import


def _app_double(inp):
    return types.SimpleNamespace(
        state=types.SimpleNamespace(**(inp.get("app_state") or {})))


def drive(ns, func, inp):
    if func not in PY:
        print("FATAL unknown func: %s" % func)
        sys.exit(2)
    ns["app"] = _app_double(inp)
    ns["_DASHBOARD_SELFTEST_INTERVAL_SECONDS"] = 60.0
    return asyncio.run(ns[PY[func]]())


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


SEAM_NAMES = ["import_httpx", "asyncio_sleep", "dashboard_selftest_once"]


def self_test(ns):
    import faulthandler
    faulthandler.dump_traceback_later(60, exit=True)
    fixture = _REPO / "tests" / "fixtures" / "web_server_flow5.jsonl"
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
    names = set(PY.values()) | set(PY) | set(SEAM_NAMES)
    for c in cases:
        CUR["in"] = c["in"]
        CUR["trace"] = []
        CUR["scripts"] = c["in"].get("seams", {})
        CUR["idx"] = {}
        try:
            drive(ns, c["func"], c["in"])
        except BaseException as ex:
            spec = "%s: %s" % (type(ex).__name__, ex)
            if type(ex).__name__ == "AttributeError" and any(("'%s'" % n) in str(ex) for n in names):
                print("SELFTEST BINDING %s: %s" % (c["func"], spec))
                bad += 1
            elif type(ex).__name__ == "NameError":
                print("SELFTEST NAMEERROR %s: %s" % (c["func"], spec))
                bad += 1
    faulthandler.cancel_dump_traceback_later()
    print("SELFTEST %d cases, %d binding/name errors" % (len(cases), bad))
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
        try:
            ret = drive(ns, func, inp)
            want = {"ret": norm(ret), "trace": norm(CUR["trace"])}
        except BaseException as ex:
            er = {"__raise__": type(ex).__name__, "msg": str(ex)}
            if isinstance(ex, SystemExit):
                er["code"] = ex.code
            want = {"ret": er, "trace": norm(CUR["trace"])}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE web_server_flow5: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE web_server_flow5: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
