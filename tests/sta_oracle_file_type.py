#!/usr/bin/env python3
"""
sta_oracle_file_type.py — Python oracle for the PURE file-type detection
helpers _is_image / _is_likely_binary (tools/file_operations.py), ported in
src/tools/file_fs_ops.c:file_fs_ops_is_image / file_fs_ops_is_likely_binary.

Imports the REAL tools.file_operations module; instantiates ShellFileOperations
with a dummy terminal_env and calls the genuine methods. No content sample is
passed, so both sides use the extension-only rule (matching the C ports, which
have no content-sample path). Output contract matches tests/t_port_file_type.c:
one JSON object per line, sorted keys, ensure_ascii=False, compact separators.
"""

import json
import os
import sys
import tempfile

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from tools.file_operations import ShellFileOperations  # noqa: E402


class _DummyEnv:
    pass


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


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
        v = rest if rest else ""

        if op == "image":
            emit({"op": "image", "in": v, "out": bool(fo._is_image(v))})
        elif op == "binary":
            emit({"op": "binary", "in": v, "out": bool(fo._is_likely_binary(v))})
        else:
            emit({"op": "unknown", "raw": raw.rstrip("\n")})


if __name__ == "__main__":
    sys.exit(main())
