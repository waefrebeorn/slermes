#!/usr/bin/env python3
"""sta_oracle_memory_tool_pure.py — oracle twin for memory_tool_pure.c.

Calls the LIVE tools/memory_tool.py pure helpers and emits one JSON line
per op. The C harness (t_port_memory_tool_pure.c) emits the same shape.

Fixture shape:
{
  "ops": [
    {"op": "drift_error", "path_name": "MEMORY.md", "bak_path": ".bak.123"},
    {"op": "read_failed_error", "path_name": "USER.md"},
    {"op": "consolidation", "response": {"success": true}, "calls": 4}
  ]
}
get_memory_dir is environment-dependent (HERMES_HOME) — covered by a
best-effort string-prefix check, but not strict-diffed.
"""
import json
import os
import sys
import importlib.util


def _load_module():
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(os.path.dirname(os.path.dirname(here)))
    cand = os.path.join(repo, "tools", "memory_tool.py")
    spec = importlib.util.spec_from_file_location("live_memory_tool", cand)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main():
    path = sys.argv[1]
    with open(path, encoding="utf-8") as f:
        spec = json.load(f)
    mod = _load_module()
    out = []
    for case in spec.get("ops", []):
        op = case.get("op")
        if op == "drift_error":
            from pathlib import Path
            d = mod._drift_error(Path(case.get("path_name", "")), case.get("bak_path", ""))
            out.append({"op": op, "out": d})
        elif op == "read_failed_error":
            from pathlib import Path
            d = mod._read_failed_error(Path(case.get("path_name", "")))
            out.append({"op": op, "out": d})
        elif op == "consolidation":
            store = mod.MemoryStore()
            resp = case.get("response", {})
            calls = case.get("calls", 1)
            last = None
            for _ in range(calls):
                last = store._consolidation_failure(resp)
            out.append({"op": op, "out": last})
    for line in out:
        sys.stdout.write(json.dumps(line, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
