#!/usr/bin/env python3
"""
sta_oracle_search_context.py — Python oracle for the PURE search-context-line
parser _parse_search_context_line (tools/file_operations.py), ported in
src/tools/file_text_ops.c:file_text_ops_parse_search_context_line.

Imports the REAL tools.file_operations module and calls the genuine function.
Output contract matches tests/t_port_search_context.c: one JSON object per
line, sorted keys, ensure_ascii=False, compact separators. The 'out' field is
a JSON array [path, line, content] (matching the C's return) or null (Python
None), so both sides are directly comparable.
"""

import json
import os
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from tools.file_operations import _parse_search_context_line  # noqa: E402


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


def main():
    for raw in sys.stdin:
        line = raw.rstrip("\n")
        if not line.strip() or line.startswith("#"):
            continue
        parsed = _parse_search_context_line(line if line else None)
        if parsed is None:
            emit({"in": line, "out": None})
        else:
            path, ln, content = parsed
            # Emit the same array shape [path, line, content] as the C port.
            emit({"in": line, "out": [path, ln, content]})


if __name__ == "__main__":
    sys.exit(main())
