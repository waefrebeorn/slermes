#!/usr/bin/env python3
"""Oracle for upstream Hermes gateway/run_common.py."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
SRC = "C:/Users/eman5/hermes-agent/gateway/run_common.py"
spec = importlib.util.spec_from_file_location("runcsv_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)
cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    if r.get("func") != "to_set": continue
    py = sorted(live._csv_or_list_to_set(r.get("raw")))
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH to_set args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE runcsv: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
