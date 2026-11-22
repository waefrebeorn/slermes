#!/usr/bin/env python3
"""Oracle for hermes_cli/mcp_startup.py:set_mcp_server_filter.
AST-extracts the LIVE function (module-global is a plain stub)."""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/mcp_startup.py"
ns = {"str": str, "list": list, "tuple": tuple, "set": set, "object": object,
      "Optional": object}
src = open(SRC, encoding="utf-8").read()
for n in ast.parse(src).body:
    if isinstance(n, ast.FunctionDef) and n.name == "set_mcp_server_filter":
        n.decorator_list = []
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
live = ns["set_mcp_server_filter"]

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    ns["_mcp_server_filter"] = None
    py = live(r.get("toolsets"))
    c = r.get("out")
    cases += 1
    if py != c:
        mism += 1
        print(f"MISMATCH {r} py={py} c={c}", file=sys.stderr)
print(f"ORACLE mcp_filter: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
