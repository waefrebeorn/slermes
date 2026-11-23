#!/usr/bin/env python3
"""Differential oracle for tools/mcp_tool_common.py (gate w8_mcptoolcom_f3c09b)."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
SRC = Q + "/tools/mcp_tool_common.py"
spec = importlib.util.spec_from_file_location("live_common_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["live_common_live"] = live
spec.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    if f == "env_ref_name":
        return live._env_ref_name(r.get("ref", ""))
    if f == "safe_numeric":
        coerce = int if r.get("coerce", "int") != "float" else float
        return live._safe_numeric(r.get("value"), r.get("default", 0),
                                  coerce=coerce, minimum=r.get("minimum", 1))
    if f == "parse_boolish":
        return live._parse_boolish(r.get("value"), default=r.get("default", True))
    if f == "mcp_server_enabled":
        return live.mcp_server_enabled(r.get("cfg", {}))
    if f == "get_lifecycle_seconds":
        return live._get_lifecycle_seconds(r.get("cfg", {}), r.get("key", ""))
    if f == "prepend_path":
        return live._prepend_path(r.get("env", {}), r.get("directory", ""))
    return None

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception:
        py = None
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w8_mcptoolcom_f3c09b: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
