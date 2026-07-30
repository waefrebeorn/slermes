#!/usr/bin/env python3
"""Faithfulness oracle for port_tools_xai_http.c:_coerce_expires_after.

Reads JSON lines from t_port_xai_expires.c and recomputes the SAME function
from the LIVE tools/xai_http.py. The C function returns a string
("null" or decimal seconds); Python returns None or int — normalized to
string for comparison.
"""
import sys, os, json
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from tools.xai_http import _coerce_expires_after

def norm(v):
    if v is None:
        return "null"
    return str(int(v))

mism = 0
n = 0
for line in sys.stdin:
    line = line.strip()
    if not line or not line.startswith("{"):
        continue
    rec = json.loads(line)
    fn = rec["fn"]
    if fn != "coerce_expires":
        print("UNKNOWN FN", fn); continue
    n += 1
    inp = rec["in"]
    exp = norm(_coerce_expires_after(inp if inp != "" else None))
    got = rec["out"]
    if exp != got:
        mism += 1
        print(f"MISMATCH in={inp!r} PY={exp!r} C={got!r}")
print(f"XAI_EXPIRES oracle: {n} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
