#!/usr/bin/env python3
"""Oracle for hermes_cli/callbacks.py _skipped (var_name, reason, message)
(port src/cli/port_callbacks.c:cb_skipped). Direct import fails at ruamel via
hermes_cli.config->hermes_yaml, so ast-extract the tiny live fn and replay it on
the harness-emitted var/reason/msg; compare the rebuilt dict against C 'out'.
"""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def main() -> int:
    src = open("C:/Users/eman5/hermes-agent/hermes_cli/callbacks.py",
               encoding="utf-8").read()
    g = {"Any": None, "Dict": None}
    got = False
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name == "_skipped":
            exec(compile(ast.Module(body=[n], type_ignores=[]), "cb", "exec"), g)
            got = True
    if not got:
        print("ORACLE callbacks: live _skipped not extracted")
        return 1
    f = g["_skipped"]
    total, mism = 0, 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        if c.get("tag") != "skip":
            continue
        total += 1
        want = f(c.get("var"), c.get("reason"), c.get("msg"))
        want_p = json.loads(json.dumps(want, sort_keys=True))
        c_out = c.get("out")
        c_p = json.loads(c_out) if isinstance(c_out, str) else c_out
        if want_p != c_p:
            mism += 1
            print(f"MISMATCH skip case{c.get('case')} C={c_out} PY={json.dumps(want_p)}")
    if total == 0:
        print("ORACLE callbacks: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE callbacks: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
