#!/usr/bin/env python3
"""Oracle: hermes_cli/default_soul.py vs LIVE Python."""
import json, sys, os, types
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import importlib.util
spec = importlib.util.spec_from_file_location("ds_mod", "/home/wubu/hermes-agent-dev/hermes_cli/default_soul.py")
mod = importlib.util.module_from_spec(spec)
try:
    spec.loader.exec_module(mod)
except Exception as e:
    print("IMPORT_FAIL", repr(e)); sys.exit(2)

# the exact legacy strings from the module
LEG0 = mod._LEGACY_TEMPLATE_SOULS[0]
LEG1 = mod._LEGACY_TEMPLATE_SOULS[1]

mm = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    b = json.loads(line)
    name = b["name"]
    if name == "legacy0": text = LEG0
    elif name == "legacy1": text = LEG1
    elif name == "legacy0_ws": text = "  \r\n" + LEG0 + "\r\n  "
    elif name == "legacy0_bom": text = "\ufeff" + LEG0 + "\n"
    elif name == "custom": text = "You are a helpful assistant with a custom persona."
    elif name == "empty": text = ""
    else: text = ""
    exp = bool(mod.is_legacy_template_soul(text))
    if exp != bool(b["out"]): mm += 1; print(f"MISMATCH {name}: exp {exp} got {b['out']}")
if mm:
    print(f"oracle: {mm} mismatch(es)"); sys.exit(1)
print("oracle: 0 mismatches")
