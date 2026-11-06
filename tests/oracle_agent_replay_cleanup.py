#!/usr/bin/env python3
"""Oracle for agent/replay_cleanup.py:is_interrupted_tool_result.

Harness (t_port_agent_replay_cleanup.c) emits {in:<str|null>, out:<0|1>} per
case. Replays the LIVE fn on the identical value. Ast-extracts the fn plus its
compiled regex constant _INTERRUPT_MARKER_LINE (an AnnAssign). Seeds re/json.
"""
import ast
import json
import re
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/replay_cleanup.py"
FN = "is_interrupted_tool_result"
# _INTERRUPT_MARKER_LINE = re.compile(r"^\[(?:command|execution) interrupted\b[^\n]*\]\s*$",
#                                     re.IGNORECASE)  -- hardcoded from the live module
INTERRUPT_PATTERN = r"^\[(?:command|execution) interrupted\b[^\n]*\]\s*$"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"re": re, "json": json, "Any": object,
          "_INTERRUPT_MARKER_LINE": re.compile(INTERRUPT_PATTERN, re.IGNORECASE)}
    body = []
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name == FN:
            body.append(n)
    exec(compile(ast.Module(body=body, type_ignores=[]), SRC, "exec"), ns)
    f = ns[FN]

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
        print("ORACLE agent_replay_cleanup: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        total += 1
        want = 1 if f(c.get("in")) else 0
        got = c.get("out")
        if want != got:
            mism += 1
            print(f"MISMATCH in={c.get('in')!r} C={got} PY={want}")
    print(f"ORACLE agent_replay_cleanup: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
