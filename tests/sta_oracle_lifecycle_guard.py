#!/usr/bin/env python3
"""Oracle: cron/lifecycle_guard.contains_gateway_lifecycle_command vs LIVE Python."""
import json, sys, os, types
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import importlib.util
spec = importlib.util.spec_from_file_location("lg_mod", "/home/wubu/hermes-agent-dev/cron/lifecycle_guard.py")
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
    exp = bool(mod.contains_gateway_lifecycle_command(b["text"]))
    if exp != bool(b["out"]):
        mm += 1; print("MISMATCH %s: exp %s got %s" % (b, exp, b["out"]))
if mm:
    print("oracle: %d mismatch(es)" % mm); sys.exit(1)
print("oracle: 0 mismatches")
