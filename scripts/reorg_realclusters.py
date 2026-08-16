#!/usr/bin/env python3
"""Distinguish REAL 1:1-splittable clusters from spurious monolith-foldins.

A file's PRIMARY claimed module = the one matching its FILENAME stem
(e.g. llm_client.c -> llm_client.py) OR the first 'Port of Python' marker.
We group by PRIMARY module only. Clusters of size>1 are the genuine
"two files port the same module" collisions. Monoliths that fold in extra
modules (but whose own name matches a DIFFERENT module) are not collisions.
"""
import os
import re

ROOT = os.getcwd()
SRC = os.path.join(ROOT, 'src')
MARKER = re.compile(r'Port of Python\s+([a-z_]+)/([A-Za-z0-9_]+)\.py', re.I)
PKG_FIX = {'hermes_cli': 'hermes_cli', 'cli': 'hermes_cli'}

def primary_module(frel):
    fn = os.path.basename(frel)[:-2]  # stem
    fn = re.sub(r'^port_(agent|tools|gateway|hermes_cli|cli|cron)_', '', fn)
    fn = re.sub(r'^port_', '', fn)
    # try filename match first: find a marker whose mod == fn
    p = os.path.join(ROOT, frel)
    try:
        t = open(p).read()
    except Exception:
        return None
    for m in MARKER.finditer(t):
        if m.group(2) == fn:
            pkg = PKG_FIX.get(m.group(1), m.group(1))
            return (pkg, m.group(2))
    # else first marker
    m = MARKER.search(t)
    if m:
        pkg = PKG_FIX.get(m.group(1), m.group(1))
        return (pkg, m.group(2))
    return None

groups = {}
for dp, _, fs in os.walk(SRC):
    for fn in fs:
        if not fn.endswith('.c'):
            continue
        frel = os.path.relpath(os.path.join(dp, fn), ROOT)
        mod = primary_module(frel)
        if mod:
            groups.setdefault(mod, []).append(frel)

real = {k: v for k, v in groups.items() if len(v) > 1}
print(f'REAL same-module collisions (filename-matched primary): {len(real)}')
total_members = sum(len(v) for v in real.values())
print(f'total files involved: {total_members}')
for (pkg, mod), members in sorted(real.items()):
    tgt = f'src/{pkg}/{mod}.c'
    sizes = {f: sum(1 for _ in open(os.path.join(ROOT, f))) for f in members}
    canon = tgt if tgt in members else max(members, key=lambda x: sizes[x])
    print(f'\n## {pkg}/{mod}.py -> {tgt}')
    print(f'   CANON: {canon}')
    for f in sorted(members, key=lambda x: -sizes[x]):
        print(f'     {"KEEP" if f==canon else "FOLD"} {f} ({sizes[f]})')
