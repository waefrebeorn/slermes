import os, re, json
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

allfiles = []
for ext in ('.c', '.h', '.inc'):
    for dp, _, fs in os.walk(SRC):
        for fn in fs:
            if fn.endswith(ext):
                allfiles.append(os.path.relpath(os.path.join(dp, fn), ROOT))

desired = {}
for f in allfiles:
    if 'port_' in os.path.basename(f) or f.startswith('src/ports/') or f.startswith('src/cli/'):
        np = target_for(f)
        if np != f:
            desired[f] = np

target_sources = {}
for f, np in desired.items():
    target_sources.setdefault(np, []).append(f)

collision_targets = set()
for tgt, srcs in target_sources.items():
    if len(srcs) > 1:
        collision_targets.add(tgt); continue
    occ = os.path.join(ROOT, tgt)
    occupant_is_source = tgt in desired
    if os.path.exists(occ) and not occupant_is_source:
        collision_targets.add(tgt)

phase1 = {f: np for f, np in desired.items() if np not in collision_targets}
collision_sources = set()
for tgt in collision_targets:
    for f in target_sources.get(tgt, []):
        collision_sources.add(f)

print("desired renames:", len(desired))
print("collision targets:", len(collision_targets), "(sources:", len(collision_sources), ")")
print("Phase1 safe moves:", len(phase1))
print()
print("=== WILL MOVE (phase1) — sample ===")
for f, np in sorted(phase1.items())[:50]:
    print(f"  {f}  ->  {np}")
print("  ... total", len(phase1))
print()
print("=== COLLISION (excluded, stays as port_/src/ports) ===")
for f in sorted(collision_sources):
    print("  ", f, "-> wants", desired.get(f))

json.dump({'desired': desired, 'phase1': phase1,
           'collision_targets': sorted(collision_targets),
           'collision_sources': sorted(collision_sources)},
          open('/tmp/reorg_plan.json','w'), indent=1)
print("\nsaved /tmp/reorg_plan.json")
