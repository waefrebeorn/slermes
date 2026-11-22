#!/usr/bin/env python3
"""Oracle for hermes_cli/update_abort_recovery.py:_abort_recovery_is_complete.
AST-extracts LIVE helper + _owed_stale_serve_rows, strips the in-body import
and seeds CLIENT_OWNED_SERVE_SUPERVISORS from update_inventory.py."""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/update_abort_recovery.py"
ns = {"bool": bool, "set": set, "dict": dict, "frozenset": frozenset,
      "CLIENT_OWNED_SERVE_SUPERVISORS": frozenset({"desktop", "desktop-ssh"})}
src = open(SRC, encoding="utf-8").read()
for n in ast.parse(src).body:
    if isinstance(n, ast.FunctionDef) and n.name in (
            "_owed_stale_serve_rows", "_abort_recovery_is_complete"):
        n.decorator_list = []
        n.body = [b for b in n.body if not isinstance(b, (ast.Import, ast.ImportFrom))]
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
live = ns["_abort_recovery_is_complete"]

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    py = bool(live(planned_gateway_profiles=r.get("planned"),
                   covered_gateway_profiles=r.get("covered"),
                   recovery_result=r.get("result"),
                   stale_runtime_rows=r.get("stale")))
    c = bool(r["out"])
    cases += 1
    if py != c:
        mism += 1
        print(f"MISMATCH {r} py={py} c={c}", file=sys.stderr)
print(f"ORACLE abort_complete: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
