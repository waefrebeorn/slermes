#!/usr/bin/env python3
"""Oracle for agent/display.py pure display helpers.

Harness families (agent_display_helpers), emitted per line:
  oneline {t, in, out}            -> _oneline(in)
  trunc   {t, in, m, out}         -> _tail_trunc(in, m)   (C PoP names it
                                      _truncate_preview; that name was refactored
                                      in live, the port IS live _tail_trunc)
  basename {t, in, out}           -> _shell_basename(in)
"""
import ast
import json
import re
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/display.py"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"re": re}
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name in (
                "_oneline", "_shell_basename", "_tail_trunc"):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    oneline = ns["_oneline"]
    shell_basename = ns["_shell_basename"]
    tail_trunc = ns["_tail_trunc"]

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
        print("ORACLE agent_display_helpers: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        total += 1
        t = c.get("t")
        if t == "oneline":
            want = oneline(c.get("in"))
            if c.get("out") != want:
                mism += 1
                print(f"MISMATCH oneline in={c.get('in')!r} C={c.get('out')!r} PY={want!r}")
        elif t == "basename":
            want = shell_basename(c.get("in"))
            if c.get("out") != want:
                mism += 1
                print(f"MISMATCH basename in={c.get('in')!r} C={c.get('out')!r} PY={want!r}")
        elif t == "trunc":
            want = tail_trunc(c.get("in"), c.get("m"))
            if c.get("out") != want:
                mism += 1
                print(f"MISMATCH trunc in={c.get('in')!r} m={c.get('m')} C={c.get('out')!r} PY={want!r}")
    print(f"ORACLE agent_display_helpers: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
