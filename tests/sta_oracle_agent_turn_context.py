#!/usr/bin/env python3
"""Oracle: agent/turn_context.py _compression_made_progress vs LIVE Python."""
import json, sys, os
sys.path.insert(0, "/home/wubu/hermes-agent-dev/agent")
import turn_context as mod

mm = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    b = json.loads(line)
    if b["t"] == "prog":
        exp = bool(mod._compression_made_progress(b["o"], b["n"], b["ot"], b["nt"]))
        if exp != bool(b["out"]): mm += 1; print(f"MISMATCH prog {b}: exp {exp} got {b['out']}")
if mm:
    print(f"oracle: {mm} mismatch(es)"); sys.exit(1)
print("oracle: 0 mismatches")
