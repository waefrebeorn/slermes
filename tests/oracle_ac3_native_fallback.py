#!/usr/bin/env python3
"""Oracle for the flow3 native fallback (ac3_call -> codex_event_has_content).
AST-extracts the LIVE helper plus _event_field and _CODEX_PROGRESS_DELTA_TYPES."""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/codex_runtime.py"
ns = {"str": str, "Any": object, "bool": bool, "isinstance": isinstance,
      "dict": dict}
src = open(SRC, encoding="utf-8").read()
t = ast.parse(src)
for n in t.body:
    if isinstance(n, ast.FunctionDef) and n.name == "_event_field":
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    if isinstance(n, (ast.Assign, ast.AnnAssign)):
        targets = n.targets if isinstance(n, ast.Assign) else [n.target]
        for tg in targets:
            if isinstance(tg, ast.Name) and tg.id == "_CODEX_PROGRESS_DELTA_TYPES":
                exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
for n in t.body:
    if isinstance(n, ast.FunctionDef) and n.name == "_codex_event_has_content":
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
live = ns["_codex_event_has_content"]

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    event = r.get("event")
    py = bool(live(event))
    c = bool(r["out"])
    cases += 1
    if py != c:
        mism += 1
        print(f"MISMATCH {r} py={py} c={c}", file=sys.stderr)
print(f"ORACLE ac3_native_fallback: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
