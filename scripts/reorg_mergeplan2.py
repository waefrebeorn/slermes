#!/usr/bin/env python3
"""Rebuild merge clusters from the live tree (ground truth, not SEED guesses).

For every .c file under src/, read its 'Port of Python <pkg>/<mod>.py' marker.
Group files by (pkg, mod). Any group with >1 member is a collision cluster
(Pattern A: dup ports, or Pattern B: native + redundant port). For each group,
pick CANONICAL = the member already at the 1:1 target path src/<pkg>/<mod>.c;
if none, the largest. Print the plan (no mutation).
"""
import os
import re

ROOT = os.getcwd()
SRC = os.path.join(ROOT, 'src')
MARKER = re.compile(r'Port of Python\s+([a-z_]+)/([A-Za-z0-9_]+)\.py', re.I)
PKG_FIX = {'hermes_cli': 'hermes_cli', 'cli': 'hermes_cli'}

def module_of(frel):
    p = os.path.join(ROOT, frel)
    try:
        t = open(p).read()
    except Exception:
        return None
    m = MARKER.search(t)
    if not m:
        return None
    pkg = PKG_FIX.get(m.group(1), m.group(1))
    return (pkg, m.group(2))

groups = {}
for dp, _, fs in os.walk(SRC):
    for fn in fs:
        if not fn.endswith('.c'):
            continue
        frel = os.path.relpath(os.path.join(dp, fn), ROOT)
        mod = module_of(frel)
        if mod:
            groups.setdefault(mod, []).append(frel)

multi = {k: v for k, v in groups.items() if len(v) > 1}
print(f'# GROUND-TRUTH MERGE CLUSTERS: {len(multi)} modules with >1 C file')
print(f'# total C files claiming a module: {sum(len(v) for v in groups.values())}; unique modules: {len(groups)}')

def target(pkg, mod):
    return f'src/{pkg}/{mod}.c'

for (pkg, mod), members in sorted(multi.items(), key=lambda kv: (kv[0][0], kv[0][1])):
    tgt = target(pkg, mod)
    # canonical = member already at target; else largest
    canon = tgt if tgt in members else None
    sizes = {}
    for f in members:
        try:
            sizes[f] = sum(1 for _ in open(os.path.join(ROOT, f)))
        except Exception:
            sizes[f] = 0
    if canon is None:
        canon = max(members, key=lambda f: sizes[f])
    print(f'\n## {pkg}/{mod}.py  ->  {tgt}')
    print(f'   CANONICAL: {canon}  ({sizes[canon]} lines)')
    for f in sorted(members, key=lambda x: -sizes[x]):
        tag = 'KEEP ' if f == canon else 'FOLD '
        print(f'   {tag} {f:55s} {sizes[f]:5d} lines')
