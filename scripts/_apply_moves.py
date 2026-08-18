import os, re, json, shutil
ROOT = os.getcwd()
SRC = os.path.join(ROOT, 'src')
OBJ_MK = os.path.join(ROOT, 'build', 'objects.mk')

resolve = json.load(open('/tmp/resolve.json'))
moves = [(m[0], m[1]) for m in resolve['moves']]

# Pre-flight: no destination already exists (would clobber)
for s, d in moves:
    if os.path.exists(os.path.join(ROOT, d)):
        # handled below via skip; just record
        pass

# 1) Move files — SKIP where destination already exists (canonical present;
#    residue needs a targeted symbol-aware merge, handled separately).
moved = []
skipped = []
for s, d in moves:
    sp = os.path.join(ROOT, s); dp = os.path.join(ROOT, d)
    if os.path.exists(dp):
        skipped.append((s, d)); continue
    os.makedirs(os.path.dirname(dp), exist_ok=True)
    shutil.move(sp, dp)
    moved.append((s, d))
print("Skipped (dest exists, need merge):", len(skipped))
for s, d in skipped: print("   SKIP", s, "->", d)

# 2) Rewire objects.mk by stem (strip .c append .o)
mk = open(OBJ_MK).read()
mk_new = mk
for s, d in moved:
    f_stem = s[:-2]; np_stem = d[:-2]
    mk_new = mk_new.replace(f_stem + '.o', np_stem + '.o')
open(OBJ_MK, 'w').write(mk_new)

# 3) Header include rewiring: collect renamed .h files
hdr_map = {}
for s, d in moved:
    if d.endswith('.h'):
        hdr_map[os.path.basename(s)] = os.path.basename(d)
        hdr_map[s[len('src/'):]] = d[len('src/'):]

inc_re = re.compile(r'#include\s+"([^"]+\.h)"')
for dp, _, fs in os.walk(SRC):
    for fn in fs:
        if not (fn.endswith('.c') or fn.endswith('.h')): continue
        p = os.path.join(dp, fn)
        try: txt = open(p).read()
        except Exception: continue
        def repl(mm):
            inc = mm.group(1)
            if inc in hdr_map: return '#include "' + hdr_map[inc] + '"'
            base = inc.split('/')[-1]
            return '#include "' + hdr_map.get(base, inc) + '"'
        new = inc_re.sub(repl, txt)
        if new != txt: open(p, 'w').write(new)

print("Moved %d files; rewired objects.mk + includes." % len(moved))
# leftover ports dir?
import subprocess
leftover = subprocess.run(['find', 'src/ports', '-type', 'f'], capture_output=True, text=True).stdout.strip().splitlines()
print("Remaining src/ports files:", len(leftover))
for f in leftover: print("   ", f)
