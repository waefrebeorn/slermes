#!/usr/bin/env python3
"""Oracle: tools/url_safety.py:_allows_private_ip_resolution vs LIVE Python."""
import json, sys, os, types
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import importlib.util
spec = importlib.util.spec_from_file_location("us_mod", "/home/wubu/hermes-agent-dev/tools/url_safety.py")
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
    exp = bool(mod._allows_private_ip_resolution(b["host"], b["scheme"]))
    if exp != bool(b["out"]): mm += 1; print(f"MISMATCH {b}: exp {exp} got {b['out']}")
if mm:
    print(f"oracle: {mm} mismatch(es)"); sys.exit(1)
print("oracle: 0 mismatches")
