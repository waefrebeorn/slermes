#!/usr/bin/env python3
"""PHASE 1+2 EXECUTOR — full 1:1 mirror rename with collision merges.

Phase 1: every port_*.c/.h (and src/ports/*, src/cli) is renamed to its
  marker-driven 1:1 target src/<pkg>/<mod>.c, prefix stripped, includes +
  objects.mk rewired. Collision groups (<=1 canonical target claimed by >=2
  sources, or occupied by a non-source native) are EXCLUDED from rename and
  handled in Phase 2.

Phase 2: for each collision group, the canonical (= file already at target, or
  largest) KEEPS its path; every other member's UNIQUE top-level functions are
  appended to the canonical file (guarded against duplicate symbols), then the
  member is deleted. Includes/objects.mk updated.

After both, caller runs `make slermes` and verifies green.
"""
import os
import re
import shutil

ROOT = os.getcwd()
SRC = os.path.join(ROOT, 'src')
OBJ_MK = os.path.join(ROOT, 'build', 'objects.mk')
MARKER = re.compile(r'Port of Python\s+([a-z_]+)/([A-Za-z0-9_]+)\.py', re.I)
PKG_FIX = {'hermes_cli': 'hermes_cli', 'cli': 'hermes_cli'}

def strip_prefix(b):
    nb = re.sub(r'^port_(agent|tools|gateway|hermes_cli|cli|cron)_', '', b)
    nb = re.sub(r'^port_', '', nb)
    return nb

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

# ---- gather all files (.c/.h/.inc) ----
allfiles = []
for ext in ('.c', '.h', '.inc'):
    for dp, _, fs in os.walk(SRC):
        for fn in fs:
            if fn.endswith(ext):
                allfiles.append(os.path.relpath(os.path.join(dp, fn), ROOT))

# ---- compute desired target per file ----
desired = {}
for f in allfiles:
    if 'port_' in os.path.basename(f) or f.startswith('src/ports/') or f.startswith('src/cli/'):
        np = target_for(f)
        if np != f:
            desired[f] = np

# ---- detect collisions: target claimed by >=2 sources, or occupied by non-source ----
target_sources = {}
for f, np in desired.items():
    target_sources.setdefault(np, []).append(f)

collision_targets = set()
for tgt, srcs in target_sources.items():
    if len(srcs) > 1:
        collision_targets.add(tgt)
        continue
    # occupied by a non-source (a file not in desired that already sits there)
    occ = os.path.join(ROOT, tgt)
    if os.path.exists(occ):
        occupant_is_source = tgt in desired  # its own source already there (will move)
        if not occupant_is_source:
            collision_targets.add(tgt)

# Phase 1 moves = desired entries whose target is NOT a collision target
phase1 = {f: np for f, np in desired.items() if np not in collision_targets}
collision_sources = set()
for tgt in collision_targets:
    for f in target_sources.get(tgt, []):
        collision_sources.add(f)

print(f'desired renames: {len(desired)}')
print(f'collision targets: {len(collision_targets)}  (sources: {len(collision_sources)})')
print(f'Phase 1 safe renames: {len(phase1)}')

# ---- apply Phase 1 ----
renamed = []
hdr_map = {}
for f, np in phase1.items():
    srcp = os.path.join(ROOT, f)
    dstp = os.path.join(ROOT, np)
    os.makedirs(os.path.dirname(dstp), exist_ok=True)
    if os.path.exists(dstp):
        print('  SKIP EXISTING', dstp); continue
    shutil.move(srcp, dstp)
    renamed.append((f, np))
    if f.endswith('.h'):
        hdr_map[os.path.basename(f)] = os.path.basename(np)
        # also map full include path (relative to src root), e.g.
        # "agent/port_x.h" -> "agent/x.h"
        rel_old = f[len('src/'):] if f.startswith('src/') else f
        rel_new = np[len('src/'):] if np.startswith('src/') else np
        hdr_map[rel_old] = rel_new

# rewrite includes of RENAMED headers in EVERY .c/.h under src/ (not just
# renamed files) — collision-excluded files also reference renamed headers.
inc_re = re.compile(r'#include\s+"([^"]+\.h)"')
for dp, _, fs in os.walk(SRC):
    for fn in fs:
        if not (fn.endswith('.c') or fn.endswith('.h')):
            continue
        p = os.path.join(dp, fn)
        try:
            txt = open(p).read()
        except Exception:
            continue
        def repl2(mm):
            inc = mm.group(1)
            if inc in hdr_map:
                return '#include "' + hdr_map[inc] + '"'
            base = inc.split('/')[-1]
            return '#include "' + hdr_map.get(base, inc) + '"'
        newtxt = inc_re.sub(repl2, txt)
        if newtxt != txt:
            open(p, 'w').write(newtxt)

# rewrite objects.mk: ONLY the exact old paths of files we actually moved.
# (NO blanket src/ports/->src/ replace: collision-excluded files stay put
#  and must keep their src/ports/... path in objects.mk.)
# Use the stem (drop .c, append .o) for object entries; also handle quoted/
# parenthesized occurrences and the leading-space continuation lines.
mk = open(OBJ_MK).read()
mk_new = mk
for f, np in renamed:
    f_stem = f[:-2] if f.endswith('.c') else f
    np_stem = np[:-2] if np.endswith('.c') else np
    mk_new = mk_new.replace(f_stem + '.o', np_stem + '.o')
    mk_new = mk_new.replace('"' + f + '"', '"' + np + '"')
    mk_new = mk_new.replace('(' + f + ')', '(' + np + ')')
    mk_new = mk_new.replace(f + ' ', np + ' ')
    mk_new = mk_new.replace(f + '\n', np + '\n')
if mk_new != mk:
    open(OBJ_MK, 'w').write(mk_new)

print(f'Phase 1 applied: {len(renamed)} files renamed.')
print('Remaining collision sources for Phase 2:', len(collision_sources))

# Save collision plan for Phase 2
import json
json.dump({'collision_targets': sorted(collision_targets),
           'collision_sources': sorted(collision_sources),
           'target_sources': {k: v for k, v in target_sources.items() if k in collision_targets}},
          open(os.path.join(ROOT, 'scripts/phase2_state.json'), 'w'), indent=1)
