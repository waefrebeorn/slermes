#!/usr/bin/env python3
# coverage-targets: get_hermes_home_override
"""LIVE oracle for hermes_constants.py:get_hermes_home_override."""
import ast
import json
import sys
import types

SRC = "C:/Users/eman5/hermes-agent/hermes_constants.py"
t = ast.parse(open(SRC, encoding="utf-8").read())
fn = next(n for n in t.body if isinstance(n, ast.FunctionDef)
          and n.name == "get_hermes_home_override")
fn.decorator_list = []

modsys = types.ModuleType("sys")
scope = types.ModuleType("hermes_scope")
state = types.SimpleNamespace(home=None)
scope.current_state = lambda: state
modsys.modules = {"hermes_scope": scope}
NS = {"sys": modsys}
exec(compile(ast.Module(body=[fn], type_ignores=[]), SRC, "exec"), NS)

cases = mism = 0
for line in sys.stdin:
    if '"out"' not in line:
        continue
    r = json.loads(line)
    state.home = r.get("home") or None
    py = NS["get_hermes_home_override"]()
    got = r["out"] or None
    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH in={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE hermes_home_override: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)

