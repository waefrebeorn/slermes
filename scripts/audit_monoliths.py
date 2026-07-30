import re, os
from pathlib import Path
from collections import Counter

ROOT = Path('.')
c_files = []
for r,_,fs in os.walk('src'):
    for f in fs:
        if f.endswith('.c'):
            c_files.append(Path(r)/f)

def modules_of(p):
    t = p.read_text(errors='ignore')
    mods = Counter()
    for m in re.finditer(r'/\*\s*PoP:[^\n]*?@\s*([\w./-]+\.py):', t):
        mods[m.group(1)] += 1
    for m in re.finditer(r'Port of Python[^:]*:?\s*([\w./-]+\.py)', t):
        mods[m.group(1)] += 1
    return mods

print(f"{'lines':>6} {'#mod':>4}  top_module  file")
print("-"*90)
rows = []
for p in c_files:
    n = sum(1 for _ in open(p, errors='ignore'))
    if n < 800:
        continue
    mods = modules_of(p)
    if not mods:
        rows.append((n, 0, '(none)', str(p)))
        continue
    top = mods.most_common(1)[0][0]
    rows.append((n, len(mods), top, str(p)))
rows.sort(reverse=True)
for n, c, top, f in rows[:35]:
    print(f"{n:6d} {c:4d}  {top:45s} {f}")
