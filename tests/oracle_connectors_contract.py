#!/usr/bin/env python3
"""Oracle for tools/connectors/contract.py::resolves.

The module imports only stdlib. Ast-extract the LIVE `resolves` function plus
the `TargetState` str-enum, `RESOLVED_STATES` const, and `_RESOLVED_BY_KIND` map
it depends on, and drive them with kind/state passed as strings (TargetState is
a str-Enum so `TargetState.connected == "connected"`). The harness passes NULL
kind as profile:null -> we call with kind=None.
"""
import ast
import json
import sys

SRC = "C:/Users/eman5/hermes-agent/tools/connectors/contract.py"


def main() -> int:
    ns = {"Enum": __import__("enum").Enum}
    for n in ast.parse(open(SRC, encoding="utf-8").read()).body:
        if isinstance(n, ast.ClassDef) and n.name == "TargetState":
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.Assign) and isinstance(n.targets[0], ast.Name):
            if n.targets[0].id in ("RESOLVED_STATES", "_RESOLVED_BY_KIND"):
                exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name == "resolves":
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    FN = ns["resolves"]

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
        print("ORACLE connectors_contract: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        if c.get("tag") != "res":
            continue
        total += 1
        kind = c["kind"]
        state = c["state"]
        want = FN(kind, state)
        if bool(want) != bool(c["out"]):
            mism += 1
            print(f"MISMATCH {c.get('case')} kind={kind!r} state={state!r}: "
                  f"C={c['out']} PY={want}")
    print(f"ORACLE connectors_contract: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
