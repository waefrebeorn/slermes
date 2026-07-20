#!/usr/bin/env python3
"""
sta_oracle_skills_guard.py — Python oracle for the PURE skill-trust helpers in
tools/skills_guard.py (_resolve_trust_level, _determine_verdict), ported in
src/cli/port_tools_skills_guard.c.

Imports the REAL tools.skills_guard module and calls the genuine functions.
For _determine_verdict the Python API takes a findings list; we map the C's
severity int (3=critical,2=high,1=medium,0=low) to a minimal findings object
whose .severity attribute drives the verdict, exactly as Python does.
Output contract matches tests/t_port_skills_guard.c: one JSON object per line,
sorted keys, ensure_ascii=False, compact separators.
"""

import json
import os
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from tools.skills_guard import (  # noqa: E402
    _resolve_trust_level,
    _determine_verdict,
)


class _F:
    def __init__(self, severity):
        self.severity = severity


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


def split_kv(line):
    line = line.rstrip("\n")
    if not line.strip() or line.startswith("#"):
        return None
    op, _, rest = line.partition(" ")
    return op, rest


SEV_NAME = {3: "critical", 2: "high", 1: "medium", 0: "low"}


def main():
    for raw in sys.stdin:
        parsed = split_kv(raw)
        if parsed is None:
            continue
        op, rest = parsed

        if op == "trust":
            v = rest if rest else ""
            emit({"op": "trust", "in": v, "out": _resolve_trust_level(v)})
        elif op == "verdict":
            parts = rest.split()
            sev = int(parts[0]) if parts else 0
            fnd = int(parts[1]) if len(parts) > 1 else 0
            findings = [_F(SEV_NAME[sev])] if sev > 0 else []
            emit({
                "op": "verdict",
                "severity": sev,
                "findings": fnd,
                "out": _determine_verdict(findings),
            })
        else:
            emit({"op": "unknown", "raw": raw.rstrip("\n")})


if __name__ == "__main__":
    sys.exit(main())
