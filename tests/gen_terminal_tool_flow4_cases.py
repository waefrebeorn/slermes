#!/usr/bin/env python3
"""Generate tests/fixtures/terminal_tool_flow4.jsonl (terminal_tool_flow4 oracle).

Each case is {"func": <name>, "in": {...}}. >=5 cases per function, covering
the _Rejected.__init__ projection, the full builtin exception hierarchy, and
the ordered except-clause dispatch flow (including BaseException passthrough).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "terminal_tool_flow4.jsonl")
CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


# The exact builtin-name universe encoded in TT4_EXC_PARENTS + _Rejected.
ALL_NAMES = [
    "BaseException", "Exception", "SystemExit", "KeyboardInterrupt", "GeneratorExit",
    "CancelledError", "BaseExceptionGroup", "ExceptionGroup", "TypeError", "ValueError",
    "AttributeError", "NameError", "LookupError", "ArithmeticError", "OSError",
    "EnvironmentError", "IOError", "WindowsError", "StopIteration", "StopAsyncIteration",
    "AssertionError", "SyntaxError", "SystemError", "ImportError", "Warning", "RuntimeError",
    "MemoryError", "BufferError", "EOFError", "ReferenceError", "_IncompleteInputError",
    "PythonFinalizationError", "NotImplementedError", "RecursionError", "UnboundLocalError",
    "UnicodeError", "UnicodeEncodeError", "UnicodeDecodeError", "UnicodeTranslateError",
    "IndexError", "KeyError", "ZeroDivisionError", "OverflowError", "FloatingPointError",
    "FileNotFoundError", "PermissionError", "IsADirectoryError", "NotADirectoryError",
    "InterruptedError", "TimeoutError", "ConnectionError", "ProcessLookupError",
    "FileExistsError", "ChildProcessError", "BlockingIOError", "BrokenPipeError",
    "ConnectionAbortedError", "ConnectionRefusedError", "ConnectionResetError",
    "ModuleNotFoundError", "IndentationError", "TabError", "UserWarning", "DeprecationWarning",
    "PendingDeprecationWarning", "SyntaxWarning", "RuntimeWarning", "FutureWarning",
    "ImportWarning", "UnicodeWarning", "BytesWarning", "ResourceWarning", "EncodingWarning",
    "_Rejected",
]

# ── rejected_init (the direct _Rejected.__init__ port) ──
add("rejected_init", result_json="")
add("rejected_init", result_json='{"output": "ok", "exit_code": 0, "status": "success"}')
add("rejected_init", result_json='{"error": "denied: command not allowed"}')
add("rejected_init", result_json='quote " and backslash \\ inside')
add("rejected_init", result_json="unicode: café ☃ 中文")
add("rejected_init", result_json="line1\nline2\ttab")
add("rejected_init", result_json="x" * 200)

# ── isinstance (full hierarchy walk) ──
for kind in ("Exception", "BaseException", "_Rejected", "ValueError", "OSError",
             "FileNotFoundError", "TypeError", "KeyboardInterrupt", "SystemExit",
             "GeneratorExit", "ExceptionGroup", "BaseExceptionGroup", "CancelledError",
             "Warning", "UnicodeEncodeError", "NotImplementedError", "UnboundLocalError",
             "WindowsError", "EnvironmentError", "IOError", "__NotAnExc__"):
    add("isinstance", result_json="rj", kind=kind)

# ── hierarchy (projection over a caller-supplied name list) ──
add("hierarchy", result_json="rj", names=ALL_NAMES)
add("hierarchy", result_json="rj", names=["BaseException", "Exception", "_Rejected", "ValueError", "OSError"])
add("hierarchy", result_json="rj", names=["SystemExit", "KeyboardInterrupt", "GeneratorExit", "CancelledError", "BaseExceptionGroup"])
add("hierarchy", result_json="rj", names=["FileNotFoundError", "PermissionError", "ConnectionError", "BrokenPipeError", "TabError"])
add("hierarchy", result_json="rj", names=["Warning", "UserWarning", "DeprecationWarning", "EncodingWarning", "ResourceWarning"])
add("hierarchy", result_json="rj", names=["__NotAnExc__", "Exception", "BaseException", "ValueError", "_Rejected"])
add("hierarchy", result_json="rj", names=[])

# ── catch_dispatch (ordered except-clause flow) ──
add("catch_dispatch", result_json='{"error":"denied"}', clauses=["Exception"])
add("catch_dispatch", result_json='{"error":"denied"}', clauses=["ValueError", "Exception", "BaseException"])
add("catch_dispatch", result_json='{"error":"denied"}', clauses=["ValueError", "TypeError", "OSError"])
add("catch_dispatch", result_json='{"error":"denied"}', clauses=["_Rejected", "Exception"])
add("catch_dispatch", result_json='{"error":"denied"}', clauses=["BaseException", "Exception"])
add("catch_dispatch", result_json='{"error":"denied"}', clauses=["KeyboardInterrupt", "SystemExit", "GeneratorExit"])
add("catch_dispatch", result_json='{"error":"denied"}', clauses=["__NotAnExc__", "FileNotFoundError", "Exception", "ValueError"])
add("catch_dispatch", result_json='{"error":"denied"}', clauses=[])
add("catch_dispatch", result_json='{"error":"denied"}', clauses=["RuntimeError", "ValueError", "TypeError"])

with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")

from collections import Counter
counts = Counter(c["func"] for c in CASES)
print("wrote %s: %d cases (%s)" % (OUT, len(CASES), dict(counts)))
