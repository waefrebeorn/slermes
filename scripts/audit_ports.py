"""Audit every PoP annotation in the tree:
 - format: /* PoP: C @ mod:func */ with */ on SAME line
 - the C-name slot must be a function actually defined/declared somewhere
 - flag orphans (C-name not found in any .c/.h) and mis-credits.
Also flag any 'FIXME'/'TODO'/'not implemented'/'stub' bodies in port_*.c.
"""
import re, subprocess, pathlib, collections, sys

ROOT=pathlib.Path('/home/wubu/hermes-agent-dev/slermes')
# 1) collect all defined C function names across src/include/lib
cfun=set()
cfunc_pat=re.compile(r'^(?:static\s+)?(?:const\s+)?(?:__attribute__\(\s*unused\s*\)\s+)?(?:\w+\s+)*(?:\*\s*)?(\w+)\s*\(')
for p in list(ROOT.rglob('*.c'))+list(ROOT.rglob('*.h')):
    try: txt=p.read_text()
    except: continue
    for m in cfunc_pat.finditer(txt):
        cfun.add(m.group(1))

# 2) explicit PoP pattern (single line)
pop_pat=re.compile(r'/\*\s*PoP:\s*(\w+)\s*@\s*([\w/.]+):([\w.]+)\s*\*/')
issues=[]
multiline=re.compile(r'/\*[\s\S]*?\n\s*\*\s*PoP:\s*(\w+)\s*@\s*([\w/.]+):([\w.]+)')
explicit_continuation=re.compile(r'PoP:\s*\w+\s*@\s*[\w/.]+:[\w.]+\s*$')

for p in list(ROOT.rglob('*.c'))+list(ROOT.rglob('*.h')):
    if '/lib/' in str(p) and 'libjson' not in str(p) and 'libbase64' not in str(p): 
        continue
    try: txt=p.read_text()
    except: continue
    # single-line PoP
    for m in pop_pat.finditer(txt):
        c,nm,fn=m.group(1),m.group(2),m.group(3)
        line=txt[m.start():m.end()]
        if '*/' not in line.split('PoP:')[0]+'*/'+line:  # sanity
            pass
        if c not in cfun:
            issues.append((str(p), 'ORPHAN_CNAME', f'{c} @ {nm}:{fn}'))
    # detect PoP lines where */ is on a DIFFERENT line (continuation bug)
    for m in re.finditer(r'PoP:\s*(\w+)\s*@\s*([\w/.]+):([\w.]+)', txt):
        # the line containing 'PoP:' should have */ after it on same line
        start=txt.rfind('\n',0,m.start())+1
        end=txt.find('\n',m.end())
        line=txt[start:end]
        if '*/' not in line:
            issues.append((str(p), 'MULTILINE_POP_NO_TRAILING_STAR', f'{m.group(1)} @ {m.group(2)}:{m.group(3)}'))

# 3) stub scan in port_ files
stub_pat=re.compile(r'(not implemented|notImplemented|TODO|FIXME|XXX: stub|placeholder|raise NotImplementedError)', re.I)
stubs=[]
for p in ROOT.rglob('port_*.c'):
    try: txt=p.read_text()
    except: continue
    for m in stub_pat.finditer(txt):
        stubs.append((str(p), m.group(0)))

print("=== ORPHAN / MIS-FORMAT PoP issues:", len(issues))
for i in issues[:60]: print("  ", i)
print("\n=== STUB/TODO markers in port_*.c:", len(stubs))
for s in stubs[:40]: print("  ", s)
