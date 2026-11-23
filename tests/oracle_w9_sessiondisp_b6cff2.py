#!/usr/bin/env python3
"""Differential oracle for gateway/session_display.py pure helpers."""
import importlib.util, json, sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
SRC = QUARRY + "/gateway/session_display.py"
spec = importlib.util.spec_from_file_location("sd_w9_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["sd_w9_live"] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    try:
        if f == "submit_fields":
            return live.submit_display_fields(r.get("params") or {})
        if f == "admit":
            return live.admit_display(r.get("params") or {})
        if f == "restore":
            return live.restore_display(r.get("committed"))
    except live.AuthorityRefusal:
        return {"error": "AuthorityRefusal"}
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w9_sessiondisp_b6cff2: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
