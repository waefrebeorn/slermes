#!/usr/bin/env python3
"""Faithfulness oracle for port_agent_oneshot.c:_strip_code_fence.

Reads JSON lines from t_port_agent_oneshot.c and recomputes the SAME function
from the LIVE agent/oneshot.py.
"""
import sys, os, json
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from agent.oneshot import _strip_code_fence

mism = 0
n = 0
for line in sys.stdin:
    line = line.strip()
    if not line or not line.startswith("{"):
        continue
    rec = json.loads(line)
    fn = rec["fn"]
    if fn != "strip_code_fence":
        print("UNKNOWN FN", fn); continue
    n += 1
    inp = rec["in"]
    exp = _strip_code_fence(inp if inp != "" else "")
    got = rec["out"]
    if exp != got:
        mism += 1
        print(f"MISMATCH in={inp!r} PY={exp!r} C={got!r}")
print(f"ONESHOT oracle: {n} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
