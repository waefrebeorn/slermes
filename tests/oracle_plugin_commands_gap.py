# coverage-targets: get_plugin_commands, get_plugin_auxiliary_tasks
#!/usr/bin/env python3
"""LIVE oracle for hermes_cli/plugins.py:get_plugin_commands facade."""
import ast
import json
import sys
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
SRC = "C:/Users/eman5/hermes-agent/hermes_cli/plugins.py"
NS = {}
tree = ast.parse(open(SRC, encoding="utf-8").read())
TARGETS = ("get_plugin_commands", "get_plugin_auxiliary_tasks")
fns = [n for n in tree.body if isinstance(n, ast.FunctionDef)
       and n.name in TARGETS]
for f in fns:
    f.decorator_list = []
exec(compile(ast.Module(body=fns, type_ignores=[]), SRC, "exec"), NS)
live = NS["get_plugin_commands"]
live_aux = NS["get_plugin_auxiliary_tasks"]

cases = mism = 0
for line in sys.stdin:
    if '"out"' not in line:
        continue
    r = json.loads(line)
    state = r["state"]
    cmd = json.loads(json.dumps(state.get("plugin_commands") or {}))
    NS["_ensure_plugins_discovered"] = lambda: SimpleNamespace(
        _plugin_commands=cmd,
        _aux_tasks=json.loads(json.dumps(state.get("aux_tasks") or {})))
    want = live()
    got = r["out"]
    want_aux = live_aux()
    got_aux = r.get("aux")
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH state={state} want={want!r} got={got!r}", file=sys.stderr)
    if want_aux != got_aux:
        mism += 1
        print(f"MISMATCH aux state={state} want={want_aux!r} got={got_aux!r}", file=sys.stderr)
print(f"ORACLE plugin_commands_gap: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
