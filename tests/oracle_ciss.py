#!/usr/bin/env python3
"""Differential oracle for upstream Hermes hermes_cli/config_issues.py."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/config_issues.py"
spec = importlib.util.spec_from_file_location("ciss_live", SRC)
live = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "_issue":
        continue
    issues = []
    for c in r.get("calls", []):
        live._issue(issues, c.get("severity", ""), c.get("message", ""), c.get("hint", ""))
    py = [{"severity": i.severity, "message": i.message, "hint": i.hint} for i in issues]
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH in={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE ciss: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
