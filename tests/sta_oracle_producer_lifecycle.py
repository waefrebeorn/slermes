#!/usr/bin/env python3
"""Oracle for gateway/session_producer_lifecycle.py:_executed_outcome. The
source function and the ProcessingOutcome enum are ast-extracted and run live.
"""
import ast
import json
import sys

LIFE = "C:/Users/eman5/hermes-agent/gateway/session_producer_lifecycle.py"
EVENT = "C:/Users/eman5/hermes-agent/gateway/platforms/event.py"

items = []
life_code = open(LIFE, encoding="utf-8").read()
for n in ast.parse(life_code).body:
    if isinstance(n, ast.FunctionDef) and n.name == "_executed_outcome":
        items.append(ast.get_source_segment(life_code, n))

ev_code = open(EVENT, encoding="utf-8").read()
for n in ast.parse(ev_code).body:
    if isinstance(n, ast.ClassDef) and n.name == "ProcessingOutcome":
        items.append(ast.get_source_segment(ev_code, n))

NS = {"Enum": __import__("enum", fromlist=["Enum"]).Enum}
for seg in items:
    exec(compile(ast.parse(seg), "x", "exec"), NS)
fn = NS["_executed_outcome"]


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE producer_lifecycle: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        want = fn(c["cap"]).value
        if c["out"] != want:
            mism += 1
            print(f"MISMATCH outcome {c['case']}: C={c['out']} PY={want} "
                  f"cap={c['cap']!r}")
    print(f"ORACLE producer_lifecycle: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
