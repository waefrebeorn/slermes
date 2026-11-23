#!/usr/bin/env python3
"""Oracle for upstream Hermes hermes_cli/toolset_scope.py (gate tscope)."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
SRC = "C:/Users/eman5/hermes-agent/hermes_cli/toolset_scope.py"
spec = importlib.util.spec_from_file_location("tscope_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    if r.get("func") != "toolset_allowed_for_platform": continue
    py = live.toolset_allowed_for_platform(r.get("ts_key", ""), r.get("platform", ""))
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE tscope: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
