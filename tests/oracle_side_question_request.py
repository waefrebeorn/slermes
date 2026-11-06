#!/usr/bin/env python3
"""Oracle for side_question_request::has_server_side_tools.
The harness emits {"in": <aph kwargs json>, "ret": bool}; we ast-extract the
LIVE has_server_side_tools plus its _CLIENT_TOOL_TYPES and
_ROUTING_ONLY_OPENROUTER_PLUGINS constants and compare on the same input."""
import ast
import json
import sys

SRC = "C:/Users/eman5/hermes-agent/agent/side_question_request.py"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"frozenset": frozenset}
    for n in ast.parse(src).body:
        if isinstance(n, ast.Assign) and any(
            isinstance(t, ast.Name) and t.id in ("_CLIENT_TOOL_TYPES",
                                                 "_ROUTING_ONLY_OPENROUTER_PLUGINS")
            for t in n.targets):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        if isinstance(n, ast.FunctionDef) and n.name == "has_server_side_tools":
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    FN = ns["has_server_side_tools"]

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
        print("ORACLE side_question_request: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        total += 1
        api_kwargs = json.loads(c["in"])
        want = bool(FN(api_kwargs))
        if c.get("ret") != want:
            mism += 1
            print(f"MISMATCH {total} C={c.get('ret')!r} PY={want!r} in={c['in']}")
    print(f"ORACLE side_question_request: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
