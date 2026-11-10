#!/usr/bin/env python3
"""Oracle for tools/bot_desktop/idle_watch.py::next_check_in.

Ast-extract the LIVE `next_check_in` plus its `_MIN_DELAY_S` constant and the
runtime DEFAULT_IDLE_STOP_MINUTES it reads. The function reads
`runtime.DEFAULT_IDLE_STOP_MINUTES` via `from tools.bot_desktop import runtime`;
seed a fake `tools.bot_desktop.runtime` in sys.modules with that attribute.
idle_null in the harness == idle=None.
"""
import ast
import json
import sys
import types

SRC = "C:/Users/eman5/hermes-agent/tools/bot_desktop/idle_watch.py"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"math": __import__("math")}
    for n in ast.parse(src).body:
        if isinstance(n, ast.Assign) and isinstance(n.targets[0], ast.Name):
            if n.targets[0].id == "_MIN_DELAY_S":
                exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name == "next_check_in":
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    FN = ns["next_check_in"]

    fake_runtime = types.ModuleType("tools.bot_desktop.runtime")
    fake_runtime.DEFAULT_IDLE_STOP_MINUTES = 30
    sys.modules["tools.bot_desktop"] = types.ModuleType("tools.bot_desktop")
    sys.modules["tools.bot_desktop.runtime"] = fake_runtime
    ns["runtime"] = fake_runtime

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
        print("ORACLE idle_watch: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        if c.get("tag") != "nc":
            continue
        total += 1
        idle = None if c["idle_null"] else c["idle"]
        want = FN(c["limit"], idle)
        if want != c["out"]:
            mism += 1
            print(f"MISMATCH {c.get('case')} limit={c['limit']} idle={idle!r}: "
                  f"C={c['out']} PY={want}")
    print(f"ORACLE idle_watch: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
