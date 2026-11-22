#!/usr/bin/env python3
"""Coverage-gap guardrail for a fixture-driven port gate.

Usage:
  python scripts/check_gate_coverage.py \
      tests/oracle_<gate>.py tests/fixtures/<gate>.jsonl <live_source.py>

Derives the functions the oracle is interested in from string literals that
match top-level functions in the LIVE source, then fails if a fixture does not
exercise one of them. This is the cheap local version of the coverage-gap
repair loop used by repository-level translation agents: a green gate is not
allowed to silently skip a function it claims to port.
"""
import ast
import json
import os
import sys

if len(sys.argv) != 4:
    print("usage: check_gate_coverage.py <oracle.py> <fixtures.jsonl> <source.py>")
    sys.exit(2)
oracle_path, fixture_path, source_path = sys.argv[1:4]
for p in (oracle_path, fixture_path, source_path):
    if not os.path.isfile(p):
        print(f"missing: {p}")
        sys.exit(2)

oracle_src = open(oracle_path, encoding="utf-8", errors="replace").read()
source_src = open(source_path, encoding="utf-8", errors="replace").read()
live_funcs = {
    n.name for n in ast.parse(source_src).body
    if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef))
}
tree = ast.parse(oracle_src)
literals = {
    n.value for n in ast.walk(tree)
    if isinstance(n, ast.Constant) and isinstance(n.value, str)
}
# Prefer the explicit dispatch set when the oracle declares one, otherwise
# pick names it resolves through its namespace dictionary. Stub helpers such
# as load_jobs are not separate gate targets.
named = set(live_funcs & literals)
for n in tree.body:
    if isinstance(n, ast.Assign):
        for t in n.targets:
            if isinstance(t, ast.Name) and t.id in {"KEEP_FUNCS", "KEEP", "TARGET_FUNCS"}:
                got = {e.value for e in ast.walk(n.value)
                       if isinstance(e, ast.Constant) and isinstance(e.value, str)}
                if got:
                    named = got & live_funcs
indexed = set()
for n in ast.walk(tree):
    if (isinstance(n, ast.Subscript) and isinstance(n.value, ast.Name)
            and n.value.id == "NS" and isinstance(n.slice, ast.Constant)
            and isinstance(n.slice.value, str)):
        indexed.add(n.slice.value)
target = sorted(named | (indexed & live_funcs))
import re
m = re.search(r"#\s*coverage-targets:\s*([^\n]+)", oracle_src)
if m:
    target = sorted({t.strip() for t in m.group(1).split(",")} & live_funcs)
fixture_funcs = set()
with open(fixture_path, encoding="utf-8", errors="replace") as f:
    for line in f:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except Exception:
            continue
        if isinstance(r, dict) and "func" in r:
            fixture_funcs.add(str(r["func"]))

missing = [f for f in target if f not in fixture_funcs]
print(f"oracle: {os.path.basename(oracle_path)}")
print(f"live functions referenced by oracle: {len(target)}")
print(f"distinct fixture funcs: {len(fixture_funcs)}")
if missing:
    print("UNCOVERED: " + ", ".join(missing))
    sys.exit(1)
if not target:
    print("WARN: no LIVE function names found in oracle literals")
    sys.exit(1)
print("VERDICT: all referenced LIVE functions have fixture cases")
