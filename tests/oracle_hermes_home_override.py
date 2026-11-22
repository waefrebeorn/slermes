#!/usr/bin/env python3
# coverage-targets: get_hermes_home_override, set_hermes_home_override, reset_hermes_home_override
"""LIVE oracle for hermes_constants.py:get_hermes_home_override."""
import ast
import json
import sys
import types

SRC = "C:/Users/eman5/hermes-agent/hermes_constants.py"
t = ast.parse(open(SRC, encoding="utf-8").read())
fns = [n for n in t.body if isinstance(n, ast.FunctionDef)
       and n.name in ("get_hermes_home_override", "set_hermes_home_override",
                      "reset_hermes_home_override")]
for fn in fns: fn.decorator_list = []

modsys = types.ModuleType("sys")
scope = types.ModuleType("hermes_scope")
state = types.SimpleNamespace(home=None)
scope.current_state = lambda: state
class Tok:
    def __init__(self, old): self.old = old
    def reset(self): state.home = self.old
scope.current_scope = lambda: None
def push_state(home):
    old = state.home
    state.home = home
    return Tok(old)
scope.push_state = push_state
modsys.modules = {"hermes_scope": scope}
sys.modules["hermes_scope"] = scope
NS = {"sys": modsys}
exec(compile(ast.Module(body=fns, type_ignores=[]), SRC, "exec"), NS)

cases = mism = 0
for line in sys.stdin:
    if '"out"' not in line:
        continue
    r = json.loads(line)
    h = r.get("home")
    state.home = r.get("home") or None
    py = NS["get_hermes_home_override"]()
    got = r["out"] or None
    state.home = None
    tok = NS["set_hermes_home_override"](h if h else None)
    NS["reset_hermes_home_override"](tok)
    py_after = NS["get_hermes_home_override"]()
    got_after = r.get("after_reset") or None
    cases += 1
    if py_after != got_after:
        mism += 1
        print(f"MISMATCH reset in={r} py={py_after!r} c={got_after!r}", file=sys.stderr)
    if py != got:
        mism += 1
        print(f"MISMATCH in={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE hermes_home_override: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)

