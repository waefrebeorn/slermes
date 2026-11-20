#!/usr/bin/env python3
"""Oracle for the tools/terminal_tool.py final UNCLAIMED-remainder FLOW core
(src/tools/terminal_tool_flow4.c).

A CRLF-aware PoP scan leaves exactly one tools/terminal_tool.py def unclaimed:
_Rejected.__init__ (the _SessionScope props and _docker_session_isolation_enabled
are claimed by flow3 with qualified PoP names). This oracle AST-extracts the
LIVE _Rejected class and projects its construction + full-builtin-hierarchy
isinstance + ordered except-clause dispatch, and diffs three ways:

  shadow-mirror (literal Python reimplementation of the C algorithm)  == live
  live drive                                                          == C harness

The full builtin exception hierarchy (py3.14 builtins + the _Rejected ->
Exception edge) is encoded on both the C side (TT4_EXC_PARENTS) and the shadow
parent table. Unknown kind names default to a fresh Exception subclass and are
never in _Rejected's MRO, matching C's default-parent walk.

Summary: ORACLE terminal_tool_flow4: N cases, M mismatches, shadow-diff K.
Fail-closed on 0 cases or any shadow/ oracle mismatch.
"""
import ast
import builtins
import inspect
import json
import os
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"

# Full builtin exception hierarchy (direct builtin base). _Rejected -> Exception.
PARENTS = {
    "SystemExit": "BaseException", "KeyboardInterrupt": "BaseException",
    "GeneratorExit": "BaseException", "CancelledError": "BaseException",
    "BaseExceptionGroup": "BaseException", "Exception": "BaseException",
    "ExceptionGroup": "BaseExceptionGroup", "TypeError": "Exception",
    "ValueError": "Exception", "AttributeError": "Exception", "NameError": "Exception",
    "LookupError": "Exception", "ArithmeticError": "Exception", "OSError": "Exception",
    "EnvironmentError": "OSError", "IOError": "OSError", "WindowsError": "OSError",
    "StopIteration": "Exception", "StopAsyncIteration": "Exception",
    "AssertionError": "Exception", "SyntaxError": "Exception", "SystemError": "Exception",
    "ImportError": "Exception", "Warning": "Exception", "RuntimeError": "Exception",
    "MemoryError": "Exception", "BufferError": "Exception", "EOFError": "Exception",
    "ReferenceError": "Exception", "_IncompleteInputError": "SyntaxError",
    "PythonFinalizationError": "RuntimeError", "_Rejected": "Exception",
    "NotImplementedError": "RuntimeError", "RecursionError": "RuntimeError",
    "UnboundLocalError": "NameError", "UnicodeError": "ValueError",
    "UnicodeEncodeError": "UnicodeError", "UnicodeDecodeError": "UnicodeError",
    "UnicodeTranslateError": "UnicodeError", "IndexError": "LookupError",
    "KeyError": "LookupError", "ZeroDivisionError": "ArithmeticError",
    "OverflowError": "ArithmeticError", "FloatingPointError": "ArithmeticError",
    "FileNotFoundError": "OSError", "PermissionError": "OSError",
    "IsADirectoryError": "OSError", "NotADirectoryError": "OSError",
    "InterruptedError": "OSError", "TimeoutError": "OSError", "ConnectionError": "OSError",
    "ProcessLookupError": "OSError", "FileExistsError": "OSError",
    "ChildProcessError": "OSError", "BlockingIOError": "OSError",
    "BrokenPipeError": "ConnectionError", "ConnectionAbortedError": "ConnectionError",
    "ConnectionRefusedError": "ConnectionError", "ConnectionResetError": "ConnectionError",
    "ModuleNotFoundError": "ImportError", "IndentationError": "SyntaxError",
    "TabError": "IndentationError", "UserWarning": "Warning",
    "DeprecationWarning": "Warning", "PendingDeprecationWarning": "Warning",
    "SyntaxWarning": "Warning", "RuntimeWarning": "Warning", "FutureWarning": "Warning",
    "ImportWarning": "Warning", "UnicodeWarning": "Warning", "BytesWarning": "Warning",
    "ResourceWarning": "Warning", "EncodingWarning": "Warning",
}


def extract_rejected():
    """AST-extract the LIVE _Rejected class; no heavy tools.terminal_tool import."""
    path = os.path.join(QUARRY, "tools", "terminal_tool.py")
    src = open(path, encoding="utf-8").read()
    ns = {"__name__": "tt4_extracted", "__file__": path}
    flags = __future_flags()
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "_Rejected":
            exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec", flags=flags), ns)
    if "_Rejected" not in ns:
        raise RuntimeError("extraction missing: _Rejected")
    return ns


