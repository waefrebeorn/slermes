#!/usr/bin/env python3
# coverage-targets: explicit_board_intent_is_pinned, kanban_path_is_fenced, owned_kanban_task, _fenced_kanban_root
"""LIVE oracle for agent/delegation_context.py:explicit_board_intent_is_pinned."""
import ast
import json
import os
import sys

SRC = "C:/Users/eman5/hermes-agent/agent/delegation_context.py"
s = open(SRC, encoding="utf-8").read()
t = ast.parse(s)
fns = [n for n in t.body if isinstance(n, ast.FunctionDef)
       and n.name in ("explicit_board_intent_is_pinned", "kanban_path_is_fenced", "owned_kanban_task", "is_dispatcher_owned_worker_context", "is_delegated_child_process_context", "_fenced_kanban_root")]
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
live_owned = NS["owned_kanban_task"]
live_root = NS["_fenced_kanban_root"]

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
    import types
    hmod = types.ModuleType("hermes_cli")
    kdb = types.ModuleType("hermes_cli.kanban_db")
    def fake_home():
        if r.get("home_error"): raise RuntimeError("no home")
        return r.get("home") or "1"
    kdb.kanban_home = fake_home
    hmod.kanban_db = kdb
    sys.modules["hermes_cli"] = hmod
    sys.modules["hermes_cli.kanban_db"] = kdb
    class Flag:
        def get(self): return r.get("non_dispatcher", 0)
    NS["_NON_DISPATCHER_OWNED_CONTEXT"] = Flag()
    py = 1 if live() else 0
    got = r["out"]
    py_fence = 1 if live_fence(r.get("path") or "/") else 0
    got_fence = r.get("fenced")
    py_owned = live_owned()
    got_owned = r.get("owned")
    py_root = live_root()
    got_root = r.get("root")
    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH in={r} py={py} c={got}", file=sys.stderr)
    if py_root != got_root:
        mism += 1
        print(f"MISMATCH root in={r} py={py_root!r} c={got_root!r}", file=sys.stderr)
    if py_owned != got_owned:
        mism += 1
        print(f"MISMATCH owned in={r} py={py_owned!r} c={got_owned!r}", file=sys.stderr)
    if py_fence != got_fence:
        mism += 1
        print(f"MISMATCH fence in={r} py={py_fence} c={got_fence}", file=sys.stderr)
print(f"ORACLE delegation_pins: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
