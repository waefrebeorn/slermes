#!/usr/bin/env python3
"""
sta_oracle_skills_guard.py — Python oracle for the skills_guard helpers in
tools/skills_guard.py,ported in src/cli/port_tools_skills_guard.c.

Imports the REAL tools.skills_guard module and calls the genuine functions.
Reads the fixture from argv[1] (one op per line). For _determine_verdict the
Python API takes a findings list; we map the C's severity int
(3=critical,2=high,1=medium,0=low) to a minimal findings object whose
.severity attribute drives the verdict, exactly as Python does.
Output contract matches tests/t_port_skills_guard.c: one JSON object per line,
compact separators, ensure_ascii=False.
"""

import json
import os
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

import tools.skills_guard as sg
from tools.skills_guard import (
    _resolve_trust_level,
    _determine_verdict,
    _content_digest,
    full_content_hash,
    _finding_dict,
    scan_skill_cached,
    SCANNER_VERSION,
)


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


SEV_NAME = {3: "critical", 2: "high", 1: "medium", 0: "low"}


class _F:
    def __init__(self, severity):
        self.severity = severity


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_skills_guard.py <cases.in>\n")
        return 2
    fixture = sys.argv[1]
    base = os.environ.get("ORACLE_FIXDIR") or os.path.dirname(os.path.abspath(fixture))
    import pathlib

    def rp(p):
        return pathlib.Path(base) / p if p and not os.path.isabs(p) else pathlib.Path(p)

    with open(fixture, "r", encoding="utf-8") as fp:
        for raw in fp:
            line = raw.rstrip("\n")
            if not line.strip() or line.startswith("#"):
                continue
            op, _, rest = line.partition(" ")
            rest = rest.strip()

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
            elif op == "content_digest":
                emit({"op": "content_digest", "path": rest, "out": _content_digest(rp(rest))})
            elif op == "full_content_hash":
                emit({"op": "full_content_hash", "path": rest, "out": full_content_hash(rp(rest))})
            elif op == "finding_dict":
                parts = rest.split(" ", 6)
                while len(parts) < 7:
                    parts.append("")
                f = sg.Finding(pattern_id=parts[0], severity=parts[1], category=parts[2],
                               file=parts[3], line=int(parts[4] or 0), match="",
                               description=parts[6])
                emit({"op": "finding_dict", "out": _finding_dict(f)})
            elif op == "scan_skill_cached":
                parts = rest.split(" ", 3)
                path = parts[0] if parts else ""
                source = parts[1] if len(parts) > 1 else ""
                source_url = parts[2] if len(parts) > 2 else ""
                cache_dir = parts[3] if len(parts) > 3 else ""
                res, prov = scan_skill_cached(
                    rp(path),
                    source or "community",
                    source_url=source_url or "",
                    cache_dir=(rp(cache_dir) if cache_dir else None),
                )
                emit({
                    "op": "scan_skill_cached",
                    "path": path,
                    "rc": 0,
                    "fresh": int(not prov.get("fresh", True)),
                    "verdict": res.verdict,
                    "trust_level": res.trust_level,
                    "summary": res.summary,
                })
            else:
                emit({"op": "unknown", "raw": line})


if __name__ == "__main__":
    sys.exit(main())
