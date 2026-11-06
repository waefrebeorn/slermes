#!/usr/bin/env python3
"""Oracle for agent/thinking_timeout_guidance.py:is_thinking_timeout.

Harness family (agent_thinking_timeout_guidance):
  {reason, model, err, out:int} -> is_thinking_timeout(classified, model, err)
where classified is reconstructed as reason.value == reason (the live duck-typed
.reason.value check). Replays the LIVE classifier: requires the timeout reason,
a non-None reasoning stale-timeout floor for the model (the port resolves this
via cross-object agent_reasoning_timeouts_get_floor; live inline-imports
get_reasoning_stale_timeout_floor), and any transport-kill substring in the
lowercased error. Ast-extracts both modules' defs + constants, and injects a
fake agent.reasoning_timeouts module so the function-body inline import resolves.
build_thinking_timeout_guidance not emitted by the harness -> not compared.
"""
import ast
import json
import re
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

TTG = "C:/Users/eman5/hermes-agent/agent/thinking_timeout_guidance.py"
RT = "C:/Users/eman5/hermes-agent/agent/reasoning_timeouts.py"


def execload(path):
    """ast-extract every module-level Assign/AnnAssign + every FunctionDef."""
    ns = {"re": re}
    src = open(path, encoding="utf-8").read()
    for n in ast.parse(src).body:
        if isinstance(n, (ast.Assign, ast.AnnAssign)):
            targets = n.targets if isinstance(n, ast.Assign) else (n.target,)
            if any(isinstance(t, ast.Name) for t in targets):
                exec(compile(ast.Module(body=[n], type_ignores=[]), path, "exec"), ns)
        elif isinstance(n, ast.FunctionDef):
            exec(compile(ast.Module(body=[n], type_ignores=[]), path, "exec"), ns)
    return ns


def main() -> int:
    ttg = execload(TTG)
    rt = execload(RT)
    # fake agent.reasoning_timeouts so the function's inline import resolves
    pkg = types.ModuleType("agent")
    pkg.__path__ = []
    sys.modules["agent"] = pkg
    sub = types.ModuleType("agent.reasoning_timeouts")
    sub.get_reasoning_stale_timeout_floor = rt["get_reasoning_stale_timeout_floor"]
    sys.modules["agent.reasoning_timeouts"] = sub
    is_tt = ttg["is_thinking_timeout"]

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
        print("ORACLE agent_thinking_timeout_guidance: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        total += 1
        classified = types.SimpleNamespace(
            reason=types.SimpleNamespace(value=c.get("reason")))
        want = is_tt(classified, c.get("model"), c.get("err"))
        if bool(c.get("out")) != want:
            mism += 1
            print(f"MISMATCH reason={c.get('reason')!r} model={c.get('model')!r} "
                  f"err={c.get('err')!r} C={c.get('out')} PY={want}")
    print(f"ORACLE agent_thinking_timeout_guidance: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
