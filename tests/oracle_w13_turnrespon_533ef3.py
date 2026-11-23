#!/usr/bin/env python3
"""Differential oracle for agent/turn_response_intake.py (gate w13_turnrespon_533ef3).

AST-extracts the live _coerce_content_text body from the quarry so no heavy
provider/turn dependencies need importing (and none of its behaviour is copied).
"""
import ast
import builtins
import json
import sys
from pathlib import Path

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

Q = Path("C:/Users/eman5/hermes-agent")
G = "w13_turnrespon_533ef3"
SRC = Q / "agent" / "turn_response_intake.py"

_tree = ast.parse(SRC.read_text(encoding="utf-8"))
_code = None
for _n in _tree.body:
    if isinstance(_n, ast.FunctionDef) and _n.name == "_coerce_content_text":
        _code = compile(ast.Module(body=[_n], type_ignores=[]), str(SRC), "exec")
        break
if _code is None:
    print("ORACLE %s: FATAL missing live _coerce_content_text" % G, file=sys.stderr)
    sys.exit(1)

_NS = {"__builtins__": builtins, "json": json, "Any": object}
exec(_code, _NS)
_coerce = _NS["_coerce_content_text"]


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") != "coerce":
            continue
        cases += 1
        try:
            py = _coerce(rec.get("raw"))
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%s C=%r PY=%r" % (G, rec.get("case"), got, py), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (G, cases, mism))
    return 1 if (mism or cases == 0) else 0


if __name__ == "__main__":
    sys.exit(main())
