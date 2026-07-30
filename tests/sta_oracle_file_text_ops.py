#!/usr/bin/env python3
"""
sta_oracle_file_text_ops.py — Python oracle for the PURE text helpers in
tools/file_operations.py (ShellFileOperations): _escape_shell_arg,
_add_line_numbers, ported in src/tools/file_text_ops.c.

Imports the REAL tools.file_operations module and calls the genuine methods on
a ShellFileOperations instance (with a dummy terminal_env — these two helpers
don't touch executor state). Output contract matches
tests/t_port_file_text_ops.c: one JSON object per line, sorted keys,
ensure_ascii=False, compact separators. Fixture content uses literal \\n / \\t
tokens decoded identically on both sides.
"""

import json
import os
import sys
import tempfile

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

# Temp HERMES_HOME so get_max_line_length() uses its default (2000) — fixture
# lines stay well under that, so no truncation triggers and C (max=0) matches.
os.environ["HERMES_HOME"] = tempfile.mkdtemp(prefix="slermes_oracle_textops_")

from tools.file_operations import ShellFileOperations  # noqa: E402


class _DummyEnv:
    pass


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
    fo = ShellFileOperations(terminal_env=_DummyEnv())
    for raw in sys.stdin:
        parsed = split_kv(raw)
        if parsed is None:
            continue
        op, rest = parsed
        v = dec(rest) if rest else ""

        if op == "escape":
            emit({"op": "escape", "in": v, "out": fo._escape_shell_arg(v)})
        elif op == "linenum":
            emit({"op": "linenum", "in": v, "out": fo._add_line_numbers(v)})
        else:
            emit({"op": "unknown", "raw": raw.rstrip("\n")})


if __name__ == "__main__":
    sys.exit(main())
