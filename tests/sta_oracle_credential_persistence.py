#!/usr/bin/env python3
"""
sta_oracle_credential_persistence.py — Python oracle for the PURE credential-key
sanitization helpers in agent/credential_persistence.py (_normalize_key,
_is_secret_payload_key), ported in src/agent/credential_persistence.c.

Imports the REAL agent.credential_persistence module and calls the genuine
functions. Output contract matches tests/t_port_credential_persistence.c: one
JSON object per line, sorted keys, ensure_ascii=False, compact separators.
"""

import json
import os
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from agent.credential_persistence import (  # noqa: E402
    _normalize_key,
    _is_secret_payload_key,
    _fingerprint_value,
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

        if op == "norm":
            emit({"op": "norm", "in": v, "out": _normalize_key(v)})
        elif op == "secret":
            emit({"op": "secret", "in": v, "out": bool(_is_secret_payload_key(v))})
        elif op == "fp":
            emit({"op": "fp", "in": v, "out": _fingerprint_value(v)})
        else:
            emit({"op": "unknown", "raw": raw.rstrip("\n")})


if __name__ == "__main__":
    sys.exit(main())
