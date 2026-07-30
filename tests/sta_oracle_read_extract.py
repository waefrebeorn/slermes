#!/usr/bin/env python3
"""
sta_oracle_read_extract.py — Python oracle for the PURE document-type helpers in
tools/read_extract.py (_extension, is_extractable_document), ported in
src/tools/port_tools_read_extract.c.

Imports the REAL tools.read_extract module and calls the genuine _extension /
is_extractable_document. Output contract matches tests/t_port_read_extract.c:
one JSON object per line, sorted keys, ensure_ascii=False, compact separators.
No sample files required (pure extension classification).
"""

import json
import os
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from tools.read_extract import _extension, is_extractable_document  # noqa: E402


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


def main():
    for raw in sys.stdin:
        line = raw.rstrip("\n")
        if not line.strip() or line.startswith("#"):
            continue
        if line.startswith("ext "):
            v = line[4:]
            emit({
                "op": "ext",
                "in": v,
                "ext": _extension(v),
                "extractable": bool(is_extractable_document(v)),
            })
        else:
            emit({"op": "unknown", "raw": raw.rstrip("\n")})


if __name__ == "__main__":
    sys.exit(main())
