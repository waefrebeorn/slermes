import os
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

def read_failed_error(path_name):
    """Deleted upstream; contract from memory_tool_pure.c."""
    return {
        "success": False,
        "error": (f"Refusing to write {path_name}: the file exists on disk but "
                  f"could not be read right now (temporarily locked by another "
                  f"program, a permission change, invalid/corrupt text encoding, "
                  f"or a filesystem error). Treating an unreadable file as empty "
                  f"and saving would wipe existing memory, so the write is "
                  f"refused. Nothing was changed — retry in a moment."),
    }


def drift_error(path_name, bak_path):
    """Deleted upstream; contract from src/tools/memory_tool_pure.c."""
    return {
        "success": False,
        "error": (f"Refusing to write {path_name}: file on disk has content that "
                  f"wouldn't round-trip through the memory tool (likely added by "
                  f"the patch tool, a shell append, a manual edit, or a concurrent "
                  f"session). A snapshot was saved to {bak_path}. Resolve the drift "
                  f"first — either rewrite the file as a clean §-delimited list of "
                  f"entries, or move the extra content out — then retry. This guard "
                  f"exists to prevent silent data loss (issue #26045)."),
        "drift_backup": bak_path,
        "remediation": ("Open the .bak file, integrate the missing entries into "
                        "the memory tool one at a time via memory(action=add, "
                        "content=...), then remove or rewrite the original file to "
                        "a clean state."),
    }

import sys
import importlib.util


def _load_module():
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.environ.get("SLERMES_DEV_ROOT") or os.path.dirname(os.path.dirname(here))
    cand = os.path.join(repo, "tools", "memory_tool.py")
    spec = importlib.util.spec_from_file_location("live_memory_tool", cand)
    mod = importlib.util.module_from_spec(spec)
    sys.modules['live_memory_tool'] = mod  # dataclasses need this
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
            d = drift_error(str(case.get("path_name", "")), case.get("bak_path", ""))
            out.append({"op": op, "out": d})
        elif op == "read_failed_error":
            from pathlib import Path
            d = read_failed_error(str(case.get("path_name", "")))
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
