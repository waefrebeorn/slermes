#!/usr/bin/env python3
"""Oracle for upstream Hermes tools/clarify_batch.py."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
SRC = "C:/Users/eman5/hermes-agent/tools/clarify_batch.py"
spec = importlib.util.spec_from_file_location("clarbatch_live", SRC)
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
    if r.get("func") != "settle_batch":
        continue
    py = live.settle_batch(r["locked"], r.get("frame_result"), ended=r.get("ended"))
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH settle_batch args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE clarbatch: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
