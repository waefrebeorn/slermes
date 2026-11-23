#!/usr/bin/env python3
"""Differential oracle for tools/kanban_toolset_context.py (gate kantool)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/tools/kanban_toolset_context.py"
spec = importlib.util.spec_from_file_location("kantool_live", SRC)
live = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    got = r.get("out")
    if func == "selection":
        ts = r.get("toolsets")
        want = "kanban" in (ts or ())
    elif func == "scoped":
        ts = r.get("toolsets")
        before = live.kanban_toolset_requested()
        with live.scoped_kanban_toolset_selection(ts):
            inside = live.kanban_toolset_requested()
        after = live.kanban_toolset_requested()
        want = {"outside": before, "inside": inside, "after": after}
    elif func == "requested":
        want = live.kanban_toolset_requested()
    else:
        continue
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH in={r} py={want!r} c={got!r}", file=sys.stderr)
print(f"ORACLE kantool: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
