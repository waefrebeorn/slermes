#!/usr/bin/env python3
"""Oracle for skills_hub_clawhub.py:_owner_from_payload / _owner_matches."""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/tools/skills_hub_clawhub.py"
FUNS = ("_owner_from_payload", "_owner_matches")
ns = {"Optional": object, "dict": dict, "str": str, "Any": object}
src = open(SRC, encoding="utf-8").read()
for n in ast.parse(src).body:
    if isinstance(n, ast.ClassDef):
        for m in n.body:
            if isinstance(m, ast.FunctionDef) and m.name in FUNS:
                exec(compile(ast.Module(body=[m], type_ignores=[]), SRC, "exec"), ns)
    if isinstance(n, ast.FunctionDef) and n.name in FUNS:
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)

# The LIVE defs are @staticmethod / @classmethod (both reference `cls` only
# as a name, never actually use it in _owner_matches's body). Call with a
# dummy cls for the classmethod shape.
class _Cls:
    pass
_Cls._owner_from_payload = staticmethod(ns["_owner_from_payload"].__func__
    if hasattr(ns["_owner_from_payload"], "__func__") else ns["_owner_from_payload"])

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    exp, data = r.get("expected"), r.get("data")
    # C echoes the extracted owner; verify it too.
    of = ns["_owner_from_payload"]
    of = getattr(of, "__func__", of)  # @staticmethod -> underlying fn
    py_owner = of(data)
    if (py_owner or None) != (r.get("owner") or None):
        mism += 1; print(f"OWNER MISMATCH {r} py={py_owner!r}", file=sys.stderr)
    om = ns["_owner_matches"]
    om = getattr(om, "__func__", om)  # @classmethod -> underlying fn(cls, ...)
    py = int(bool(om(_Cls, exp, data)))
    cases += 1
    if py != r["out"]:
        mism += 1
        print(f"MISMATCH {r} py={py} c={r['out']}", file=sys.stderr)
print(f"ORACLE owner_matches: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
