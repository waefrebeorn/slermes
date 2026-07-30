#!/usr/bin/env python3
"""
sta_oracle_skills_hub_path.py — Python oracle for the PURE path-validation
helpers in tools/skills_hub.py (_normalize_bundle_path / _validate_skill_name
/ _validate_install_parent_path / _normalize_lock_install_path).

Imports the REAL module and exercises the genuine functions. Output contract
matches tests/t_port_skills_hub_path.c: one JSON object per line, sorted keys,
ensure_ascii=False, compact separators.
"""

import json
import os
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from tools.skills_hub import (  # noqa: E402
    _validate_skill_name,
    _validate_install_parent_path,
    _normalize_lock_install_path,
)


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


def split_kv(line):
    line = line.rstrip("\n")
    if not line.strip() or line.startswith("#"):
        return None
    op, _, rest = line.partition(" ")
    return op, rest


def call_ok(fn, *args):
    """Return (ok, out) where out is the normalized string or '' on ValueError."""
    try:
        return True, fn(*args)
    except (ValueError, Exception):
        return False, ""


def main():
    for raw in sys.stdin:
        parsed = split_kv(raw)
        if parsed is None:
            continue
        op, rest = parsed
        v = rest if rest else ""

        if op == "validate_skill":
            ok, out = call_ok(_validate_skill_name, v)
            emit({"op": "validate_skill", "in": v, "ok": ok, "out": out})

        elif op == "validate_parent":
            ok, out = call_ok(_validate_install_parent_path, v)
            emit({"op": "validate_parent", "in": v, "ok": ok, "out": out})

        elif op == "lock_path":
            if "|" in v:
                skill, _, ipath = v.partition("|")
            else:
                skill, ipath = v, ""
            ok, out = call_ok(_normalize_lock_install_path, ipath, skill)
            emit({"op": "lock_path", "skill": skill, "install_path": ipath,
                  "ok": ok, "out": out})

        else:
            emit({"op": "unknown", "raw": raw.rstrip("\n")})

    return 0


if __name__ == "__main__":
    sys.exit(main())
