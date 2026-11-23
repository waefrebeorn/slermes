#!/usr/bin/env python3
"""Differential oracle for upstream Hermes session_rules/results.py."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, Q)
NAME = "w8_results_live"
spec = importlib.util.spec_from_file_location(NAME, Q + "/session_rules/results.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    if f == "reanchor":
        return live.reanchor_current_turn_user_idx(r.get("messages"), r.get("user_message"))
    if f == "compact":
        return live.compact_result(r.get("result"), user_message=r.get("user_message"))
    return None

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE results: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)