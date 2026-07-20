#!/usr/bin/env python3
"""
sta_oracle_fuzzy_match_helpers.py — Python oracle for the PURE fuzzy-match
string helpers in tools/fuzzy_match.py (_unicode_normalize, _leading_whitespace,
_first_meaningful_line), ported in src/cli/port_fuzzy_match_helpers.c.

Imports the REAL tools.fuzzy_match module and calls the genuine functions.
Output contract matches tests/t_port_fuzzy_match_helpers.c: one JSON object per
line, sorted keys, ensure_ascii=False, compact separators. Fixture content uses
literal \\n / \\t tokens decoded identically on both sides.
"""

import json
import os
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from tools.fuzzy_match import (  # noqa: E402
    _unicode_normalize,
    _leading_whitespace,
    _first_meaningful_line,
)


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


def dec(s):
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
        v = dec(rest) if rest else ""

        if op == "norm":
            emit({"op": "norm", "in": v, "out": _unicode_normalize(v)})
        elif op == "lead":
            emit({"op": "lead", "in": v, "ws": _leading_whitespace(v)})
        elif op == "first":
            emit({"op": "first", "in": v, "out": _first_meaningful_line(v)})
        else:
            emit({"op": "unknown", "raw": raw.rstrip("\n")})


if __name__ == "__main__":
    sys.exit(main())
