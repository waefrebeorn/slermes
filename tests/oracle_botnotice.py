#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/bot_peer_notices.py."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/gateway/bot_peer_notices.py"
spec = importlib.util.spec_from_file_location("botnotice_live", SRC)
live = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live)

class Source:
    pass

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "automatic_notices_allowed":
        continue
    src = Source()
    if "is_bot" in r:
        src.is_bot = r["is_bot"]
    py = live.automatic_notices_allowed(src)
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH in={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE botnotice: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
