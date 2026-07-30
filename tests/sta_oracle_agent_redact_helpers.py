#!/usr/bin/env python3
"""Oracle: agent/redact.py helper subset vs LIVE Python (newline-delimited JSON)."""
import json, sys, os
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import importlib.util
spec = importlib.util.spec_from_file_location("redact_mod", "/home/wubu/hermes-agent-dev/agent/redact.py")
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

mm = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    b = json.loads(line)
    if b["t"] == "mask":
        exp = mod._mask_token_nonreusable(b["in"])
        if exp != b["out"]: mm += 1; print(f"MISMATCH mask {b['in']!r}: exp {exp!r} got {b['out']!r}")
    elif b["t"] == "envdump":
        exp = bool(mod.is_env_dump_command(b["in"])); got = bool(b["out"])
        if exp != got: mm += 1; print(f"MISMATCH envdump {b['in']!r}: exp {exp} got {got}")
if mm:
    print(f"oracle: {mm} mismatch(es)"); sys.exit(1)
print("oracle: 0 mismatches")
