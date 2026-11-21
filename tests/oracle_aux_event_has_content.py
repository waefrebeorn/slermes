#!/usr/bin/env python3
"""Oracle for the real ac_anthropic_event_has_content port.

Ast-extracts LIVE _anthropic_event_has_content + _field from
agent/auxiliary_client.py (no module import) and diffs against the C port's
verdict on the same emitted event string. C emits {"event", "out"}; the
oracle re-drives the live Python on the emitted event and compares.
"""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/auxiliary_client.py"
FUNS = ("_field", "_anthropic_event_has_content")

ns = {"Any": object}
src = open(SRC, encoding="utf-8").read()
tree = ast.parse(src)
for n in tree.body:
    if isinstance(n, ast.FunctionDef) and n.name in FUNS:
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
live = ns["_anthropic_event_has_content"]

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    event_str = r.get("event", "")
    c_out = r.get("out")
    try:
        event = json.loads(event_str)
    except Exception:
        event = None
    py_out = int(bool(live(event)))
    cases += 1
    if py_out != c_out:
        mism += 1
        print(f"MISMATCH event={event_str!r} py={py_out} c={c_out}", file=sys.stderr)

print(f"ORACLE aux_event_has_content: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
