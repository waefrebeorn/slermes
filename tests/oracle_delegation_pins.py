#!/usr/bin/env python3
# coverage-targets: explicit_board_intent_is_pinned, kanban_path_is_fenced
"""LIVE oracle for agent/delegation_context.py:explicit_board_intent_is_pinned."""
import ast
import json
import os
import sys

SRC = "C:/Users/eman5/hermes-agent/agent/delegation_context.py"
s = open(SRC, encoding="utf-8").read()
t = ast.parse(s)
fns = [n for n in t.body if isinstance(n, ast.FunctionDef)
       and n.name in ("explicit_board_intent_is_pinned", "kanban_path_is_fenced")]
for fn in fns:
    fn.decorator_list = []
NS = {"os": os, "bool": bool,
      "DELEGATED_CHILD_ENV_MARKER": "HERMES_DELEGATED_CHILD_CONTEXT"}
class Ctx:
    def __init__(self): self.v = 0
    def get(self): return self.v
ctx = Ctx()
NS["_DELEGATED_CHILD_CONTEXT"] = ctx
class Path:
    def __init__(self, p):
        if p.startswith("~/"):
            p = os.path.expanduser(p)
        parts = []
        for seg in p.split("/"):
            if seg in ("", "."): continue
            if seg == "..":
                if parts: parts.pop()
                continue
            parts.append(seg)
        self.p = "/" + "/".join(parts)
    def expanduser(self): return self
    def resolve(self): return self
    def relative_to(self, other):
        if self.p.startswith(other.p.rstrip("/") + "/"):
            return self
        raise ValueError("not relative")
    def __eq__(self, other): return isinstance(other, Path) and self.p == other.p

NS["Path"] = Path
exec(compile(ast.Module(body=fns, type_ignores=[]), SRC, "exec"), NS)
live = NS["explicit_board_intent_is_pinned"]
live_fence = NS["kanban_path_is_fenced"]

keys = ("HERMES_DELEGATED_CHILD_CONTEXT", "HERMES_KANBAN_TASK", "HERMES_KANBAN_DB")
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
    py_fence = 1 if live_fence(r.get("path") or "/") else 0
    got_fence = r.get("fenced")
    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH in={r} py={py} c={got}", file=sys.stderr)
    if py_fence != got_fence:
        mism += 1
        print(f"MISMATCH fence in={r} py={py_fence} c={got_fence}", file=sys.stderr)
print(f"ORACLE delegation_pins: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
