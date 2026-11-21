#!/usr/bin/env python3
"""Oracle for hermes_cli/nous_auth_keepalive.py:_refresh_horizon_seconds."""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/nous_auth_keepalive.py"
ns = {"int": int}
src = open(SRC, encoding="utf-8").read()
for n in ast.parse(src).body:
    if isinstance(n, ast.FunctionDef) and n.name == "_refresh_horizon_seconds":
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
live = ns["_refresh_horizon_seconds"]
# SKEW is imported from auth_constants.py: 120
ns["ACCESS_TOKEN_REFRESH_SKEW_SECONDS"] = 120

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    py = int(live(r["tick"], r["floor"]))
    cases += 1
    if py != r["out"]:
        mism += 1
        print(f"MISMATCH {r} py={py} c={r['out']}", file=sys.stderr)
print(f"ORACLE refresh_horizon: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
