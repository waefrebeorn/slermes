#!/usr/bin/env python3
"""Oracle for tools/memory_tool.py:check_memory_requirements.
Stubs get_builtin_memory_store_flags with the two passed flags."""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/tools/memory_tool.py"
ns = {"object": object}
src = open(SRC, encoding="utf-8").read()
for n in ast.parse(src).body:
    if isinstance(n, ast.FunctionDef) and n.name == "check_memory_requirements":
        n.decorator_list = []  # strip decorators with undefined names
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
live = ns["check_memory_requirements"]
ns["_memory_surface_flags"] = type("C", (), {"set": staticmethod(lambda v: None)})()

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    flags = (bool(r["memory_enabled"]), bool(r["user_profile_enabled"]))
    ns["get_builtin_memory_store_flags"] = lambda _f=flags: _f
    py = bool(live())
    c = bool(r["out"])
    cases += 1
    if py != c:
        mism += 1
        print(f"MISMATCH {r} py={py} c={c}", file=sys.stderr)
print(f"ORACLE memory_available: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
