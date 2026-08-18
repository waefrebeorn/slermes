import os, re, json
ROOT = os.getcwd()
SRC = os.path.join(ROOT, 'src')
OBJ_MK = os.path.join(ROOT, 'build', 'objects.mk')
mk = open(OBJ_MK).read()

plan = json.load(open('/tmp/reorg_plan.json'))
desired = plan['desired']
from collections import defaultdict
tgt_srcs = defaultdict(list)
for f, np in desired.items():
    tgt_srcs[np].append(f)

# For each target, does a built .o exist for the TARGET path itself?
def target_built(tgt_c):
    o = tgt_c[:-2] + '.o'
    return o in mk

# Also: does the target .c file exist on disk at all?
print("=== Per target: built? file-exists? #wanted ===")
rows = []
for tgt in sorted(tgt_srcs):
    n = len(tgt_srcs[tgt])
    built = target_built(tgt)
    exists = os.path.exists(os.path.join(ROOT, tgt))
    rows.append((tgt, n, built, exists))
    print(f"  {tgt:55s} wanted={n} built_at_target={built} file_on_disk={exists}")

# Distinguish: (A) target built & exists -> real collision/redundant
#              (B) target NOT built -> residue is the only impl -> MOVE candidate
print()
move_candidates = [(t,s) for t,s,n,b,e in [(t,n,target_built(t),os.path.exists(os.path.join(ROOT,t))) for t,s in tgt_srcs.items()] if n==1 and not b and not e]
print("=== MOVE candidates (1 wanted, target not built, target file absent):", len(move_candidates))
for t,s in move_candidates:
    print(f"   {s[0]}  ->  {t}")

real_collisions = [(t,s) for t,s in tgt_srcs.items() if len(s)>1 or (target_built(t) or os.path.exists(os.path.join(ROOT,t)))]
print()
print("=== REAL collisions/redundant (target built or exists, or >1 wanted):", len(real_collisions))
for t,s in real_collisions:
    print(f"   TARGET {t}  <- {len(s)} sources: {s}")
