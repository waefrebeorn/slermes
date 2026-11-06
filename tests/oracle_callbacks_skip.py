#!/usr/bin/env python3
"""Oracle for hermes_cli/callbacks.py::_skipped.

Ast-extract the LIVE `_skipped` and drive it; compare the JSON-serialized dict
against the C output. Pure function, no deps beyond the dict it builds.
"""
import ast
import json
import sys

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/callbacks.py"


def main() -> int:
    ns = {}
    for n in ast.parse(open(SRC, encoding="utf-8").read()).body:
        if isinstance(n, ast.FunctionDef) and n.name == "_skipped":
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    FN = ns["_skipped"]

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
        print("ORACLE callbacks_skip: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        if c.get("tag") != "skip":
            continue
        total += 1
        want = FN(c["var"], c["reason"], c["msg"])
        ws = json.dumps(want, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
        cs = json.dumps(json.loads(c["out"]), ensure_ascii=False, sort_keys=True,
                        separators=(",", ":"))
        if ws != cs:
            mism += 1
            print(f"MISMATCH {c.get('case')} var={c['var']!r} reason={c['reason']!r} "
                  f"msg={c['msg']!r}\n  PY={ws}\n  C ={cs}")
    print(f"ORACLE callbacks_skip: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
