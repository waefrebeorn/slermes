#!/usr/bin/env python3
"""Build the authoritative Python-module -> C-file(s) map for the slermes tree.

Uses the scanner's own fold-in markers plus a function-name cross-check, so the
rename/split plan mirrors the Python package tree 1:1 (the user's hard rule:
identical folders + identical file/function naming).

Emits a JSON map:
  pymodule (e.g. "agent/agent_init.py") ->
     { "candidates": [rel c paths claiming it],
       "func_match_files": [c files whose functions match python def names] }
"""
import ast
import json
import os
import re
import sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else os.getcwd()
SL = os.path.join(ROOT)  # slermes root
PKGS = ['agent', 'tools', 'hermes_cli', 'gateway']

# 1. Index python def names per module
py_defs = {}   # module_rel -> set(def names)
for pkg in PKGS:
    pd = os.path.join(ROOT, pkg)
    if not os.path.isdir(pd):
        continue
    for dp, _, fs in os.walk(pd):
        for fn in fs:
            if not fn.endswith('.py'):
                continue
            rel = os.path.relpath(os.path.join(dp, fn), ROOT)
            try:
                src = open(os.path.join(dp, fn)).read()
            except Exception:
                continue
            try:
                tree = ast.parse(src)
            except Exception:
                continue
            names = set()
            for node in ast.walk(tree):
                if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                    names.add(node.name)
            py_defs[rel] = names

# 2. Scan C/H for fold-in markers and function-name evidence
marker_re = re.compile(r'Port of Python\s+([a-z_]+/[A-Za-z0-9_]+\.py)', re.I)
modref_re = re.compile(r'(?:agent|tools|hermes_cli|gateway)/([A-Za-z0-9_]+\.py)')

c_files = []
for dp, _, fs in os.walk(os.path.join(ROOT, 'src')):
    for fn in fs:
        if fn.endswith(('.c', '.h')):
            c_files.append(os.path.relpath(os.path.join(dp, fn), ROOT))

# map: pymodule -> list of c files claiming via marker
marker_map = {}
# map: pymodule -> list of c files whose functions overlap python def names strongly
func_map = {}

for cf in c_files:
    try:
        src = open(os.path.join(ROOT, cf)).read()
    except Exception:
        continue
    # fold-in markers
    for m in marker_re.finditer(src):
        pym = m.group(1)
        marker_map.setdefault(pym, []).append(cf)
    for m in modref_re.finditer(src):
        pym = m.group(1)
        marker_map.setdefault(pym, []).append(cf)
    # function-name overlap
    c_funcs = set(re.findall(r'(?:^|[^A-Za-z0-9_])([A-Za-z_][A-Za-z0-9_]*)\s*\(', src))
    for pymod, names in py_defs.items():
        base = os.path.basename(pymod)  # e.g. agent_init.py
        mod = base[:-3]
        # overlap of non-trivial python names present as C identifiers
        overlap = names & c_funcs
        # only count meaningful overlap (>=3 or contains module name fn)
        if len(overlap) >= 3 or mod in c_funcs:
            func_map.setdefault(pymod, []).append((cf, len(overlap)))

out = {}
all_py = set(py_defs.keys()) | set(marker_map.keys()) | set(func_map.keys())
for pym in sorted(all_py):
    out[pym] = {
        'marker_files': sorted(set(marker_map.get(pym, []))),
        'func_match_files': sorted(set(f for f, _ in func_map.get(pym, []))),
    }

json.dump(out, open(os.path.join(ROOT, 'scripts/pyport_map.json'), 'w'), indent=1)
# Summary
multi = {k: v for k, v in out.items() if (v['marker_files'] or v['func_match_files'])}
print(f"python modules total (indexed): {len(py_defs)}")
print(f"modules with >=1 candidate C file: {len(multi)}")
# how many have a 1:1 obvious target (single file mentions module name in filename)
one_to_one = 0
for k, v in out.items():
    mod = os.path.basename(k)[:-3]
    cands = v['marker_files'] + v['func_match_files']
    if len(cands) == 1 and mod in os.path.basename(cands[0]):
        one_to_one += 1
print(f"modules with a single same-named C candidate: {one_to_one}")
print("map written -> scripts/pyport_map.json")
