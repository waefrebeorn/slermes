#!/usr/bin/env python3
"""Oracle for agent/prompt_builder.py:_get_context_file_read_timeout."""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/prompt_builder.py"
ns = {"int": int, "float": float}
src = open(SRC, encoding="utf-8").read()
for n in ast.parse(src).body:
    if isinstance(n, ast.Assign) and any(getattr(x, "id", "") == "_CONTEXT_FILE_READ_TIMEOUT_SECS" for x in n.targets):
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    if isinstance(n, ast.FunctionDef) and n.name == "_get_context_file_read_timeout":
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
live = ns["_get_context_file_read_timeout"]

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    val = r.get("val")
    ns["_config_readonly"] = lambda _key, _v=val: {"context_file_read_timeout": _v}
    py = float(live())
    c = float(r["out"])
    cases += 1
    if py != c:
        mism += 1
        print(f"MISMATCH val={val!r} py={py} c={c}", file=sys.stderr)
print(f"ORACLE get_context_timeout: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
