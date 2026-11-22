#!/usr/bin/env python3
"""Oracle for cron/scheduler_preflight.py:_delivery_platform_routed_from_primary_gateway.
Stub _primary_profile_routes_for_current_home to return objects with .platform
set to each element of the passed routes array."""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/cron/scheduler_preflight.py"
ns = {"str": str, "any": any}
src = open(SRC, encoding="utf-8").read()
for n in ast.parse(src).body:
    if isinstance(n, ast.FunctionDef) and n.name == "_delivery_platform_routed_from_primary_gateway":
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
live = ns["_delivery_platform_routed_from_primary_gateway"]

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    routes = r.get("routes") or []
    ns["_primary_profile_routes_for_current_home"] = lambda _rs=routes: [
        type("R", (), {"platform": x})() for x in _rs]
    py = bool(live(r.get("platform", "")))
    c = bool(r["out"])
    cases += 1
    if py != c:
        mism += 1
        print(f"MISMATCH {r} py={py} c={c}", file=sys.stderr)
print(f"ORACLE delivery_platform_routed: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
