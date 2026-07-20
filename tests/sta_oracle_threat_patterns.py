#!/usr/bin/env python3
"""
sta_oracle_threat_patterns.py — Python oracle for the threat-pattern scanner in
tools/threat_patterns.py:scan_for_threats, ported in
src/cli/port_tools_threat_patterns.c.

Imports the REAL tools.threat_patterns module and calls the genuine
scan_for_threats. Emits the SET of matched ids (sorted) so the comparison is
order-independent and focuses on the MATCH SET — the correct security semantic.
Fixture content uses literal \\n tokens decoded identically on both sides.
Output contract matches tests/t_port_threat_patterns.c: one JSON object per
line, sorted keys, ensure_ascii=False, compact separators.
"""

import json
import os
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from tools.threat_patterns import scan_for_threats  # noqa: E402


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


def dec(s):
    if s is None:
        return ""
    return s.replace("\\n", "\n")


def main():
    for raw in sys.stdin:
        line = raw.rstrip("\n")
        if not line.strip() or line.startswith("#"):
            continue
        if line.startswith("scan "):
            rest = line[5:]
            bar = rest.find(" | ")
            if bar < 0:
                emit({"op": "scan", "error": "bad-fixture"})
                continue
            scope = rest[:bar]
            content = dec(rest[bar + 3:])
            found = scan_for_threats(content, scope=scope)
            emit({
                "op": "scan",
                "scope": scope,
                "content": content,
                "found": sorted(found),
            })
        else:
            emit({"op": "unknown", "raw": raw.rstrip("\n")})


if __name__ == "__main__":
    sys.exit(main())
