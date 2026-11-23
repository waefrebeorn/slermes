#!/usr/bin/env python3
"""Oracle for upstream Hermes cron/constants.py."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
SRC = "C:/Users/eman5/hermes-agent/cron/constants.py"
spec = importlib.util.spec_from_file_location("cronconst_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)
cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    if func == "constants":
        py = {"FIRE_CLAIM_TTL_SECONDS": live.FIRE_CLAIM_TTL_SECONDS,
              "FIRE_CLAIM_SKEW_SECONDS": live.FIRE_CLAIM_SKEW_SECONDS,
              "CLAIM_TTL_INACTIVITY_HEADROOM": live.CLAIM_TTL_INACTIVITY_HEADROOM}
    elif func == "is_recurring":
        py = live.is_recurring(r.get("job"))
    else: continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE cronconst: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
