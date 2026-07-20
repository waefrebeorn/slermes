#!/usr/bin/env python3
"""
sta_oracle_path_security.py — Python oracle for the path-safety helpers in
tools/path_security.py (has_traversal_component, validate_within_dir), ported
in src/cli/port_tools_path_security.c.

Imports the REAL tools.path_security module. For validate_within_dir it
materializes the same temp tree the C harness builds (real FS, no mocks) and
compares the SAFETY DECISION (safe vs escaped) rather than the exact error
string — the two ports emit differently-worded messages but must agree on
whether a path is safe. Output contract matches tests/t_port_path_security.c:
one JSON object per line, sorted keys, ensure_ascii=False, compact separators.
"""

import json
import os
import sys
from pathlib import Path

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from tools.path_security import (  # noqa: E402
    has_traversal_component,
    validate_within_dir,
)


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


def split_kv(line):
    line = line.rstrip("\n")
    if not line.strip() or line.startswith("#"):
        return None
    op, _, rest = line.partition(" ")
    return op, rest


def mkdir_p(p: str):
    os.makedirs(p, exist_ok=True)


def main():
    for raw in sys.stdin:
        parsed = split_kv(raw)
        if parsed is None:
            continue
        op, rest = parsed

        if op == "traversal":
            v = rest if rest else ""
            emit({"op": "traversal", "in": v, "has": bool(has_traversal_component(v))})
        elif op == "within":
            buf = rest
            bar = buf.find(" | ")
            if bar < 0:
                emit({"op": "within", "error": "bad-fixture"})
                continue
            root = buf[:bar]
            rel = buf[bar + 3:]
            mkdir_p(root)
            full = os.path.join(root, rel)
            parent = os.path.dirname(full)
            if parent:
                mkdir_p(parent)
            with open(full, "w") as fh:
                fh.write("x")
            err = validate_within_dir(Path(full), Path(root))
            emit({"op": "within", "in": full, "safe": err is None})
        else:
            emit({"op": "unknown", "raw": raw.rstrip("\n")})


if __name__ == "__main__":
    sys.exit(main())
