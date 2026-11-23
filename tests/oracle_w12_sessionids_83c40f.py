#!/usr/bin/env python3
"""Differential oracle for upstream Hermes session_rules/session_ids.py (gate w12_sessionids_83c40f)."""
import importlib.util
import json
import sys

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = QUARRY + "/session_rules/session_ids.py"
spec = importlib.util.spec_from_file_location("session_ids_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["session_ids_live"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "is_known_session_id":
        return bool(live.is_known_session_id(r.get("value", "")))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w12_sessionids_83c40f: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
