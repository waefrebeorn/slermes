# coverage-targets: get_plugin_commands
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
fn = next(n for n in tree.body
          if isinstance(n, ast.FunctionDef) and n.name == "get_plugin_commands")
fn.decorator_list = []
exec(compile(ast.Module(body=[fn], type_ignores=[]), SRC, "exec"), NS)
live = NS["get_plugin_commands"]

cases = mism = 0
for line in sys.stdin:
    if '"out"' not in line:
        continue
    r = json.loads(line)
    state = r["state"]
    cmd = json.loads(json.dumps(state.get("plugin_commands") or {}))
    NS["_ensure_plugins_discovered"] = lambda: SimpleNamespace(_plugin_commands=cmd)
    want = live()
    got = r["out"]
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH state={state} want={want!r} got={got!r}", file=sys.stderr)
print(f"ORACLE plugin_commands_gap: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
