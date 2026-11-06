#!/usr/bin/env python3
"""Oracle for agent/reasoning_timeouts.py:get_reasoning_stale_timeout_floor.

Harness families (agent_reasoning_timeouts):
  {in:<model str>|null, out:<float via %g>|null}
Replays the live fn on the SAME model string. Seeds re + ast-extracts the
_REASONING_STALE_TIMEOUT_FLOORS dict and the _SORTED_REASONING_FLOORS built-list
(an AnnAssign: list of (slug, floor, compiled ^slug(?:$|[-._:])) patterns,
longest-slug-first). out is emitted with %g (6 sig digits), so compare float
with relative tolerance; a null in/out means the live fn returned None.
"""
import ast
import json
import re
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/reasoning_timeouts.py"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"re": re}
    for n in ast.parse(src).body:
        if isinstance(n, ast.Assign) and any(
            isinstance(t, ast.Name) and t.id == "_REASONING_STALE_TIMEOUT_FLOORS"
            for t in n.targets):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.AnnAssign) and isinstance(n.target, ast.Name) \
                and n.target.id in ("_REASONING_STALE_TIMEOUT_FLOORS",
                                    "_SORTED_REASONING_FLOORS"):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name == "get_reasoning_stale_timeout_floor":
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    fn = ns["get_reasoning_stale_timeout_floor"]

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
        print("ORACLE agent_reasoning_timeouts: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        total += 1
        want = fn(c.get("in"))
        have = c.get("out")
        # null out -> live None; a real float must match within %g rounding
        if want is None:
            ok = have is None
        else:
            ok = have is not None and abs(want - have) <= max(1e-6, abs(want) * 1e-6)
        if not ok:
            mism += 1
            print(f"MISMATCH in={c.get('in')!r} C={have!r} PY={want!r}")
    print(f"ORACLE agent_reasoning_timeouts: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
