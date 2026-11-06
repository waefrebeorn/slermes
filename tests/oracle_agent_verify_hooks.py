#!/usr/bin/env python3
"""Oracle for agent/verify_hooks.py config-driven verification hooks.

Harness families (agent_verify_hooks), emitted per line:
  nudges   {t, cfg:<json|null>, out:int}    -> max_verify_nudges(json.loads(cfg))
  guidance {t, cfg:<json|null>, out:str|null}
        -> coding_verify_guidance(json.loads(cfg))
Calls the LIVE fns with a parsed dict (None for literal null). Ast-extracts
max_verify_nudges + coding_verify_guidance + _agent_cfg + the two constants and
provides the utils.is_truthy_value source so no module import is done.
"""
import ast
import json
import re
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/verify_hooks.py"
UTILS = "C:/Users/eman5/hermes-agent/utils.py"
FUNS = ("max_verify_nudges", "coding_verify_guidance", "_agent_cfg")
CONSTS = ("DEFAULT_MAX_VERIFY_NUDGES", "CODING_VERIFY_GUIDANCE")


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {}
    for n in ast.parse(src).body:
        if isinstance(n, (ast.Assign, ast.AnnAssign)):
            targets = n.targets if isinstance(n, ast.Assign) else (n.target,)
            if any(isinstance(t, ast.Name) and t.id in CONSTS for t in targets):
                exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name in FUNS:
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    # inject utils.is_truthy_value (ast-extract from utils.py)
    usrc = open(UTILS, encoding="utf-8").read()
    for n in ast.parse(usrc).body:
        if isinstance(n, ast.Assign) and any(
            isinstance(t, ast.Name) and t.id == "TRUTHY_STRINGS" for t in n.targets):
            exec(compile(ast.Module(body=[n], type_ignores=[]), UTILS, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name == "is_truthy_value":
            exec(compile(ast.Module(body=[n], type_ignores=[]), UTILS, "exec"), ns)
    ns["is_truthy_value"] = ns["is_truthy_value"]
    nudges = ns["max_verify_nudges"]
    guidance = ns["coding_verify_guidance"]

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
        print("ORACLE agent_verify_hooks: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        total += 1
        cfg = json.loads(c.get("cfg")) if c.get("cfg") else None
        if c.get("t") == "nudges":
            want = nudges(cfg)
            if c.get("out") != want:
                mism += 1
                print(f"MISMATCH nudges cfg={c.get('cfg')!r} C={c.get('out')} PY={want}")
        elif c.get("t") == "guidance":
            want = guidance(cfg)
            if c.get("out") != want:
                mism += 1
                print(f"MISMATCH guidance cfg={c.get('cfg')!r} "
                      f"C={c.get('out')!r} PY={want!r}")
    print(f"ORACLE agent_verify_hooks: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
