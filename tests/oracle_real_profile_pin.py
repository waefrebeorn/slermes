#!/usr/bin/env python3
"""Oracle for hermes_cli/browser_connect.py:_real_profile_pin.
Stub _browser_setting to return the passed JSON value's Python equivalent."""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/browser_connect.py"
ns = {"str": str, "isinstance": isinstance, "object": object}
src = open(SRC, encoding="utf-8").read()
for n in ast.parse(src).body:
    if isinstance(n, ast.FunctionDef) and n.name == "_real_profile_pin":
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
live = ns["_real_profile_pin"]

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    v = r.get("value")
    ns["_browser_setting"] = lambda _k, _v=v: _v
    py = live()
    c = r["out"]
    cases += 1
    if py != c:
        mism += 1
        print(f"MISMATCH {r} py={py!r} c={c!r}", file=sys.stderr)
print(f"ORACLE real_profile_pin: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
