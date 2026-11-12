#!/usr/bin/env python3
"""Oracle for hermes_cli/sizefmt.py::format_bytes.

The harness (t_port_sizefmt.c) emits {"case":N,"in":<str|null>,"out":<str>}.
The live function raises TypeError/ValueError on None and non-numeric input,
which the C maps to "?"; the oracle asserts that mapping.
"""
import ast
import json
import sys

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/sizefmt.py"


def main() -> int:
    ns = {}
    src = open(SRC, encoding="utf-8").read()
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name == "format_bytes":
            mod = ast.Module(body=[n], type_ignores=[])
            exec(compile(ast.fix_missing_locations(mod), SRC, "exec"), ns)
    fn = ns["format_bytes"]

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
        try:
            want = fn(inp)
        except (TypeError, ValueError):
            want = "?"
        got = c.get("out")
        if want != got:
            mism += 1
            print(f"MISMATCH {c.get('case')}: in={inp!r} C={got!r} PY={want!r}")
    if total == 0:
        print("ORACLE sizefmt: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE sizefmt: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
