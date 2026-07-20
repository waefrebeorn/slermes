#!/usr/bin/env python3
"""
sta_oracle_tool_output_limits.py — Python oracle for the PURE tool-output-limit
helpers in tools/tool_output_limits.py (_coerce_positive_int, get_max_bytes,
get_max_lines, get_max_line_length), ported in
src/cli/port_tools_tool_output_limits.c.

Imports the REAL tools.tool_output_limits module; with no config present it
returns the module defaults (matching the C hard-coded defaults). Output
contract matches tests/t_port_tool_output_limits.c: one JSON object per line,
sorted keys, ensure_ascii=False, compact separators.
"""

import json
import os
import sys
import tempfile

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from tools.tool_output_limits import (  # noqa: E402
    _coerce_positive_int,
    get_max_bytes,
    get_max_lines,
    get_max_line_length,
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

        if op == "coerce":
            parts = rest.split()
            v = int(parts[0]) if parts else 0
            d = int(parts[1]) if len(parts) > 1 else 0
            emit({"op": "coerce", "in": v, "default": d,
                  "out": _coerce_positive_int(v, d)})
        elif op == "limits":
            emit({"op": "limits", "max_bytes": get_max_bytes(),
                  "max_lines": get_max_lines(),
                  "max_line_length": get_max_line_length()})
        else:
            emit({"op": "unknown", "raw": raw.rstrip("\n")})


if __name__ == "__main__":
    sys.exit(main())
