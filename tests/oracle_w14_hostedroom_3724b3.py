#!/usr/bin/env python3
"""Differential oracle for gateway/hosted_room_context.py (gate w14_hostedroom_3724b3).

The LIVE scheduled_post_kind runs on the actor value parsed from each fixture
case; its result is compared to the `out` emitted by the C harness.
"""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
SRC = "C:/Users/eman5/hermes-agent/gateway/hosted_room_context.py"
spec = importlib.util.spec_from_file_location("w14_hrc_live", SRC)
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
    func = r.get("func")
    if func != "scheduled_post_kind":
        continue
    want = live.scheduled_post_kind(r.get("actor"))
    got = r.get("out")
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH {func} in={r} py={want!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w14_hostedroom_3724b3: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
