#!/usr/bin/env python3
"""Oracle for port_profile_describer.c -- drives the LIVE
profile_describer.py:_sample_skills.

The harness emits {"family":"samp","case":N,"names":<json>,"out":<json>};
`out` is the C side's JSON array. The Python reference is ast-extracted (the
module's aux-client LLM call is not imported).
"""
import ast
import json
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\hermes_cli\profile_describer.py"


def extract():
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    ns = {}
    # module-level constants the def reads (MAX_SKILLS_FOR_PROMPT)
    for node in tree.body:
        if isinstance(node, ast.Assign):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "_sample_skills":
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    return ns


def main():
    ns = extract()
    fn = ns.get("_sample_skills")
    if fn is None:
        print("ORACLE profile_describer: _sample_skills not found")
        return 2

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
        cases += 1

        names = c.get("names")
        if names is None:
            want = []
        else:
            want = fn(names)

        got = c.get("out")
        if got is None:
            got = []

        if got != want:
            mism += 1
            print(f"MISMATCH _sample_skills case={c.get('case')} "
                  f"n={len(names) if names else 0}\n  C={got!r}\n  PY={want!r}")

    print(f"ORACLE profile_describer: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
