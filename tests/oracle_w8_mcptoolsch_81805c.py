#!/usr/bin/env python3
"""Differential oracle for tools/mcp_tool_schema.py (gate w8_mcptoolsch_81805c)."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
SRC = Q + "/tools/mcp_tool_schema.py"
spec = importlib.util.spec_from_file_location("live_schema_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["live_schema_live"] = live
spec.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    if f == "sanitize":
        return live.sanitize_mcp_name_component(r.get("value", ""))
    if f == "prefixed":
        return live.mcp_prefixed_tool_name(r.get("server", ""), r.get("tool", ""))
    if f == "matches":
        pats = live._normalize_name_filter(r.get("patterns", []), "filter")
        return live.matches_name_filter(r.get("tool_name", ""), pats)
    if f == "rewrite":
        return live._rewrite_local_refs(r.get("node"))
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
print("ORACLE w8_mcptoolsch_81805c: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
