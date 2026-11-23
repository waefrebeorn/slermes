#!/usr/bin/env python3
"""Differential oracle for tools/mcp_tool_agent.py (gate w7_mcptoolage_9bc44f)."""
import importlib.util
import json
import sys
import types

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

import tools  # noqa: F401  (parent-package bootstrap)
spec = importlib.util.spec_from_file_location("tools.mcp_tool_agent", Q + "/tools/mcp_tool_agent.py")
live = importlib.util.module_from_spec(spec)
sys.modules["tools.mcp_tool_agent"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    try:
        if func == "def_name":
            py = live._def_name(r.get("tool_def", {}))
        elif func == "agent_tool_names":
            py = live.agent_tool_names(types.SimpleNamespace(tools=r.get("tools", [])))
        elif func == "merge":
            merged, names = live._merge_preserving_prefix(
                r.get("current_defs", []), r.get("new_defs", []),
                set(r.get("registered_names", [])), set(r.get("held", [])))
            py = {"merged": merged, "names": sorted(names)}
        else:
            continue
    except Exception as exc:
        print("LIVE ERROR %s %r: %r" % (func, r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w7_mcptoolage_9bc44f: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
