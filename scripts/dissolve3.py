#!/usr/bin/env python3
"""dissolve3.py — per-function dissolution, GROUPING by canonical destination file.

Improvements over dissolve2:
- Functions whose PoP module has NO existing canonical are grouped into the
  EXISTING best-fit subsystem grab-bag successor: src/<subsys>/<module>.c
  created ONCE per python module (proper name), NOT one .c per function.
  Key fix: py module path 'gateway/authz_mixin.py' -> src/gateway/authz_mixin.c
  (single file holding all authz_mixin fns), not a file per fn.
- Statics/defines in the residue preamble stay.
"""
import os, re, sys, subprocess
from collections import defaultdict

ROOT='/home/wubu/hermes-agent-dev/slermes'
DRY='--apply' not in sys.argv
TARGETS=[a for a in sys.argv[1:] if not a.startswith('--')]

POP=re.compile(r'/\*+\s*PoP:\s*([A-Za-z0-9_]+)\s*@\s*([A-Za-z0-9_/.]+?)\.py')
DEF=re.compile(r'^(?:static\s+|inline\s+|const\s+)*(?:[A-Za-z_][A-Za-z0-9_ \t]*[\* \t])?([A-Za-z_][A-Za-z0-9_]*)\s*\(')

def blocks_of(text):
    lines=text.split('\n'); n=len(lines); i=0; out=[]
    while i<n:
        l=lines[i]
        if not l or l[0] in ' #}/)':
            i+=1; continue
        m=DEF.match(l)
        if m and not l.startswith('typedef'):
            name=m.group(1)
            j=i; depth=0; started=False; proto=False
            while j<n:
                depth+=lines[j].count('{')-lines[j].count('}')
                if '{' in lines[j]: started=True
                if started and depth<=0: break
                if not started and ';' in lines[j]:
                    proto=True; break
                j+=1
                if j-i>500: break
            if started and not proto:
                out.append((i,j,name))
            i=j+1; continue
        i+=1
    return lines,out

def lead_start(lines, ds):
    s=ds
    while s>0:
        st=lines[s-1].strip()
        if st=='':
            if s-2>=0 and lines[s-2].rstrip().endswith('*/'): s-=1; continue
            break
        if st.endswith('*/') or st.startswith(('*','/*','//')) or POP.search(lines[s-1]):
            s-=1; continue
        break
    return s

def canon(py):
    rel='src/'+py+'.c'
    return rel

def main():
    for target in TARGETS:
        path=os.path.join(ROOT,target)
        text=open(path,errors='ignore').read()
        lines, blks = blocks_of(text)
        moves=defaultdict(list); covered=set(); stats=defaultdict(int); new_canons=set()
        for (i,j,name) in blks:
            s=lead_start(lines,i)
            chunk='\n'.join(lines[s:j+1])
            pm=POP.search(chunk)
            if not pm:
                stats['kept']+=1; continue
            py=pm.group(2)
            rel=canon(py)
            full=os.path.join(ROOT,rel)
            isnew=False
            if not os.path.exists(full):
                os.makedirs(os.path.dirname(full),exist_ok=True)
                base=os.path.basename(rel)[:-2]
                open(full,'w').write(
                    f'/*\n * {base}.c — canonical C11 port of {py}.py.\n */\n\n'
                    '#include "hermes_json.h"\n#include "hermes_logger.h"\n'
                    '#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n')
                new_canons.add(rel); isnew=True
            cc=open(full,errors='ignore').read()
            if re.search(r'\b%s\s*\('%re.escape(name), cc):
                stats['dup']+=1; continue
            moves[rel].append(chunk)
            covered.update(range(s,j+1)); stats['moved']+=1
        for rel,chunks in sorted(moves.items()):
            with open(os.path.join(ROOT,rel),'a') as fh:
                fh.write('\n\n/* === dissolved from %s === */\n'%os.path.basename(target))
                fh.write('\n\n'.join(c.rstrip() for c in chunks)+'\n')
        rest='\n'.join(l for idx,l in enumerate(lines) if idx not in covered)
        open(path,'w').write(rest)
        print(f"{target}: moved={stats['moved']} kept={stats['kept']} dup={stats['dup']} new_canonicals={len(new_canons)} remain={len(rest.splitlines())} dests={len(moves)}")
    print('(dry)' if DRY else '(applied)')

main()