def __future_flags():
    import __future__
    return __future__.annotations.compiler_flag


NS = extract_rejected()
REJ = NS["_Rejected"]


def resolve_exc(name):
    """Live class object for an except-clause name; unknown -> fresh Exception subclass."""
    if name == "_Rejected":
        return REJ
    obj = getattr(builtins, name, None)
    if inspect.isclass(obj) and issubclass(obj, BaseException):
        return obj
    return type(name, (Exception,), {})   # not in _Rejected's MRO -> isinstance False


def e_of(rj):
    """Construct a live _Rejected exactly as the real __init__ does."""
    return REJ(rj)


# ── LIVE drive ──
def live_rejected_init(rj):
    e = e_of(rj)
    return {"args": list(e.args), "str": str(e), "result_json": e.result_json}


def live_drive(func, inp):
    rj = inp.get("result_json", "")
    if func == "rejected_init":
        return {"ret": live_rejected_init(rj), "trace": []}
    if func == "isinstance":
        return {"ret": bool(isinstance(e_of(rj), resolve_exc(inp.get("kind", "")))), "trace": []}
    if func == "hierarchy":
        e = e_of(rj)
        return {"ret": {n: bool(isinstance(e, resolve_exc(n))) for n in (inp.get("names") or [])},
                "trace": []}
    if func == "catch_dispatch":
        e = e_of(rj)
        trace = []
        caught = None
        for name in (inp.get("clauses") or []):
            matched = bool(isinstance(e, resolve_exc(name)))
            trace.append({"name": name, "matched": matched})
            if matched and caught is None:
                caught = name
        return {"ret": {"caught_by": caught}, "trace": trace}
    raise ValueError("unknown func: %s" % func)


# ── SHADOW mirror (literal reimplementation of the C tt4_* algorithm) ──
def shadow_parent(kind):
    if kind in PARENTS:
        return PARENTS[kind]
    if kind == "BaseException":
        return None
    return "Exception"   # tt4_exc_parent: unknown -> Exception subclass


def shadow_exc_is(kind, base):
    k = kind
    for _ in range(32):
        if k is None:
            return False
        if k == base:
            return True
        k = shadow_parent(k)
    return False


def shadow_isinstance(name):
    return shadow_exc_is("_Rejected", name)


def shadow_drive(func, inp):
    rj = inp.get("result_json", "")
    if func == "rejected_init":
        return {"ret": {"args": [rj], "str": rj, "result_json": rj}, "trace": []}
    if func == "isinstance":
        return {"ret": bool(shadow_isinstance(inp.get("kind", ""))), "trace": []}
    if func == "hierarchy":
        return {"ret": {n: bool(shadow_isinstance(n)) for n in (inp.get("names") or [])},
                "trace": []}
    if func == "catch_dispatch":
        trace = []
        caught = None
        for name in (inp.get("clauses") or []):
            matched = bool(shadow_isinstance(name))
            trace.append({"name": name, "matched": matched})
            if matched and caught is None:
                caught = name
        return {"ret": {"caught_by": caught}, "trace": trace}
    raise ValueError("unknown func: %s" % func)


def canon(x):
    if isinstance(x, dict):
        return ("obj", sorted(((k, canon(v)) for k, v in x.items()), key=lambda kv: kv[0]))
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", str(x))


def main():
    cases = 0
    mism = 0
    shadowdiff = 0
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
        got_cmp = {"ret": got.get("ret"), "trace": got.get("trace") or []}
        cases += 1
        try:
            want = live_drive(func, inp)
            shadow = shadow_drive(func, inp)
        except BaseException as e:
            print("DRIVE-ERROR %s: %s: %s" % (func, type(e).__name__, e))
            mism += 1
            continue
        if canon(shadow) != canon(want):
            shadowdiff += 1
            print("SHADOW-DIFF %s" % func)
            print("  C-mirror: %s" % json.dumps(shadow, ensure_ascii=False))
            print("  LIVE    : %s" % json.dumps(want, ensure_ascii=False))
        if canon(want) != canon(got_cmp):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got_cmp, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    print("ORACLE terminal_tool_flow4: %d cases, %d mismatches, shadow-diff %d"
          % (cases, mism, shadowdiff))
    return 0 if (mism == 0 and shadowdiff == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
