#!/usr/bin/env python3
"""Oracle for hermes_cli/profile_describer.py _sample_skills. The function is
ast-extracted and run live (MAX_SKILLS_FOR_PROMPT seeded).
"""
import ast
import json
import sys

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/profile_describer.py"
code = open(SRC, encoding="utf-8").read()
tree = ast.parse(code)

seg = None
values = {}
for n in tree.body:
    if isinstance(n, ast.Assign):
        for t in n.targets:
            if isinstance(t, ast.Name) and t.id == "MAX_SKILLS_FOR_PROMPT":
                values["MAX_SKILLS_FOR_PROMPT"] = ast.get_source_segment(code, n)
    elif isinstance(n, ast.FunctionDef) and n.name == "_sample_skills":
        seg = ast.get_source_segment(code, n)

NS = {}
if values:
    exec(compile(ast.Module(body=[ast.parse(values["MAX_SKILLS_FOR_PROMPT"]).body[0]],
                           type_ignores=[]), "x", "exec"), NS)
exec(compile(ast.Module(body=[ast.parse(seg).body[0]], type_ignores=[]),
             "x", "exec"), NS)
fn = NS["_sample_skills"]


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE profile_describer: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        want = fn(list(c["names"]))
        if c["out"] != want:
            mism += 1
            print(f"MISMATCH samp {c['case']}: C={c['out']!r} PY={want!r}")
    print(f"ORACLE profile_describer: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
