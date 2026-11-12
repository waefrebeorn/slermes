#!/usr/bin/env python3
"""Oracle for tools/microsoft_graph_auth.py::format_graph_error.

The harness (t_port_microsoft_graph_auth.c) emits {"case":N,"in":<json>,"out":<str|null>}.
The live function returns a string or None; the C maps None to a NULL buffer.
"""
import ast
import json
import sys

SRC = "C:/Users/eman5/hermes-agent/tools/microsoft_graph_auth.py"


def main() -> int:
    ns = {"Any": object}
    src = open(SRC, encoding="utf-8").read()
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name == "format_graph_error":
            mod = ast.Module(body=[n], type_ignores=[])
            exec(compile(ast.fix_missing_locations(mod), SRC, "exec"), ns)
    fn = ns["format_graph_error"]

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
        want = fn(c.get("in"))
        got = c.get("out")
        if want != got:
            mism += 1
            print(f"MISMATCH {c.get('case')}: in={c.get('in')!r} C={got!r} PY={want!r}")
    if total == 0:
        print("ORACLE microsoft_graph_auth: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE microsoft_graph_auth: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
