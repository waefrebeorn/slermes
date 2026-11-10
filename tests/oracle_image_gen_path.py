#!/usr/bin/env python3
"""Oracle for tools/image_generation_tool.py:_looks_like_absolute_file_path
(port src/tools/image_gen_path.c). The live module pulls heavy deps, so
ast-extract the fn verbatim and seed os.path (ntpath on Windows Python) exactly
as live resolves it.

Harness emits {fn:"laf", in:<str>, out:true|false} for each case."""
import ast
import os
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/tools/image_generation_tool.py"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"os": os, "Any": object, "Optional": object}
    body = [n for n in ast.parse(src).body
            if isinstance(n, ast.FunctionDef)
            and n.name == "_looks_like_absolute_file_path"]
    exec(compile(ast.Module(body=body, type_ignores=[]), SRC, "exec"), ns)
    fn = ns["_looks_like_absolute_file_path"]

    total = 0
    mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            import json
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        want = 1 if fn(c.get("in")) else 0
        got = 1 if c.get("out") else 0
        if want != got:
            mism += 1
            print(f"MISMATCH in={c.get('in')!r} C={c.get('out')!r} PY={bool(want)}")
    if total == 0:
        print("ORACLE image_gen_path: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE image_gen_path: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
