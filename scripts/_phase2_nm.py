import os, re, json, subprocess
ROOT = os.getcwd()
SRC = os.path.join(ROOT, 'src')
OBJ_MK = os.path.join(ROOT, 'build', 'objects.mk')

# Better def extractor: only top-level function definitions.
# Matches:  [static|inline|const|extern]*  TYPE name(   where TYPE has no
# keyword that precedes a declaration-statement. We strip typedef/struct/enum
# by requiring a '(' right after the identifier and the function is followed
# by a '{' (definition) or ';' (proto) on the same nesting. We just want the
# set of defined top-level symbols; use nm on the .o instead for ground truth.

plan = json.load(open('/tmp/reorg_plan.json'))
desired = plan['desired']
collision_sources = set(plan['collision_sources'])

from collections import defaultdict
tgt_srcs = defaultdict(list)
for f, np in desired.items():
    if f in collision_sources:
        tgt_srcs[np].append(f)

# Build map target_path -> object .o (from objects.mk)
mk = open(OBJ_MK).read()
objs = re.findall(r'(src/[^ ]+\.o)', mk)
# target .c -> .o
tgt_to_obj = {}
for o in objs:
    tgt_to_obj[o[:-2] + '.c'] = o

# For each canonical target, gather the DEFINED global symbols actually shipped
# by looking at the .o linked. If the .o exists (compiled), use nm.
def shipped_defs(o_path):
    if not os.path.exists(o_path):
        return None
    r = subprocess.run(['nm', o_path], capture_output=True, text=True)
    out = r.stdout
    syms = set()
    for line in out.splitlines():
        parts = line.split()
        if len(parts) < 3: continue
        typ = parts[1]
        if typ == 'T':  # global defined text
            syms.add(parts[2])
    return syms

report = {'deletable': [], 'keep': []}
for tgt, srcs in sorted(tgt_srcs.items()):
    obj = tgt_to_obj.get(tgt)
    canon_syms = None
    if obj:
        canon_syms = shipped_defs(os.path.join(ROOT, obj))
    if canon_syms is None:
        # target not built / no .o -> cannot safely delete sources that want it
        for s in srcs:
            report['keep'].append([s, tgt, 'NO_CANON_OBJ'])
        continue
    for s in srcs:
        s_abs = os.path.join(ROOT, s)
        s_obj = tgt_to_obj.get(s)
        if s_obj and os.path.exists(os.path.join(ROOT, s_obj)):
            s_syms = shipped_defs(os.path.join(ROOT, s_obj))
        else:
            s_syms = None
        if s_syms is None:
            # residue not built alone; try parse
            report['keep'].append([s, tgt, 'SRC_NO_OBJ'])
            continue
        missing = s_syms - canon_syms
        if not missing:
            report['deletable'].append([s, tgt])
        else:
            report['keep'].append([s, tgt, len(missing)])

print("DELETABLE:", len(report['deletable']))
for d in report['deletable']: print("   DEL", d[0], "->", d[1])
print()
print("KEEP:", len(report['keep']))
for k in report['keep']: print("   KEEP", k[0], "->", k[1], k[2])

json.dump(report, open('/tmp/phase2_nm.json','w'), indent=1)
print("\nsaved /tmp/phase2_nm.json")
