#!/usr/bin/env python3
"""Oracle for agent/auxiliary_managed_local.py:_is_managed_local_endpoint.
AST-extracts the LIVE helper, stubs _managed_local_netloc with the passed value."""
import ast
import json
import sys
from urllib.parse import urlparse

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/auxiliary_managed_local.py"
ns = {"str": str, "Optional": object, "urlparse": urlparse}
src = open(SRC, encoding="utf-8").read()
for n in ast.parse(src).body:
    if isinstance(n, ast.FunctionDef) and n.name == "_is_managed_local_endpoint":
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
live = ns["_is_managed_local_endpoint"]

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    url, managed = r.get("url"), r.get("managed")
    ns["_managed_local_netloc"] = lambda _m=managed: _m
    py = bool(live(url))
    c = bool(r["out"])
    cases += 1
    if py != c:
        mism += 1
        print(f"MISMATCH {r} py={py} c={c}", file=sys.stderr)
print(f"ORACLE managed_local_endpoint: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
