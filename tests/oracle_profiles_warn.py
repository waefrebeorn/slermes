#!/usr/bin/env python3
"""Oracle for hermes_cli/web_routers/profiles.py:_warn_profile_read_error.
Uses a real Python set for _profile_read_warned; detects 'warned' by set growth."""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/web_routers/profiles.py"
ns = {"str": str}
src = open(SRC, encoding="utf-8").read()
for n in ast.parse(src).body:
    if isinstance(n, ast.FunctionDef) and n.name == "_warn_profile_read_error":
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
live = ns["_warn_profile_read_error"]
warned = set()
ns["_profile_read_warned"] = warned
ns["_log"] = type("L", (), {"warning": staticmethod(lambda *a, **k: None)})()

class Exc:
    def __init__(self, m): self.m = m
    def __str__(self): return self.m

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    before = len(warned)
    live(r.get("profile", ""), Exc(r.get("error", "")))
    py = 1 if len(warned) > before else 0
    c = r["out"]
    cases += 1
    if py != c:
        mism += 1
        print(f"MISMATCH {r} py={py} c={c}", file=sys.stderr)
print(f"ORACLE profiles_warn: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
