#!/usr/bin/env python3
"""Differential oracle for tools/mcp_tool_node_abi.py (gate w10_mcptoolno_477ad2)."""
import importlib.util
import json
import sys
import types
from pathlib import Path

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w10_mcptoolno_477ad2"
sys.path.insert(0, QUARRY)

SRC = QUARRY + "/tools/mcp_tool_node_abi.py"
_spec = importlib.util.spec_from_file_location("mcp_tool_node_abi_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["mcp_tool_node_abi_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "package_of":
        root, package = live._package_of(rec.get("path") or "")
        return {"root": str(root) if root is not None else None, "package": package}
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") != "package_of":
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} func={rec.get('func')} "
                  f"C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
