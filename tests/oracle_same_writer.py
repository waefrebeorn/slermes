#!/usr/bin/env python3
"""Oracle for hermes_cli/active_sessions.py:_is_same_writer.
AST-extracts the LIVE helper, stubs os.getpid() with the harness-emitted pid."""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/active_sessions.py"
ns = {"str": str, "int": int, "bool": bool, "dict": dict,
      "Optional": object, "Any": object}
src = open(SRC, encoding="utf-8").read()
for n in ast.parse(src).body:
    if isinstance(n, ast.FunctionDef) and n.name == "_is_same_writer":
        n.decorator_list = []
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
live = ns["_is_same_writer"]

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    ns["os"] = type("O", (), {"getpid": staticmethod(lambda _p=r["pid"]: _p)})()
    py = 1 if live(r.get("entry"), r.get("metadata")) else 0
    c = r["out"]
    cases += 1
    if py != c:
        mism += 1
        print(f"MISMATCH {r} py={py} c={c}", file=sys.stderr)
print(f"ORACLE same_writer: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
