#!/usr/bin/env python3
"""Differential oracle for agent/tool_progress_relay.py (gate w11_toolprogre_3f5e37).

Imports the LIVE module directly (stdlib-only imports); the wire payload
builder and the unbound reporter accessor need no stubs.
"""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

SRC = QUARRY + "/agent/tool_progress_relay.py"
spec = importlib.util.spec_from_file_location("w11_toolprogre_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w11_toolprogre_live"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "payload":
        return live.tool_progress_payload(r.get("name"), r.get("message"), r.get("kwargs") or {})
    if func == "current":
        return live.current_tool_progress()
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
    try:
        py = compute(r)
    except Exception as exc:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')}: python raised {exc!r}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
if cases == 0:
    print("ORACLE w11_toolprogre_3f5e37: 0 cases (link failure in disguise)")
    sys.exit(1)
print(f"ORACLE w11_toolprogre_3f5e37: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
