#!/usr/bin/env python3
"""Oracle for hermes_cli/dashboard_auth/audit.py::_bounded_value.

Ast-extract the LIVE `_bounded_value` plus its constants and drive it on the
parsed `in` value; serialize the bounded result the same way (compact JSON via
json.dumps sort_keys-like). The harness passes `in` as raw JSON and `out` as the
C-serialized bounded json. The module imports only stdlib + typing.
"""
import ast
import json
import sys

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/dashboard_auth/audit.py"


def load():
    ns = {}
    for n in ast.parse(open(SRC, encoding="utf-8").read()).body:
        if isinstance(n, ast.Assign) and isinstance(n.targets[0], ast.Name):
            if n.targets[0].id in ("_MAX_FIELD_LENGTH", "_TRUNCATION_MARKER"):
                exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name == "_bounded_value":
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    ns["_MAX_FIELD_LENGTH"] = 256
    ns["_TRUNCATION_MARKER"] = "...[truncated]"
    return ns["_bounded_value"]


def main() -> int:
    FN = load()
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
        print("ORACLE audit_bounded: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        if c.get("tag") != "bnd":
            continue
        total += 1
        pyin = c["in"]   # already a parsed JSON value from the line's json.loads
        want = FN(pyin)
        # normalize: Python returns maybe non-str-leaf objects unchanged; the
        # C keeps numbers/bools. Serialize both compact and compare.
        ws = json.dumps(want, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
        # out is the C serialization (preserves key order; only json-parse it)
        cs = json.dumps(json.loads(c["out"]), ensure_ascii=False, sort_keys=True,
                        separators=(",", ":"))
        if ws != cs:
            mism += 1
            print(f"MISMATCH {c.get('case')} in={c['in']!r}\n  PY={ws}\n  C ={cs}")
    print(f"ORACLE audit_bounded: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
