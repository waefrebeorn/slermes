#!/usr/bin/env python3
"""Oracle for tools/web_result_cache.py:bucket_limit."""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/tools/web_result_cache.py"
ns = {"int": int}
src = open(SRC, encoding="utf-8").read()
for n in ast.parse(src).body:
    if isinstance(n, ast.Assign) and any(getattr(x, "id", "") == "_LIMIT_BUCKETS" for x in n.targets):
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    if isinstance(n, ast.FunctionDef) and n.name == "bucket_limit":
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
live = ns["bucket_limit"]

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    py = int(live(r["limit"]))
    cases += 1
    if py != r["out"]:
        mism += 1
        print(f"MISMATCH limit={r['limit']} py={py} c={r['out']}", file=sys.stderr)
print(f"ORACLE bucket_limit: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
