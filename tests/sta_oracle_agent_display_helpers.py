#!/usr/bin/env python3
"""Oracle: agent/display.py pure helpers vs LIVE Python."""
import json, sys, os, types
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import importlib.util
spec = importlib.util.spec_from_file_location("disp_mod", "/home/wubu/hermes-agent-dev/agent/display.py")
mod = importlib.util.module_from_spec(spec)
try:
    spec.loader.exec_module(mod)
except Exception as e:
    print("IMPORT_FAIL", repr(e)); sys.exit(2)

mm = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    b = json.loads(line)
    if b["t"] == "oneline":
        exp = mod._oneline(b["in"])
        if exp != b["out"]: mm += 1; print(f"MISMATCH oneline {b['in']!r}: exp {exp!r} got {b['out']!r}")
    elif b["t"] == "trunc":
        exp = mod._truncate_preview(b["in"], b["m"])
        if exp != b["out"]: mm += 1; print(f"MISMATCH trunc {b['in']!r} m={b['m']}: exp {exp!r} got {b['out']!r}")
    elif b["t"] == "base":
        exp = mod._shell_basename(b["in"])
        if exp != b["out"]: mm += 1; print(f"MISMATCH base {b['in']!r}: exp {exp!r} got {b['out']!r}")
if mm:
    print(f"oracle: {mm} mismatch(es)"); sys.exit(1)
print("oracle: 0 mismatches")
