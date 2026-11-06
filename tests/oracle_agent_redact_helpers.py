#!/usr/bin/env python3
"""Oracle for agent/redact.py helper subset.

Harness families (agent_redact_helpers), emitted per line:
  mask    {t, in, out} -> _mask_token_nonreusable(in)   (non-reusable sentinel)
  envdump {t, in, out:int} -> is_env_dump_command(in)
The mask family needs the whole PREFIX_SUBSTRINGS chain; envdump needs
_command_segments + _ENV_DUMP_COMMANDS + shlex. Ast-extract live definitions.
"""
import ast
import json
import re
import shlex
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/redact.py"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"re": re, "shlex": shlex}
    # dependency order: Assigns first (incl. computed _PREFIX_SUBSTRINGS which
    # needs _extract_literal_prefix + _PREFIX_PATTERNS), then the fns.
    wanted_assigns = {"_ENV_DUMP_COMMANDS", "_PREFIX_PATTERNS", "_PREFIX_SUBSTRINGS"}
    wanted_fns = {"_extract_literal_prefix", "_skip_char_class", "_read_char_class",
                  "_extract_until_first_meta", "_mask_token_nonreusable",
                  "_command_segments", "is_env_dump_command"}
    # We must reach _extract_literal_prefix first (computed subscript), so run
    # the fns it depends on before the Assign referencing it. Simpler: exec all
    # body nodes that are Assign/FunctionDef in source order; ast preserves order.
    for n in ast.parse(src).body:
        if isinstance(n, ast.Assign) and any(
            isinstance(t, ast.Name) and t.id in wanted_assigns for t in n.targets):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name in wanted_fns:
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    mask = ns["_mask_token_nonreusable"]
    envdump = ns["is_env_dump_command"]

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
        print("ORACLE agent_redact_helpers: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        total += 1
        if c.get("t") == "mask":
            want = mask(c.get("in"))
            if c.get("out") != want:
                mism += 1
                print(f"MISMATCH mask in={c.get('in')!r} C={c.get('out')!r} PY={want!r}")
        elif c.get("t") == "envdump":
            want = envdump(c.get("in"))
            if bool(c.get("out")) != want:
                mism += 1
                print(f"MISMATCH envdump in={c.get('in')!r} C={c.get('out')} PY={want}")
    print(f"ORACLE agent_redact_helpers: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
