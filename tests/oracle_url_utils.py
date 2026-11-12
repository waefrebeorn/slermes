#!/usr/bin/env python3
"""Oracle for hermes_cli/url_utils.py::format_url_host.

The harness (t_port_url_utils.c) emits {"case":N,"in":<str|null>,"out":<str|null>}.
The live function raises on None, so a NULL input is asserted to map to NULL
(the C convention) rather than driving the live function.
"""
import ast
import json
import sys

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/url_utils.py"


def main() -> int:
    ns = {}
    src = open(SRC, encoding="utf-8").read()
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name == "format_url_host":
            mod = ast.Module(body=[n], type_ignores=[])
            exec(compile(ast.fix_missing_locations(mod), SRC, "exec"), ns)
    fn = ns["format_url_host"]

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
        inp = c.get("in")
        got = c.get("out")
        want = None if inp is None else fn(inp)
        if want != got:
            mism += 1
            print(f"MISMATCH {c.get('case')}: in={inp!r} C={got!r} PY={want!r}")
    if total == 0:
        print("ORACLE url_utils: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE url_utils: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
