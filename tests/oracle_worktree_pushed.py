#!/usr/bin/env python3
"""Oracle for hermes_cli/worktree_ops.py:_worktree_branch_pushed_exact.
AST-extracts the LIVE function, stubbing the two subprocess helpers with the
harness-emitted branch and local HEAD."""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/worktree_ops.py"
ns = {"bool": bool, "str": str, "dict": dict, "Optional": object}
src = open(SRC, encoding="utf-8").read()
for n in ast.parse(src).body:
    if isinstance(n, ast.FunctionDef) and n.name == "_worktree_branch_pushed_exact":
        n.decorator_list = []
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
live = ns["_worktree_branch_pushed_exact"]

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    ns["_worktree_current_branch"] = lambda path, timeout, _b=r.get("branch"): _b or None
    ns["_git_out"] = lambda args, path, timeout=None, _h=r.get("local_head"): _h
    py = bool(live("wt", r.get("remote_heads")))
    c = bool(r["out"])
    cases += 1
    if py != c:
        mism += 1
        print(f"MISMATCH {r} py={py} c={c}", file=sys.stderr)
print(f"ORACLE worktree_pushed: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
