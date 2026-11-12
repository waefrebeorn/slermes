#!/usr/bin/env python3
"""Oracle for hermes_cli/models_validate.py::_non_public_host.

The harness (t_port_models_validate.c) emits {"case":N,"in":"<host>","out":0|1}.
Compare with Python bool equality.
"""
import ast
import json
import sys

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/models_validate.py"


def main() -> int:
    ns = {}
    for n in ast.parse(open(SRC, encoding="utf-8").read()).body:
        if isinstance(n, ast.FunctionDef) and n.name == "_non_public_host":
            exec(compile(ast.fix_missing_locations(ast.Module(body=[n], type_ignores=[])),
                         SRC, "exec"), ns)
    fn = ns["_non_public_host"]

    total = 0
    mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        want = bool(fn(c.get("in")))
        got = bool(c.get("out"))
        if want != got:
            mism += 1
            print(f"MISMATCH {c.get('case')}: in={c.get('in')!r} C={got} PY={want}")
    if total == 0:
        print("ORACLE models_validate: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE models_validate: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
