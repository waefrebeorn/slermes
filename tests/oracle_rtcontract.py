#!/usr/bin/env python3
"""Oracle for gateway/runtime_contract.py:next_start_action (gate rtcontract)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/gateway/runtime_contract.py"
spec = importlib.util.spec_from_file_location("rtcontract_live", SRC)
live = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live)

RAISES = "__raises_ValueError__"
cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "action":
        continue
    obs = live.RuntimeObservation(
        state=r.get("state", ""),
        installed_service=bool(r.get("installed", False)),
        service_may_start=bool(r.get("may_start", False)),
        update_paused=bool(r.get("paused", False)),
    )
    try:
        py = live.next_start_action(obs)
    except ValueError:
        py = RAISES
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH {r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE rtcontract: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
