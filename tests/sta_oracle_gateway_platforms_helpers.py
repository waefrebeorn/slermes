#!/usr/bin/env python3
"""Oracle: gateway/platforms/helpers.py table helpers vs LIVE Python.
Reads newline-delimited JSON; compares each line vs live Python."""
import json, sys, os, types
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
sys.path.insert(0, os.path.join("/home/wubu/hermes-agent-dev", "gateway"))
utils_mod = types.ModuleType("utils"); utils_mod.atomic_json_write = lambda *a, **k: None
sys.modules["utils"] = utils_mod
import importlib.util
spec = importlib.util.spec_from_file_location("gh_helpers", os.path.join("/home/wubu/hermes-agent-dev","gateway/platforms/helpers.py"))
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
    t = b["t"]
    if t == "is_table_row":
        exp = bool(mod.is_table_row(b["in"])); got = bool(b["out"])
        if exp != got: mm += 1; print(f"MISMATCH is_table_row {b['in']!r}: exp {exp} got {got}")
    elif t == "split_row":
        exp = mod.split_markdown_table_row(b["in"]); got = b["cells"]
        if exp != got: mm += 1; print(f"MISMATCH split_row {b['in']!r}: exp {exp} got {got}")
    elif t == "convert":
        exp = mod.convert_table_to_bullets(b["in"]).replace("\r\n","\n").rstrip("\n")
        got = b["out"].replace("\\n","\n")
        if exp != got: mm += 1; print(f"MISMATCH convert:\nEXP:\n{exp}\nGOT:\n{got}")
    elif t == "convert_notable":
        exp = mod.convert_table_to_bullets(b["in"]).replace("\r\n","\n").rstrip("\n")
        got = b["out"].replace("\\n","\n")
        if exp != got: mm += 1; print(f"MISMATCH convert_notable:\nEXP:\n{exp}\nGOT:\n{got}")
if mm:
    print(f"oracle: {mm} mismatch(es)"); sys.exit(1)
print("oracle: 0 mismatches")
