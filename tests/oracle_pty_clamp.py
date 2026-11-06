#!/usr/bin/env python3
"""Oracle for pty_clamp: _clamp_dimension / win _clamp (identical 2-arg clamp).
The harness emits {fn:"dim"|"win", value, maximum, out}. Live python for both:
    n = int(value); return max(_MIN_DIMENSION, min(n, maximum))
with _MIN_DIMENSION=1 (both pty_bridge and win_pty_bridge)."""
import ast
import json
import sys

SRC_DIM = "C:/Users/eman5/hermes-agent/hermes_cli/pty_bridge.py"
SRC_WIN = "C:/Users/eman5/hermes-agent/hermes_cli/win_pty_bridge.py"


def _load(path, func, const):
    src = open(path, encoding="utf-8").read()
    ns = {}
    for n in ast.parse(src).body:
        if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id == const for t in n.targets):
            exec(compile(ast.Module(body=[n], type_ignores=[]), path, "exec"), ns)
        if isinstance(n, ast.FunctionDef) and n.name == func:
            exec(compile(ast.Module(body=[n], type_ignores=[]), path, "exec"), ns)
    return ns[func]


def main() -> int:
    dim = _load(SRC_DIM, "_clamp_dimension", "_MIN_DIMENSION")
    win = _load(SRC_WIN, "_clamp", "_MIN_DIMENSION")

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
        print("ORACLE pty_clamp: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        total += 1
        fn = dim if c.get("fn") == "dim" else win
        want = fn(c["value"], c["maximum"])
        if c.get("out") != want:
            mism += 1
            print(f"MISMATCH {c.get('case')} fn={c.get('fn')} value={c['value']} max={c['maximum']} "
                  f"C={c.get('out')!r} PY={want!r}")
    print(f"ORACLE pty_clamp: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
