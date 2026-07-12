#!/usr/bin/env python3
"""Oracle: hermes_cli/checkpoints.py formatters vs LIVE Python."""
import json, sys, os, types
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import importlib.util
spec = importlib.util.spec_from_file_location("cp_mod", "/home/wubu/hermes-agent-dev/hermes_cli/checkpoints.py")
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
    if b["t"] == "bytes":
        exp = mod._fmt_bytes(b["n"])
        if exp != b["out"]: mm += 1; print(f"MISMATCH bytes {b['n']}: exp {exp!r} got {b['out']!r}")
    elif b["t"] == "age":
        import time
        real = time.time
        time.time = lambda: b["now"]
        try:
            exp = mod._fmt_age(b["ts"])
        finally:
            time.time = real
        if exp != b["out"]: mm += 1; print(f"MISMATCH age ts={b['ts']} now={b['now']}: exp {exp!r} got {b['out']!r}")
if mm:
    print(f"oracle: {mm} mismatch(es)"); sys.exit(1)
print("oracle: 0 mismatches")
