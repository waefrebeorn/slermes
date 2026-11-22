#!/usr/bin/env python3
# coverage-targets: _platform_default_hermes_home
"""LIVE oracle for hermes_constants.py:_platform_default_hermes_home."""
import ast
import json
import sys
import types

SRC = "C:/Users/eman5/hermes-agent/hermes_constants.py"
t = ast.parse(open(SRC, encoding="utf-8").read())
fn = next(n for n in t.body if isinstance(n, ast.FunctionDef)
          and n.name == "_platform_default_hermes_home")
fn.decorator_list = []

class P:
    def __init__(self, p): self.p = p.rstrip("/") if p != "/" else "/"
    def __truediv__(self, other): return P(self.p + "/" + str(other).lstrip("/"))
    def __str__(self): return self.p

mod = types.ModuleType("sys")
cases = mism = 0
for line in sys.stdin:
    if '"out"' not in line:
        continue
    r = json.loads(line)
    mod.platform = "win32" if r.get("win32") else "linux"
    ns = {"sys": mod, "Path": P}
    exec(compile(ast.Module(body=[fn], type_ignores=[]), SRC, "exec"), ns)
    py = str(ns["_platform_default_hermes_home"](
        {"HERMES_DATA_DIR_SUFFIX": r.get("suffix", ""),
         "LOCALAPPDATA": r.get("local_appdata", "")},
        lambda: P(r.get("user_home") or "")))
    got = r["out"]
    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH in={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE hermes_home_defaults: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
