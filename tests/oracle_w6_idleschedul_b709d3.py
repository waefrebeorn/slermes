#!/usr/bin/env python3
"""Differential oracle for agent/idle_schedule.py (w6_idleschedul_b709d3 gate).

The module imports only stdlib at module scope, so it loads directly. The
interval gate is compared as {due, state}: the C port returns the mutated state
alongside the boolean; the oracle copies the fixture state, runs the live
interval_due with a no-op save callback, and compares the same shape.
"""
import importlib.util, json, sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/agent/idle_schedule.py"
spec = importlib.util.spec_from_file_location("live_idle_schedule", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["live_idle_schedule"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    try:
        if func == "parse_iso_ok":
            py = 1 if live.parse_iso(r.get("ts")) is not None else 0
        elif func == "interval_due":
            st = json.loads(json.dumps(r.get("state", {})))
            due = live.interval_due(
                st, r.get("interval_hours", 0.0),
                now=live.parse_iso(r.get("now")),
                seed_summary=r.get("seed_summary", ""),
                save=lambda s: None,
            )
            py = {"due": bool(due), "state": st}
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w6_idleschedul_b709d3: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
