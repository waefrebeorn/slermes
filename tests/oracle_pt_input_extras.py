#!/usr/bin/env python3
"""Oracle for port_pt_input_extras.c -- drives the LIVE pt_input_extras.py defs.

The live functions are ast-extracted; the module's other half mutates
prompt_toolkit's ANSI_SEQUENCES and is not imported here.
"""
import ast
import json
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\hermes_cli\pt_input_extras.py"
WANTED = ["_lock_variants", "_lock_twins"]
OFFSETS = (0, 64, 128, 192)


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {}
    for node in ast.parse(src).body:
        if isinstance(node, ast.Assign):
            for tgt in node.targets:
                if isinstance(tgt, ast.Name) and tgt.id == "_LOCK_BIT_OFFSETS":
                    ns["_LOCK_BIT_OFFSETS"] = ast.literal_eval(node.value)
    ns.setdefault("_LOCK_BIT_OFFSETS", OFFSETS)
    for node in ast.parse(src).body:
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
        want = list(ns[fn](c["in"]))
        got = c.get("out") or []
        if got != want:
            mism += 1
            print(f"MISMATCH {fn} in={c['in']} C={got!r} PY={want!r}")

    print(f"ORACLE pt_input_extras: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
