import re, os, subprocess

ROOT = "."
# Scan all .c files under src/ and lib/ (skip third-party libs we don't own)
SKIP_DIRS = ('lib/whisper_cpp','lib/lib','node_modules','third_party','external','vendor')

def walk_c(dirpath):
    out=[]
    for dp,_,fns in os.walk(dirpath):
        if any(s in dp for s in SKIP_DIRS): continue
        for fn in fns:
            if fn.endswith('.c'):
                out.append(os.path.join(dp,fn))
    return out

files=[]
for d in ('src','lib','include'):
    files += walk_c(d)

rows=[]
for f in files:
    try:
        src=open(f,encoding='utf-8',errors='replace').read()
    except Exception as e:
        continue
    lines=src.count('\n')+1
    if lines < 400:  # ignore small files
        continue
    has_hermes_h = ('#include "hermes.h"' in src) or ('#include <hermes.h>' in src)
    # void* passthrough heuristic: "void *" used as a generic handle in signatures
    voidp = len(re.findall(r'void\s*\*', src))
    # PoP python modules
    pypaths = set(re.findall(r'PoP:\s*\w+\s*@\s*([A-Za-z0-9_/]+)\.py', src))
    pypaths |= set(re.findall(r'Port of Python[^:]*:\s*([A-Za-z0-9_/]+)\.py', src))
    # top-level functions
    defs = re.findall(r'^(?:static\s+)?(?:const\s+)?[A-Za-z_][\w\s\*]*?\b([a-z_][\w]*)\s*\(', src, re.M)
    ndefs = len(defs)
    # opaque struct present?
    opaque = ('struct ' in src and ('_state' in src or '_ctx' in src or '_t ' in src or 'typedef struct' in src))
    rows.append((lines, f, has_hermes_h, voidp, len(pypaths), ndefs, opaque, sorted(pypaths)))

# Sort by lines descending
rows.sort(reverse=True)
print(f"{'LINES':>6} {'HERMES.H':>8} {'void*':>6} {'#PYMOD':>6} {'#FNS':>5}  FILE  (#pymodules)")
for lines,f,hh,voidp,npy,ndefs,opaque,pymods in rows[:60]:
    flags = ('H' if hh else '.')+('V' if voidp>5 else '.')+('O' if opaque else '.')
    pymod_str = ','.join(p.split('/')[-1] for p in pymods[:6])
    if len(pymods)>6: pymod_str+='...'
    print(f"{lines:6} {str(hh):>8} {voidp:6} {npy:6} {ndefs:5}  {f}  [{pymod_str}]")
print(f"\nTotal files scanned: {len(files)}, files >=400 lines: {len(rows)}")
