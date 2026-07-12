#!/usr/bin/env python3
"""Oracle: agent/verify_hooks.py vs LIVE Python (newline-delimited JSON)."""
import json, sys, os, types
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import importlib.util
spec = importlib.util.spec_from_file_location("vh_mod", "/home/wubu/hermes-agent-dev/agent/verify_hooks.py")
mod = importlib.util.module_from_spec(spec)
utils_mod = types.ModuleType("utils")
utils_mod.is_truthy_value = lambda v, default=False: (
    default if v is None else v if isinstance(v, bool) else
    (str(v).strip().lower() in {"1","true","yes","on"}) if isinstance(v, str) else bool(v))
sys.modules["utils"] = utils_mod
try:
    spec.loader.exec_module(mod)
except Exception as e:
    print("IMPORT_FAIL", repr(e)); sys.exit(2)

mm = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    b = json.loads(line)
    cfg = None if b["cfg"] == "null" else json.loads(b["cfg"])
    got = b["out"]  # native: None or str
    if b["t"] == "nudges":
        exp = mod.max_verify_nudges(cfg)
        if exp != got: mm += 1; print(f"MISMATCH nudges {b['cfg']}: exp {exp!r} got {got!r}")
    elif b["t"] == "guidance":
        exp = mod.coding_verify_guidance(cfg)
        if exp != got: mm += 1; print(f"MISMATCH guidance {b['cfg']}: exp {exp!r} got {got!r}")
if mm:
    print(f"oracle: {mm} mismatch(es)"); sys.exit(1)
print("oracle: 0 mismatches")
