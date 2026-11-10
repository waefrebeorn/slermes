#!/usr/bin/env python3
"""Oracle for hermes_cli/dashboard_auth/audit.py:_bounded_value
(port src/cli/port_audit.c:port_audit_bounded_value). Recursively bounds object
string fields: str longer than _MAX_FIELD_LENGTH=256 truncated to
256-len("...[truncated]")=242 chars + marker. Non-(str,int,float,bool,None)
values are repr()'d. Ast-extract the live fn + seed constants + json so the
oracle is the live code itself."""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def main() -> int:
    src = open("C:/Users/eman5/hermes-agent/hermes_cli/dashboard_auth/audit.py",
               encoding="utf-8").read()
    g = {"Any": None, "json": json}
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name == "_bounded_value":
            exec(compile(ast.Module(body=[n], type_ignores=[]), "bv", "exec"), g)
        elif isinstance(n, ast.Assign):
            for t in n.targets:
                if isinstance(t, ast.Name) and t.id in ("_MAX_FIELD_LENGTH", "_TRUNCATION_MARKER"):
                    exec(compile(ast.Module(body=[n], type_ignores=[]), "bv", "exec"), g)
    bv = g["_bounded_value"]
    total, mism = 0, 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        if c.get("tag") != "bnd":
            continue
        want = bv(c.get("in"))
        want_s = json.dumps(want, ensure_ascii=False, sort_keys=True)
        # C json_serialize is compact (no spaces); compare on parsed value, not
        # string formatting, to avoid whitespace-only artifacts.
        if json.loads(want_s) != json.loads(c.get("out")):
            mism += 1
            hint = ""
            if isinstance(want, str):
                hint = f" (len {len(want)})"
            print(f"MISMATCH bnd case{c.get('case')} C={c.get('out')!r} PY={want_s!r}{hint}")
    if total == 0:
        print("ORACLE audit: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE audit: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
