#!/usr/bin/env python3
"""Oracle: gateway/drain_control.py vs LIVE Python (newline-delimited JSON)."""
import json, sys, os, types
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import importlib.util
spec = importlib.util.spec_from_file_location("dc_mod", "/home/wubu/hermes-agent-dev/gateway/drain_control.py")
mod = importlib.util.module_from_spec(spec)
utils_mod = types.ModuleType("utils")
import json as _json
def _atomic_json_write(path, data, indent=None):
    import os
    tmp = str(path) + ".tmp"
    with open(tmp, "w") as f:
        _json.dump(data, f)
    os.replace(tmp, str(path))
utils_mod.atomic_json_write = _atomic_json_write
sys.modules["utils"] = utils_mod
try:
    spec.loader.exec_module(mod)
except Exception as e:
    print("IMPORT_FAIL", repr(e)); sys.exit(2)

HOME = os.environ.get("SLERMES_HOME")
if HOME is None: HOME = "."
mod.clear_drain_request(home=__import__("pathlib").Path(HOME))

mm = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    b = json.loads(line)
    home = __import__("pathlib").Path(HOME)
    if b["t"] == "read_absent":
        exp = mod.read_drain_request(home=home)
        got = b["out"]
        if exp != got: mm += 1; print(f"MISMATCH read_absent: exp {exp!r} got {got!r}")
    elif b["t"] == "write":
        exp = mod.write_drain_request(home=home)
        got = b["out"]
        # compare key fields, ignore requested_at
        ej = exp if isinstance(exp, dict) else json.loads(exp)
        gj = json.loads(got)
        ej.pop("requested_at", None); gj.pop("requested_at", None)
        if ej != gj: mm += 1; print(f"MISMATCH write:\nEXP:{ej}\nGOT:{gj}")
    elif b["t"] == "read_present":
        exp = mod.read_drain_request(home=home)
        got = b["out"]
        if exp is None or got is None:
            if (exp is None) != (got is None): mm += 1; print(f"MISMATCH read_present: exp {exp!r} got {got!r}")
        else:
            ej = exp if isinstance(exp, dict) else json.loads(exp)
            gj = json.loads(got)
            ej.pop("requested_at", None); gj.pop("requested_at", None)
            if ej != gj: mm += 1; print(f"MISMATCH read_present: exp {ej} got {gj}")
    elif b["t"] == "clear":
        exp = bool(mod.clear_drain_request(home=home)); got = bool(b["out"])
        if exp != got: mm += 1; print(f"MISMATCH clear: exp {exp} got {got}")
    elif b["t"] == "clear_again":
        exp = bool(mod.clear_drain_request(home=home)); got = bool(b["out"])
        if exp != got: mm += 1; print(f"MISMATCH clear_again: exp {exp} got {got}")
if mm:
    print(f"oracle: {mm} mismatch(es)"); sys.exit(1)
print("oracle: 0 mismatches")
