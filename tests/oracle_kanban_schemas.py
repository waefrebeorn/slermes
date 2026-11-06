#!/usr/bin/env python3
"""Oracle for tools/kanban_tools_schemas.py::_schema (and its `_prop`
dependency already committed). Ast-extract the LIVE _prop, _DESC_BOARD,
_board_schema_prop and _schema, then compare json.dumps of the result against
the C serialization. The module imports stdlib only at top level.
"""
import ast
import json
import sys

sys.stderr.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
SRC = "C:/Users/eman5/hermes-agent/tools/kanban_tools_schemas.py"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {}
    for n in ast.parse(src).body:
        if isinstance(n, ast.Assign) and isinstance(n.targets[0], ast.Name):
            if n.targets[0].id == "_DESC_BOARD":
                exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name in ("_prop", "_board_schema_prop", "_schema"):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    FN = ns["_schema"]

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
        print("ORACLE kanban_schema: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        if c.get("tag") != "schema":
            continue
        total += 1
        want = FN(c["name"], c["desc"], c["props"], c["required"])
        ws = json.dumps(want, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
        cs = json.dumps(json.loads(c["out"]), ensure_ascii=False, sort_keys=True,
                        separators=(",", ":"))
        if ws != cs:
            mism += 1
            print(f"MISMATCH {c.get('case')} name={c['name']!r}\n  PY={ws}\n  C ={cs}")
    print(f"ORACLE kanban_schema: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
