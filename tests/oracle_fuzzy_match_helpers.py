#!/usr/bin/env python3
"""Oracle for tools/fuzzy_match.py PURE string helpers.

Harness families (fuzzy_match_helpers), read from stdin as "op value" lines,
each emitted as one JSON object:
  norm   {op, in, out}      -> _unicode_normalize(in)
  lead   {op, in, ws}       -> len(_leading_whitespace(in))   (byte count)
  first  {op, in, out}      -> _first_meaningful_line(in) or ""
The harness decodes literal \\n / \\t tokens in the input value before calling
the helper; the oracle mirrors that decode so both see the same string.
"""
import ast
import json
import re
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/tools/fuzzy_match.py"


def decode_nl(s: str) -> str:
    return s.replace("\\n", "\n").replace("\\t", "\t")


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"re": re}
    for n in ast.parse(src).body:
        if isinstance(n, ast.Assign) and any(
            isinstance(t, ast.Name) and t.id == "UNICODE_MAP" for t in n.targets):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name in (
                "_unicode_normalize", "_leading_whitespace", "_first_meaningful_line"):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    normalize = ns["_unicode_normalize"]
    lead = ns["_leading_whitespace"]
    first = ns["_first_meaningful_line"]

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
        print("ORACLE fuzzy_match_helpers: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        op = c.get("op")
        if op == "unknown":
            continue
        total += 1
        v = decode_nl(c.get("in") or "")
        if op == "norm":
            want = normalize(v)
            if c.get("out") != want:
                mism += 1
                print(f"MISMATCH norm in={c.get('in')!r} C={c.get('out')!r} PY={want!r}")
        elif op == "lead":
            want = len(lead(v))
            if c.get("ws") != want:
                mism += 1
                print(f"MISMATCH lead in={c.get('in')!r} C={c.get('ws')!r} PY={want!r}")
        elif op == "first":
            want = first(v)
            if c.get("out") != (want or ""):
                mism += 1
                print(f"MISMATCH first in={c.get('in')!r} C={c.get('out')!r} PY={want!r}")
    print(f"ORACLE fuzzy_match_helpers: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
