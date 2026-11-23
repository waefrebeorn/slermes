#!/usr/bin/env python3
"""Differential oracle for acp_adapter/entry.py (gate w6_entry_b16419)."""
import importlib.util, json, sys
sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/acp_adapter/entry.py"
spec = importlib.util.spec_from_file_location("w6_entry_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w6_entry_live"] = live
spec.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    if f == "parse_args":
        return vars(live._parse_args(r.get("argv", [])))
    if f == "is_benign_probe":
        return r.get("method", "") in live._BENIGN_PROBE_METHODS
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
print(f"ORACLE w6_entry_b16419: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
