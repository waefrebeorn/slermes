#!/usr/bin/env python3
"""Faithfulness oracle for port_agent_model_metadata.c:is_output_cap_error.

Reads JSON lines emitted by t_port_model_metadata_output_cap.c and recomputes
the SAME function from the LIVE agent/model_metadata.py.
"""
import sys, os, json
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from agent.model_metadata import is_output_cap_error

mism = 0
n = 0
for line in sys.stdin:
    line = line.strip()
    if not line or not line.startswith("{"):
        continue
    rec = json.loads(line)
    fn = rec["fn"]
    if fn != "is_output_cap_error":
        print("UNKNOWN FN", fn); continue
    n += 1
    msg = rec["msg"]
    exp = 1 if is_output_cap_error(msg if msg != "" else "") else 0
    got = rec["out"]
    if exp != got:
        mism += 1
        print(f"MISMATCH msg={msg!r} PY={exp!r} C={got!r}")
print(f"OUTPUT_CAP_ERROR oracle: {n} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
