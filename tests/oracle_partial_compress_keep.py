#!/usr/bin/env python3
"""Oracle for hermes_cli/partial_compress.py:_coerce_keep.

Harness family (partial_compress_keep):
  {case, token:<!json|null>, ret:int}
Replays the live _coerce_keep on the SAME token. Seeds just the two constants;
the fn is pure (int() + clamp). The 'null' case is legitimate: int(None) raises
TypeError -> DEFAULT_KEEP_LAST, matching C's NULL handling.
"""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/partial_compress.py"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"DEFAULT_KEEP_LAST": 2, "MAX_KEEP_LAST": 100}
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name == "_coerce_keep":
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    coerce_keep = ns["_coerce_keep"]

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
        print("ORACLE partial_compress_keep: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        total += 1
        want = coerce_keep(c.get("token"))
        if c.get("ret") != want:
            mism += 1
            print(f"MISMATCH {c.get('case')} token={c.get('token')!r} "
                  f"C={c.get('ret')!r} PY={want!r}")
    print(f"ORACLE partial_compress_keep: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
