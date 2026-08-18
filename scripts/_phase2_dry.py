import os, re, json, subprocess
ROOT = os.getcwd()
SRC = os.path.join(ROOT, 'src')
OBJ_MK = os.path.join(ROOT, 'build', 'objects.mk')

# symbol extractor (matches prior proven method)
DEF_RE = re.compile(
    r'^(?:static\s+|inline\s+|const\s+|extern\s+)?'
    r'(?:[A-Za-z_][A-Za-z0-9_*\s]*?)\s+'
    r'([A-Za-z_][A-Za-z0-9_]*)\s*\(', re.M)
VAR_RE = re.compile(
    r'^(?:static\s+|const\s+)?[A-Za-z_][A-Za-z0-9_*\s]*?\s+'
    r'([A-Za-z_][A-Za-z0-9_]*)\s*=', re.M)
KW = {'if','for','while','switch','return','sizeof','typeof','do','struct','union','enum'}

def def_names(txt):
    names = set()
    for m in DEF_RE.finditer(txt): names.add(m.group(1))
    for m in VAR_RE.finditer(txt): names.add(m.group(1))
    names -= KW
    return names

plan = json.load(open('/tmp/reorg_plan.json'))
desired = plan['desired']            # residue source -> target
collision_sources = set(plan['collision_sources'])

# group: target -> [residue sources]
from collections import defaultdict
tgt_srcs = defaultdict(list)
for f, np in desired.items():
    if f in collision_sources:
        tgt_srcs[np].append(f)

mk = open(OBJ_MK).read()

deletable = []   # (source, target, missing_in_canonical)
keep = []        # (source, target, unique_symbols)

for tgt, srcs in sorted(tgt_srcs.items()):
    tgt_abs = os.path.join(ROOT, tgt)
    if not os.path.exists(tgt_abs):
        # canonical missing - defensive skip
        for s in srcs: keep.append((s, tgt, 'NO_CANONICAL'))
        continue
    canon = def_names(open(tgt_abs).read())
    for s in srcs:
        s_abs = os.path.join(ROOT, s)
        if not os.path.exists(s_abs):
            keep.append((s, tgt, 'SRC_MISSING')); continue
        syms = def_names(open(s_abs).read())
        missing = syms - canon
        if not missing:
            deletable.append((s, tgt))
        else:
            keep.append((s, tgt, sorted(missing)[:5]))

print("=== DELETABLE (fully redundant residue, safe to remove) ===", len(deletable))
for s, t in deletable: print("   DEL", s, "->", t)
print()
print("=== KEEP (has unique symbols, stays as port_/src/ports) ===", len(keep))
for s, t, m in keep:
    print(f"   KEEP {s} -> wants {t}  unique={m if isinstance(m,str) else len(m)}")

json.dump({'deletable': deletable, 'keep': [list(x) for x in keep]},
          open('/tmp/phase2_dry.json','w'), indent=1)
print("\nsaved /tmp/phase2_dry.json")
