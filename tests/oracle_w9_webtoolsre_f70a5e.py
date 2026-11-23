#!/usr/bin/env python3
"""Differential oracle for tools/web_tools_rescue.py (gate w9_webtoolsre_f70a5e)."""
import importlib.util
import json
import sys

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/tools/web_tools_rescue.py"
spec = importlib.util.spec_from_file_location("w9_webtoolsre_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def compute(r):
    if r.get("func") == "policy_blocked":
        return live._policy_blocked_result(r.get("result", {}))
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
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w9_webtoolsre_f70a5e: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if (mism or not cases) else 0)
