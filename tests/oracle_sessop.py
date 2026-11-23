#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/session_operator.py."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.path.insert(0, "C:/Users/eman5/hermes-agent")

SRC = "C:/Users/eman5/hermes-agent/gateway/session_operator.py"
spec = importlib.util.spec_from_file_location("session_operator_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["session_operator_live"] = live
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
    if r.get("func") != "is_viewer_input":
        continue
    cases += 1
    row = {"request_id": r.get("request_id"), "payload": r.get("payload", {})}
    py = live.is_viewer_input(row)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH is_viewer_input py={py!r} c={got!r} row={row!r}", file=sys.stderr)
print(f"ORACLE sessop: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
