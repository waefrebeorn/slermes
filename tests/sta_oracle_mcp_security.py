#!/usr/bin/env python3
"""Oracle: hermes_cli/mcp_security.py vs LIVE Python."""
import json, sys, os, types
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import importlib.util
spec = importlib.util.spec_from_file_location("ms_mod", "/home/wubu/hermes-agent-dev/hermes_cli/mcp_security.py")
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
    if b["t"] == "basename":
        exp = mod._command_basename(b["cmd"])
        if exp != b["out"]: mm += 1; print(f"MISMATCH basename {b['cmd']!r}: exp {exp!r} got {b['out']!r}")
    elif b["t"] == "inline":
        exp = mod._inline_script(b["args"])
        if exp != b["out"]: mm += 1; print(f"MISMATCH inline {b['args']!r}: exp {exp!r} got {b['out']!r}")
    elif b["t"] == "entry":
        # reconstruct entry dict from command \x1f args \x1f env...
        parts = b["e"].split("\x1f") if b["e"] else []
        entry = {"command": parts[0] if len(parts) > 0 else "", "args": parts[1] if len(parts) > 1 else "",
                 "env": {f"k{i}": v for i, v in enumerate(parts[2:])}}
        exp = mod._entry_text(entry)
        if exp != b["out"]: mm += 1; print(f"MISMATCH entry {b['e']!r}: exp {exp!r} got {b['out']!r}")
if mm:
    print(f"oracle: {mm} mismatch(es)"); sys.exit(1)
print("oracle: 0 mismatches")
