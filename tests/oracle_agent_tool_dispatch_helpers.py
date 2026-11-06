#!/usr/bin/env python3
"""Oracle for agent/tool_dispatch_helpers.py mutation-tracking helpers.

Harness families (agent_tool_dispatch_helpers), emitted per line:
  neutralize  {t, in, out}                 -> _neutralize_delimiters(in)
  landed_*    {t, tool, args:<json>, result:<json|null>, paths:[...]}
              -> _extract_landed_file_mutation_paths(tool, json.loads(args),
                                                     None if result is null)
Ast-extracts the live helpers + their deps: _neutralize_delimiters (uses
_DELIMITER_TOKEN_RE), _extract_file_mutation_targets (uses _V4A_FILE_HEADER +
_V4A_MOVE_HEADER regexes and the _FILE_MUTATING_TOOLS frozenset from
tool_result_classification). The live args are a dict (C passes JSON text), so
the oracle json.loads the emitted args field; a null result maps to None.
"""
import ast
import json
import re
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/tool_dispatch_helpers.py"
TRC = "C:/Users/eman5/hermes-agent/agent/tool_result_classification.py"
FUNS = ("_neutralize_delimiters", "_extract_file_mutation_targets",
        "_extract_landed_file_mutation_paths")


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"re": re, "json": json}
    for n in ast.parse(src).body:
        if isinstance(n, (ast.Assign, ast.AnnAssign)):
            targets = n.targets if isinstance(n, ast.Assign) else (n.target,)
            rhs = n.value
            if not any(isinstance(t, ast.Name) for t in targets):
                continue
            # seed static constants + bare re.compile calls (pure)
            ok = isinstance(rhs, (ast.Constant, ast.List, ast.Tuple, ast.Dict,
                                  ast.Set, ast.BinOp))
            if isinstance(rhs, ast.Call) and isinstance(rhs.func, ast.Attribute) \
                    and isinstance(rhs.func.value, ast.Name) \
                    and rhs.func.value.id == "re":
                ok = True
            if ok and any(t.id in ("_V4A_FILE_HEADER", "_V4A_MOVE_HEADER",
                                   "_DELIMITER_TOKEN_RE")
                          for t in targets if isinstance(t, ast.Name)):
                exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name in FUNS:
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    # _FILE_MUTATING_TOOLS lives in tool_result_classification (a frozenset).
    try:
        for n in ast.parse(open(TRC, encoding="utf-8").read()).body:
            if isinstance(n, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == "FILE_MUTATING_TOOL_NAMES"
                for t in n.targets):
                exec(compile(ast.Module(body=[n], type_ignores=[]), TRC, "exec"), ns)
        ns["_FILE_MUTATING_TOOLS"] = ns["FILE_MUTATING_TOOL_NAMES"]
    except Exception as e:
        print(f"IMPORT_FAIL {e!r}")
        return 2
    neutralize = ns["_neutralize_delimiters"]
    landed = ns["_extract_landed_file_mutation_paths"]

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
        print("ORACLE agent_tool_dispatch_helpers: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        total += 1
        if c.get("t") == "neutralize":
            want = neutralize(c.get("in"))
            if c.get("out") != want:
                mism += 1
                print(f"MISMATCH neutralize in={c.get('in')!r} C={c.get('out')!r} PY={want!r}")
        else:
            args = c.get("args") if isinstance(c.get("args"), dict) else (
                json.loads(c.get("args")) if c.get("args") else {})
            result = c.get("result")
            want = landed(c.get("tool"), args, result)
            if c.get("paths") != want:
                mism += 1
                print(f"MISMATCH {c.get('t')} tool={c.get('tool')!r} "
                      f"C={c.get('paths')} PY={want}")
    print(f"ORACLE agent_tool_dispatch_helpers: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
