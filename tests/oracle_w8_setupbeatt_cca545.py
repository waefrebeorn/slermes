#!/usr/bin/env python3
"""Differential oracle for tools/setup_beat_tool.py (gate w8_setupbeatt_cca545)."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
SRC = Q + "/tools/setup_beat_tool.py"
spec = importlib.util.spec_from_file_location("live_setupbeat_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["live_setupbeat_live"] = live
spec.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    slugs = r.get("slugs", None)
    if f == "suggest_from":
        return live._suggest_from(set(slugs) if slugs is not None else None)
    if f == "unlisted_suggestions":
        # Explicit seam for _connector_slugs() (the live function reads the host).
        live._connector_slugs = (lambda s=slugs: set(s) if s is not None else None)
        return live._unlisted_suggestions(r.get("args", {}))
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
    try:
        py = compute(r)
    except Exception:
        py = None
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w8_setupbeatt_cca545: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
