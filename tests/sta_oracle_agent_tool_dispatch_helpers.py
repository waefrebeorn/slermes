#!/usr/bin/env python3
"""Oracle: agent/tool_dispatch_helpers.py mutation helpers vs LIVE Python.
Newline-delimited JSON."""
import json, sys, os, types
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import importlib.util
spec = importlib.util.spec_from_file_location("tdh_mod", "/home/wubu/hermes-agent-dev/agent/tool_dispatch_helpers.py")
mod = importlib.util.module_from_spec(spec)
m = types.ModuleType("agent.tool_result_classification"); m.FILE_MUTATING_TOOL_NAMES = {"write_file", "patch"}
sys.modules["agent.tool_result_classification"] = m
try:
    spec.loader.exec_module(mod)
except Exception as e:
    print("IMPORT_FAIL", repr(e)); sys.exit(2)

PATCH = "*** Update File: src/a.py\n*** Add File: src/b.py\n*** Delete File: src/c.py\n"
RESULT = '{"success": true, "files_modified": ["src/x.py", "src/y.py"]}'

mm = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    b = json.loads(line)
    if b["t"] == "neutralize":
        exp = mod._neutralize_delimiters(b["in"])
        if exp != b["out"]: mm += 1; print(f"MISMATCH neutralize:\nEXP:{exp!r}\nGOT:{b['out']!r}")
    elif b["t"] == "landed_write":
        exp = mod._extract_landed_file_mutation_paths("write_file", {"path": "/tmp/a.txt", "mode": "replace"}, None)
        if exp != b["paths"]: mm += 1; print(f"MISMATCH landed_write: exp {exp} got {b['paths']}")
    elif b["t"] == "landed_patch":
        exp = mod._extract_landed_file_mutation_paths("patch", {"mode": "patch", "patch": PATCH}, None)
        if exp != b["paths"]: mm += 1; print(f"MISMATCH landed_patch: exp {exp} got {b['paths']}")
    elif b["t"] == "landed_result":
        exp = mod._extract_landed_file_mutation_paths("patch", {"mode": "replace"}, RESULT)
        if exp != b["paths"]: mm += 1; print(f"MISMATCH landed_result: exp {exp} got {b['paths']}")
if mm:
    print(f"oracle: {mm} mismatch(es)"); sys.exit(1)
print("oracle: 0 mismatches")
