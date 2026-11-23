#!/usr/bin/env python3
"""Differential oracle for tools/connectors/data_use.py (gate connuse)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/tools/connectors/data_use.py"
spec = importlib.util.spec_from_file_location("connuse_live", SRC)
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
    got = r.get("out")
    if func == "collect":
        with live.collect_data_use() as sink:
            for batch in (r.get("batches") or []):
                live.record_data_use(batch)
        want = sorted(sink)
    elif func == "after_scope":
        with live.collect_data_use() as sink:
            live.record_data_use(r.get("inside") or [])
        live.record_data_use(r.get("outside") or [])  # context exited: no-op
        want = sorted(sink)
    elif func == "outside":
        live.record_data_use(r.get("requires") or [])  # no sink bound: no-op
        want = []
    else:
        continue
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH in={r} py={want!r} c={got!r}", file=sys.stderr)
print(f"ORACLE connuse: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
