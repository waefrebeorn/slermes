#!/usr/bin/env python3
"""Oracle for upstream Hermes session_rules/surfaces.py."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
SRC = "C:/Users/eman5/hermes-agent/session_rules/surfaces.py"
spec = importlib.util.spec_from_file_location("ssurf_live", SRC)
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
    if r.get("func") != "created_surface":
        continue
    py = live.created_surface(r.get("label"))
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH created_surface args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE ssurf: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
