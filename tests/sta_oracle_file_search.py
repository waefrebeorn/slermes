#!/usr/bin/env python3
"""
sta_oracle_file_search.py — Python oracle for the PURE file-search diagnostics
helpers in tools/file_operations.py (_search_stdout_and_limit,
_split_tool_diagnostics), ported in src/tools/port_file_operations_search.c.

Imports the REAL tools.file_operations module and exercises the genuine
functions. Output contract matches tests/t_port_file_search.c: one JSON object
per line, sorted keys, ensure_ascii=False, compact separators. Fixture content
uses literal \\n / \\t tokens decoded to real chars on BOTH sides.
"""

import json
import os
import re
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from tools.file_operations import (  # noqa: E402
    _search_stdout_and_limit,
    _split_tool_diagnostics,
    ExecuteResult,
)


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


def dec(s):
    """Decode \\n / \\t token escapes (mirror of C harness)."""
    if s is None:
        return ""
    return s.replace("\\n", "\n").replace("\\t", "\t")


def split_kv(line):
    line = line.rstrip("\n")
    if not line.strip() or line.startswith("#"):
        return None
    op, _, rest = line.partition(" ")
    return op, rest


def main():
    for raw in sys.stdin:
        parsed = split_kv(raw)
        if parsed is None:
            continue
        op, rest = parsed

        if op == "search":
            if "|" in rest:
                exit_s, _, stdout = rest.partition("|")
                exit_code = int(exit_s) if exit_s else 0
                stdout = dec(stdout)
            else:
                exit_code, stdout = 0, dec(rest)
            res = ExecuteResult(exit_code=exit_code, stdout=stdout)
            out, reason = _search_stdout_and_limit(res)
            emit({"op": "search", "exit_code": exit_code, "reason": reason, "out": out})

        elif op == "split":
            content = dec(rest) if rest else ""
            diagnostics, payload = _split_tool_diagnostics(content)
            emit({"op": "split", "diagnostics": diagnostics, "payload": payload})

        else:
            emit({"op": "unknown", "raw": raw.rstrip("\n")})


if __name__ == "__main__":
    sys.exit(main())
