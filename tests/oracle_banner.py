#!/usr/bin/env python3
"""Oracle for port_banner.c -- drives the LIVE hermes_cli/banner.py defs.

The harness echoes its inputs from C. The live functions are ast-extracted
because importing hermes_cli.banner pulls in rich / config / version_info.
"""
import ast
import json
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\hermes_cli\banner.py"
WANTED = ["_plural", "_format_context_length", "_display_toolset_name"]


def extract():
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    ns = {}
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    return ns


def main():
    ns = extract()
    cases = 0
    mism = 0

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        fn = c.get("fn")
        cases += 1

        if fn == "_plural":
            want = ns["_plural"](c["in"], c["word"])
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH _plural in={c['in']!r} C={got!r} PY={want!r}")

        elif fn == "_format_context_length":
            want = ns["_format_context_length"](c["in"])
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH _format_context_length in={c['in']!r} C={got!r} PY={want!r}")

        elif fn == "_display_toolset_name":
            # JSON null and the empty string are the same truthiness case
            want = ns["_display_toolset_name"](c["in"] or "")
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH _display_toolset_name in={c['in']!r} C={got!r} PY={want!r}")

    print(f"ORACLE banner: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
