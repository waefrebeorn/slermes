#!/usr/bin/env python3
"""Oracle for hermes_cli/dashboard_auth/audit.py::_bounded_value.

The harness (t_port_dashboard_auth_audit.c) emits {"case":N,"in":<json>,"out":<json>}.
The live function is recursive over dict/list/scalars; the C side only sees
parsed JSON, so the Python repr() branch for exotic objects is unreachable.
Compare with Python == (so int/float representations compare equal).
"""
import ast
import json
import sys

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/dashboard_auth/audit.py"


def main() -> int:
    ns = {"Any": object}
    for n in ast.parse(open(SRC, encoding="utf-8").read()).body:
        if isinstance(n, ast.Assign) and isinstance(n.targets[0], ast.Name) \
                and n.targets[0].id in ("_MAX_FIELD_LENGTH", "_TRUNCATION_MARKER"):
            exec(compile(ast.fix_missing_locations(ast.Module(body=[n], type_ignores=[])),
                         SRC, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name == "_bounded_value":
            exec(compile(ast.fix_missing_locations(ast.Module(body=[n], type_ignores=[])),
                         SRC, "exec"), ns)
    fn = ns["_bounded_value"]

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
        # The live function reads its cap from module globals; align them with
        # the harness so both sides bound at the same length.
        ns["_MAX_FIELD_LENGTH"] = c.get("max_len", ns["_MAX_FIELD_LENGTH"])
        ns["_TRUNCATION_MARKER"] = c.get("marker", ns["_TRUNCATION_MARKER"])
        want = fn(c.get("in"))
        got = c.get("out")
        if want != got:
            mism += 1
            print(f"MISMATCH {c.get('case')}: in={c.get('in')!r} "
                  f"C={got!r} PY={want!r}")
    if total == 0:
        print("ORACLE dashboard_auth_audit: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE dashboard_auth_audit: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
