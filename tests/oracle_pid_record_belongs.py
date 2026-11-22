#!/usr/bin/env python3
"""Oracle for gateway/status.py:_pid_record_belongs_to_current_profile.
Stubs _same_hermes_home with Windows normcase semantics (matching the C port),
and _get_process_hermes_home with the passed process_home."""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/gateway/status.py"
ns = {"str": str, "dict": dict, "Any": object, "Optional": object}
src = open(SRC, encoding="utf-8").read()
for n in ast.parse(src).body:
    if isinstance(n, ast.FunctionDef) and n.name == "_pid_record_belongs_to_current_profile":
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
live = ns["_pid_record_belongs_to_current_profile"]

def same_home(l, r):
    return str(l).lower().replace("/", "\\") == str(r).lower().replace("/", "\\")

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    rec = r.get("record")
    ph = r.get("process_home", "")
    ns["_same_hermes_home"] = same_home
    ns["_get_process_hermes_home"] = lambda _p=ph: _p
    py = bool(live(rec))
    c = bool(r["out"])
    cases += 1
    if py != c:
        mism += 1
        print(f"MISMATCH {r} py={py} c={c}", file=sys.stderr)
print(f"ORACLE pid_record_belongs: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
