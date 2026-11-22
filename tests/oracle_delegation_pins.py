#!/usr/bin/env python3
# coverage-targets: explicit_board_intent_is_pinned
"""LIVE oracle for agent/delegation_context.py:explicit_board_intent_is_pinned."""
import ast
import json
import os
import sys

SRC = "C:/Users/eman5/hermes-agent/agent/delegation_context.py"
s = open(SRC, encoding="utf-8").read()
t = ast.parse(s)
fn = next(n for n in t.body if isinstance(n, ast.FunctionDef)
          and n.name == "explicit_board_intent_is_pinned")
fn.decorator_list = []
NS = {"os": os, "bool": bool,
      "DELEGATED_CHILD_ENV_MARKER": "HERMES_DELEGATED_CHILD_CONTEXT"}
class Ctx:
    def __init__(self): self.v = 0
    def get(self): return self.v
ctx = Ctx()
NS["_DELEGATED_CHILD_CONTEXT"] = ctx
exec(compile(ast.Module(body=[fn], type_ignores=[]), SRC, "exec"), NS)
live = NS["explicit_board_intent_is_pinned"]

keys = ("HERMES_DELEGATED_CHILD_CONTEXT", "HERMES_KANBAN_TASK")
cases = mism = 0
for line in sys.stdin:
    if '"out"' not in line:
        continue
    r = json.loads(line)
    env = r.get("env") or {}
    for k in keys:
        os.environ.pop(k, None)
    for k, v in env.items():
        if v is not None:
            os.environ[k] = str(v)
    ctx.v = r.get("in_process", 0)
    py = 1 if live() else 0
    got = r["out"]
    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH in={r} py={py} c={got}", file=sys.stderr)
print(f"ORACLE delegation_pins: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
