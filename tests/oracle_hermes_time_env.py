#!/usr/bin/env python3
# coverage-targets: _env_timezone
"""LIVE oracle for hermes_time.py:_env_timezone."""
import ast
import json
import os
import sys
import types

SRC = "C:/Users/eman5/hermes-agent/hermes_time.py"
t = ast.parse(open(SRC, encoding="utf-8").read())
fn = next(n for n in t.body if isinstance(n, ast.FunctionDef)
          and n.name == "_env_timezone")
fn.decorator_list = []

secret_scope = types.ModuleType("agent.secret_scope")
NS = {"os": os}
exec(compile(ast.Module(body=[fn], type_ignores=[]), SRC, "exec"), NS)

cases = mism = 0
for line in sys.stdin:
    if '"out"' not in line:
        continue
    r = json.loads(line)
    for k in ("HERMES_TIMEZONE",):
        os.environ.pop(k, None)
    if r.get("tz"):
        os.environ["HERMES_TIMEZONE"] = r["tz"]
    secret_scope.is_multiplex_active = lambda: bool(r.get("multiplex"))
    sys.modules["agent.secret_scope"] = secret_scope
    py = NS["_env_timezone"]()
    got = r["out"]
    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH in={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE hermes_time_env: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
