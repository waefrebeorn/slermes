#!/usr/bin/env python3
"""Differential oracle for upstream Hermes agent/bedrock_service_tiers.py."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/bedrock_service_tiers.py"
spec = importlib.util.spec_from_file_location("bedtier_live", SRC)
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
    if r.get("func") != "bedrock_fast_mode_overrides":
        continue
    py = live.bedrock_fast_mode_overrides(r.get("model_id"), r.get("base_url"), r.get("tier"))
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH in={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE bedtier: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
