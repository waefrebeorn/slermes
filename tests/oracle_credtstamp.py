#!/usr/bin/env python3
"""Oracle for agent/credential_pool_timestamps.py:_parse_absolute_timestamp (gate credtstamp)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/credential_pool_timestamps.py"
spec = importlib.util.spec_from_file_location("credtstamp_live", SRC)
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
    if r.get("func") != "parse":
        continue
    py = live._parse_absolute_timestamp(r.get("value"))
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH {r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE credtstamp: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
