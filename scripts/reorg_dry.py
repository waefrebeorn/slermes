#!/usr/bin/env python3
"""DRY: report collision counts WITHOUT moving anything (mirrors reorg_exec)."""
import os, re
ROOT = os.getcwd()
SRC = os.path.join(ROOT, 'src')
MARKER = re.compile(r'Port of Python\s+([a-z_]+)/([A-Za-z0-9_]+)\.py', re.I)
PKG_FIX = {'hermes_cli': 'hermes_cli', 'cli': 'hermes_cli'}

def strip_prefix(b):
    nb = re.sub(r'^port_(agent|tools|gateway|hermes_cli|cli|cron)_', '', b)
    return re.sub(r'^port_', '', nb)

def target_for(frel):
    rel = frel[len('src/'):] if frel.startswith('src/') else frel
    p = os.path.join(ROOT, 'src', rel)
    try:
        t = open(p).read()
    except Exception:
        t = ''
    m = MARKER.search(t)
    base = os.path.basename(frel)
    if m:
        pkg, mod = m.group(1), m.group(2)
        pkg = PKG_FIX.get(pkg, pkg)
        return os.path.join('src', pkg, mod + '.c')
    d, b = os.path.split(frel)
    if d == 'src/cli': d2 = 'src/hermes_cli'
    elif d.startswith('src/cli/'): d2 = 'src/hermes_cli/' + d[len('src/cli/'):]
    elif d.startswith('src/ports/'):
        sub = d[len('src/ports/'):]; d2 = 'src/' + sub if sub else 'src'
    else: d2 = d
    if d2 == 'src/cli': d2 = 'src/hermes_cli'
    return os.path.join(d2, strip_prefix(b))

allf = []
for ext in ('.c', '.h'):
    for dp, _, fs in os.walk(SRC):
        for fn in fs:
            if fn.endswith(ext):
                allf.append(os.path.relpath(os.path.join(dp, fn), ROOT))

desired = {}
for f in allf:
    if 'port_' in os.path.basename(f) or f.startswith('src/ports/') or f.startswith('src/cli/'):
        np = target_for(f)
        if np != f:
            desired[f] = np

ts = {}
for f, np in desired.items():
    ts.setdefault(np, []).append(f)
ct = set()
for tgt, srcs in ts.items():
    if len(srcs) > 1:
        ct.add(tgt); continue
    occ = os.path.join(ROOT, tgt)
    if os.path.exists(occ) and tgt not in desired:
        ct.add(tgt)
phase1 = {f: np for f, np in desired.items() if np not in ct}
print('desired renames:', len(desired))
print('collision targets:', len(ct))
print('Phase-1 safe renames:', len(phase1))
print('--- collision targets (file claimed by >1 source) ---')
for t in sorted(ct):
    print('  ', t, '<-', ts[t])
