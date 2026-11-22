#!/usr/bin/env python3
"""Oracle for hermes_cli/auth.py:_is_same_auth_store.
AST-extracts LIVE _same_path + _is_same_auth_store; fake Paths are already
resolved, and samefile raises OSError unless the case says they are the same
inode (mirrors the caller-supplied flag)."""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/auth.py"
ns = {"bool": bool, "OSError": OSError, "Exception": Exception}


class P:
    def __init__(self, s, sf):
        self.s, self.sf = s, sf

    def resolve(self, strict=False):
        return self

    def __eq__(self, o):
        return isinstance(o, P) and self.s == o.s

    def samefile(self, o):
        if not self.sf:
            raise OSError("not same inode")
        return True


ns["Path"] = P
src = open(SRC, encoding="utf-8").read()
for n in ast.parse(src).body:
    if isinstance(n, ast.FunctionDef) and n.name in ("_same_path", "_is_same_auth_store"):
        n.decorator_list = []
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
live = ns["_is_same_auth_store"]

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    sf = bool(r.get("samefile"))
    py = bool(live(P(r.get("left", ""), sf), P(r.get("right", ""), sf)))
    c = bool(r["out"])
    cases += 1
    if py != c:
        mism += 1
        print(f"MISMATCH {r} py={py} c={c}", file=sys.stderr)
print(f"ORACLE same_auth_store: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
