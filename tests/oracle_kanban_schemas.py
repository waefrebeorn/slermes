#!/usr/bin/env python3
"""Oracle for tools/kanban_tools_schemas.py::_prop.

Ast-extract the LIVE `_prop` and drive it; compare the JSON-serialized dict to
the C output. The module imports stdlib only at top level (typing), but ast-
extract just the one function to stay hermetic.
"""
import ast
import json
import sys

SRC = "C:/Users/eman5/hermes-agent/tools/kanban_tools_schemas.py"


def main() -> int:
    ns = {}
    for n in ast.parse(open(SRC, encoding="utf-8").read()).body:
        if isinstance(n, ast.FunctionDef) and n.name == "_prop":
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    FN = ns["_prop"]

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE kanban_schemas: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        if c.get("tag") != "prop":
            continue
        total += 1
        want = FN(c["type"], c["desc"])
        ws = json.dumps(want, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
        cs = json.dumps(json.loads(c["out"]), ensure_ascii=False, sort_keys=True,
                        separators=(",", ":"))
        if ws != cs:
            mism += 1
            print(f"MISMATCH {c.get('case')} type={c['type']!r} desc={c['desc']!r}\n"
                  f"  PY={ws}\n  C ={cs}")
    print(f"ORACLE kanban_schemas: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
