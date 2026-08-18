import os, re, json, subprocess, shutil
ROOT = os.getcwd()
SRC = os.path.join(ROOT, 'src')
OBJ_MK = os.path.join(ROOT, 'build', 'objects.mk')

def shipped_defs(o_path):
    if not os.path.exists(o_path):
        return None
    r = subprocess.run(['nm', o_path], capture_output=True, text=True)
    syms = set()
    for line in r.stdout.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[1] == 'T':
            syms.add(parts[2])
    return syms

plan = json.load(open('/tmp/reorg_plan.json'))
desired = plan['desired']   # residue source -> target canonical path
mk = open(OBJ_MK).read()
objs = re.findall(r'(src/[^ ]+\.o)', mk)
built_set = set(objs)

from collections import defaultdict
tgt_srcs = defaultdict(list)
for f, np in desired.items():
    tgt_srcs[np].append(f)

# For each target, classify sources:
#  - built sources (in objects.mk) vs not
#  - which is the canonical (prefer the one at src/<pkg>/ already? none. prefer
#    the src/ports/* variant, since those are the real built ports)
moves = []      # (src, dst) built residue -> canonical (MOVE + rewire)
deletes = []    # (src, dst, reason) non-built duplicate fully covered -> DELETE
keep = []       # (src, dst, reason) has unique symbols -> keep (defer)

for tgt, srcs in sorted(tgt_srcs.items()):
    # built residue files wanting this target
    built = [s for s in srcs if (s[:-2]+'.o') in built_set]
    nonbuilt = [s for s in srcs if (s[:-2]+'.o') not in built_set]
    # canonical = the built residue at src/ports/* if any, else first built, else first src
    canon = None
    for s in built:
        if s.startswith('src/ports/'):
            canon = s; break
    if canon is None and built:
        canon = built[0]
    if canon is None:
        canon = srcs[0]
    canon_obj = canon[:-2] + '.o'
    canon_syms = shipped_defs(os.path.join(ROOT, canon_obj)) or set()
    # MOVE the canonical residue to target (if it's not already at target)
    if canon != tgt:
        moves.append((canon, tgt))
    # For non-built duplicates: check coverage
    for s in nonbuilt:
        s_syms = shipped_defs(os.path.join(ROOT, s[:-2]+'.o'))
        if s_syms is None:
            # not compiled; parse text for defs (rough)
            try:
                txt = open(os.path.join(ROOT, s)).read()
            except Exception:
                keep.append((s, tgt, 'unreadable')); continue
            syms = set(re.findall(r'(?:static\s+|inline\s+|extern\s+)?[A-Za-z_][\w\s\*]*?\s+([a-z_]\w*)\s*\(', txt, re.M))
            syms.discard('if'); syms.discard('for'); syms.discard('while'); syms.discard('switch')
            syms.discard('return'); syms.discard('sizeof'); syms.discard('do')
            missing = syms - canon_syms
        else:
            missing = s_syms - canon_syms
        if not missing:
            deletes.append((s, tgt, 'fully-covered'))
        else:
            keep.append((s, tgt, 'unique=%d' % len(missing)))

# Also handle: for built residue that is NOT the chosen canon but shares target
for tgt, srcs in sorted(tgt_srcs.items()):
    built = [s for s in srcs if (s[:-2]+'.o') in built_set]
    canon = None
    for s in built:
        if s.startswith('src/ports/'):
            canon = s; break
    if canon is None and built:
        canon = built[0]
    for s in built:
        if s != canon and s != tgt:
            # a second BUILT residue at different path -> would collide. Check coverage.
            c_syms = shipped_defs(os.path.join(ROOT, canon[:-2]+'.o')) or set()
            s_syms = shipped_defs(os.path.join(ROOT, s[:-2]+'.o')) or set()
            missing = s_syms - c_syms
            if not missing:
                deletes.append((s, tgt, 'built-fully-covered'))
            else:
                keep.append((s, tgt, 'built-unique=%d' % len(missing)))

print("MOVES (residue -> canonical, rewire incl+mk):", len(moves))
for m in moves: print("   MV", m[0], "->", m[1])
print()
print("DELETES (fully covered duplicate):", len(deletes))
for d in deletes: print("   DEL", d[0], "-> covered by", d[1], "(", d[2], ")")
print()
print("KEEP (unique symbols, defer):", len(keep))
for k in keep: print("   KEEP", k[0], "->", k[1], k[2])

json.dump({'moves': moves, 'deletes': deletes, 'keep': [list(x) for x in keep]},
          open('/tmp/resolve.json','w'), indent=1)
print("\nsaved /tmp/resolve.json")
