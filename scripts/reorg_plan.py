#!/usr/bin/env python3
"""Plan the full 1:1 mirror rename for slermes src/."""
import os
import re
from collections import Counter

root = 'src'
files = []
for dp, _, fs in os.walk(root):
    for f in fs:
        if f.endswith('.c'):
            files.append(os.path.relpath(os.path.join(dp, f), root))


def newpath(f):
    d, b = os.path.split(f)
    if d.startswith('src/ports/'):
        sub = d[len('src/ports/'):]
        d2 = 'src/' + sub if sub else 'src'
    elif d == 'src/cli':
        d2 = 'src/hermes_cli'
    elif d.startswith('src/cli/'):
        d2 = 'src/hermes_cli/' + d[len('src/cli/'):]
    else:
        d2 = d
        # but d could be 'src/cli' already handled; keep others
    if d2 == 'src/cli':
        d2 = 'src/hermes_cli'
    nb = re.sub(r'^port_(agent|tools|gateway|hermes_cli|cli|cron)_', '', b)
    nb = re.sub(r'^port_', '', nb)
    return os.path.join(d2, nb)


moves = []
seen = {}
collisions = []
for f in files:
    np = newpath(f)
    if np in seen and seen[np] != f:
        collisions.append((f, np, seen[np]))
    seen[np] = f
    if np != f:
        moves.append((f, np))

print('total .c in src:', len(files))
print('files to move/rename:', len(moves))
print('COLLISIONS:', len(collisions))
for c in collisions[:40]:
    print('  ', c)
fold = Counter(os.path.dirname(m[1]) for m in moves)
print('--- target folder distribution ---')
for k, v in sorted(fold.items()):
    print(f'  {k}: {v}')

# Verify no target collides with an EXISTING file not in the move set
existing = set(files)
extra = []
for _, np in moves:
    if np not in existing and os.path.exists(np):
        extra.append(np)
print('targets that already exist on disk (not from move):', len(extra))
for e in extra[:20]:
    print('  ', e)
