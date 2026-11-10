#!/usr/bin/env python3
"""Oracle for gateway/session_connections.py session_owned.

Dicts and enums are importable directly (session_connections imports pydantic at
module level though), so ast-extract the live def and run it on the harness 'in'.
"""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8")
SRC = "C:/Users/eman5/hermes-agent/gateway/session_connections.py"

ns = {}
tree = ast.parse(open(SRC, encoding="utf-8").read())
for n in tree.body:
    if isinstance(n, ast.FunctionDef) and n.name == "session_owned":
        exec(compile(ast.Module(body=[n], type_ignores=[]), "<x>", "exec"), ns)
session_owned = ns["session_owned"]

passes = 0
fails = []
total = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        c = json.loads(line)
    except json.JSONDecodeError:
        continue
    total += 1
    got = 1 if session_owned(c["in"]) else 0
    if got == c["out"]:
        passes += 1
    else:
        fails.append((c["in"], got, c["out"]))
print("pass/total: %d/%d" % (passes, total))
for f in fails[:8]:
    print("FAIL", f)
if fails:
    print("ORACLE session_owned: %d cases, %d mismatches" % (total, len(fails)))
    sys.exit(1)
print("ORACLE session_owned: %d cases, 0 mismatches" % total)
