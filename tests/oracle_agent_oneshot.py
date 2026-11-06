#!/usr/bin/env python3
"""Oracle for agent/oneshot.py:_strip_code_fence.

Harness family (agent_oneshot), emitted per line:
  {fn:strip_code_fence, in, out} -> _strip_code_fence(in)
Ast-extracts the single live def. Nothing else needed.
"""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/oneshot.py"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {}
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name == "_strip_code_fence":
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    strip = ns["_strip_code_fence"]

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
        print("ORACLE agent_oneshot: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        total += 1
        want = strip(c.get("in"))
        if c.get("out") != want:
            mism += 1
            print(f"MISMATCH in={c.get('in')!r} C={c.get('out')!r} PY={want!r}")
    print(f"ORACLE agent_oneshot: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
