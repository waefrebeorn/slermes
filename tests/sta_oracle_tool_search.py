#!/usr/bin/env python3
"""
sta_oracle_tool_search.py — Python oracle for the PURE tool-search helpers in
tools/tool_search.py (_tokenize, estimate_tokens_from_schemas), ported in
src/tools/tool_search.c.

Imports the REAL tools.tool_search module and calls the genuine functions.
For estimate_tokens_from_schemas the Python API takes a list of tool defs; we
pass a single-element list built from json.loads(schema) — for compact JSON
input this equals ceil(len(compact_json)/CHARS_PER_TOKEN), matching the C's
strlen-based count. Fixture content uses literal \\n tokens decoded identically
on both sides. Output contract matches tests/t_port_tool_search.c: one JSON
object per line, sorted keys, ensure_ascii=False, compact separators.
"""

import json
import os
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from tools.tool_search import (  # noqa: E402
    _tokenize,
    estimate_tokens_from_schemas,
)


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


def dec(s):
    if s is None:
        return ""
    return s.replace("\\n", "\n")


def main():
    for raw in sys.stdin:
        line = raw.rstrip("\n")
        if not line.strip() or line.startswith("#"):
            continue
        if line.startswith("tok "):
            v = dec(line[4:])
            emit({"op": "tok", "in": v, "tokens": _tokenize(v)})
        elif line.startswith("est "):
            v = dec(line[4:])
            td = json.loads(v) if v.strip() else {}
            emit({"op": "est", "in": v, "tokens": estimate_tokens_from_schemas([td])})
        else:
            emit({"op": "unknown", "raw": raw.rstrip("\n")})


if __name__ == "__main__":
    sys.exit(main())
