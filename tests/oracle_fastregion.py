#!/usr/bin/env python3
"""Oracle for hermes_cli/models_fast_regions.py:openai_fast_region_allows (gate fastregion)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
SRC = QUARRY + "/hermes_cli/models_fast_regions.py"
spec = importlib.util.spec_from_file_location("fastregion_live", SRC)
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
    if r.get("func") != "allows":
        continue
    py = live.openai_fast_region_allows(
        r.get("base_url"), r.get("model_id"), r.get("tier"))
    got = r.get("out")
    cases += 1
    if bool(py) != bool(got):
        mism += 1
        print(f"MISMATCH {r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE fastregion: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
