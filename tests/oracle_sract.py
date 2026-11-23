#!/usr/bin/env python3
"""Differential oracle for session_rules/activity.py (gate sract)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/session_rules/activity.py"
spec = importlib.util.spec_from_file_location("sract_live", SRC)
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
    func = r.get("func")
    got = r.get("out")
    if func == "bound":
        want = live.bound_activity_description(r.get("description"))
    elif func == "normalize":
        want = live.normalize_activity_provenance(r.get("provenance")).value
    elif func == "max":
        want = live.ACTIVITY_DESCRIPTION_MAX
    else:
        continue
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH in={r} py={want!r} c={got!r}", file=sys.stderr)
print(f"ORACLE sract: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
