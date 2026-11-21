#!/usr/bin/env python3
"""Oracle for hermes_cli/browser_connect.py:_real_profile_autoclose."""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/browser_connect.py"
ns = {"bool": bool}
src = open(SRC, encoding="utf-8").read()
for n in ast.parse(src).body:
    if isinstance(n, ast.FunctionDef) and n.name == "_real_profile_autoclose":
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
live = ns["_real_profile_autoclose"]

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    val = r.get("setting")
    ns["_browser_setting"] = lambda _k, _v=val: _v
    py = int(bool(live()))
    cases += 1
    if py != r["out"]:
        mism += 1
        print(f"MISMATCH setting={val!r} py={py} c={r['out']}", file=sys.stderr)
print(f"ORACLE autoclose: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
