#!/usr/bin/env python3
"""Differential oracle for agent/i18n_layers.py (gate w6_i18nlayers_f42323)."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/i18n_layers.py"
spec = importlib.util.spec_from_file_location("w6_i18nlayers_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w6_i18nlayers_live"] = live
spec.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    v = r.get("value")
    if f == "normalize_language_id":
        return live.normalize_language_id(v)
    if f == "is_language_id":
        return live.is_language_id(v)
    if f == "flatten":
        return live.flatten(v if v is not None else {})
    if f == "non_text_leaves":
        return live.non_text_leaves(v if v is not None else {})
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
print(f"ORACLE w6_i18nlayers_f42323: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
