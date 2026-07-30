#!/usr/bin/env python3
"""
sta_oracle_delegate_pure.py — Python oracle for the PURE delegate_tool.py
helpers (delegate_stringify_tool_content / delegate_looks_like_error_output /
delegate_normalize_role), ported in src/tools/delegate.c.

Imports the REAL tools.delegate_tool module and exercises the genuine functions.
Output contract matches tests/t_port_delegate_pure.c: one JSON object per line,
sorted keys, ensure_ascii=False, compact separators.
"""

import json
import os
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from tools.delegate_tool import (  # noqa: E402
    _stringify_tool_content,
    _looks_like_error_output,
    _normalize_role,
)


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


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
        v = rest if rest else ""

        if op == "stringify":
            try:
                content = json.loads(v) if v else None
            except Exception:
                content = v
            emit({"op": "stringify", "in": v,
                  "out": _stringify_tool_content(content)})

        elif op == "error":
            try:
                content = json.loads(v) if v else None
            except Exception:
                content = v
            emit({"op": "error", "in": v,
                  "is_error": bool(_looks_like_error_output(content))})

        elif op == "role":
            emit({"op": "role", "in": v, "out": _normalize_role(v if v else None)})

        else:
            emit({"op": "unknown", "raw": raw.rstrip("\n")})

    return 0


if __name__ == "__main__":
    sys.exit(main())
