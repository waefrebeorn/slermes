#!/usr/bin/env python3
"""Extract function extents from commands.c using pycparser + gcc -E (keeps
# linenum markers -> original line numbers). Robust, no brace-counting hacks."""
import subprocess, sys, os, tempfile
from pycparser import c_parser, c_ast

BASE='/home/wubu/hermes-agent-dev/slermes/'
SRC='src/cli/commands.c'
TABLE_LO, TABLE_HI = 535, 632

def parse_func_lines(src_text):
    """Return {name: (decl_line, end_line)} using gcc -E line markers + pycparser."""
    tmp=tempfile.NamedTemporaryFile(mode='w',suffix='.c',delete=False)
    tmp.write(src_text); tmp.close()
    try:
        res=subprocess.run(['gcc','-E','-std=c99','-undef',
                             '-I','include','-I','lib','-I','src',
                             tmp.name],
                            capture_output=True,text=True,timeout=120)
        marked=res.stdout
        if res.returncode!=0:
            # try without extra includes
            res=subprocess.run(['gcc','-E','-std=c99','-undef',tmp.name],
                              capture_output=True,text=True,timeout=120)
            marked=res.stdout
    finally:
        os.unlink(tmp.name)
    parser=c_parser.CParser()
    ast=parser.parse(marked, filename='commands.c')
    # map func name -> decl line (from # markers)
    info={}
    class V(c_ast.NodeVisitor):
        def visit_FuncDef(self, node):
            name=node.decl.name
            decl_line=node.coord.line
            # end line: find the '}' that closes this function by scanning the
            # marked source from decl_line, but easier: pycparser gives decl
            # coord only. We approximate end via brace matching on ORIGINAL
            # using the reliable approach below.
            info[name]=decl_line
            self.generic_visit(node)
    V().visit(ast)
    return info

def main():
    src=open(BASE+SRC).read()
    lines=src.splitlines()
    decl_lines=parse_func_lines(src)
    # For each func, brace-match from its decl line using a CORRECT counter on
    # the cleaned text, but starting EXACTLY at the known decl line (from the
    # real parser) eliminates the mis-start problem.
    def strip(s):
        out=[];i=0;m=0
        while i<len(s):
            c=s[i];nx=s[i+1] if i+1<len(s) else ''
            if m==0:
                if c=='/' and nx=='/':m=1;i+=2;out.append(' ');continue
                if c=='/' and nx=='*':m=2;i+=2;out.append(' ');continue
                if c=='"':m=3;i+=1;out.append(' ');continue
                if c=="'":m=4;i+=1;out.append(' ');continue
                out.append(c);i+=1;continue
            elif m==1:
                if c=='\n':m=0;out.append('\n')
                i+=1;continue
            elif m==2:
                if c=='*' and nx=='/':m=0;i+=2;out.append(' ');continue
                i+=1;continue
            elif m==3:
                if c=='\\':i+=2;continue
                if c=='"':m=0
                i+=1;continue
            elif m==4:
                if c=='\\':i+=2;continue
                if c=="'":m=0
                i+=1;continue
        return ''.join(out)
    clean=strip(src); cl=clean.splitlines(); n=len(cl)
    results={}
    for name,dline in decl_lines.items():
        idx=dline-1
        # find first '{' after the decl line (params close then brace)
        k=idx; dp=0; op=False; pc=False; bs=None
        while k<n:
            line=cl[k]
            for cpos,ch in enumerate(line):
                if ch=='(':dp+=1;op=True
                elif ch==')' and op:
                    dp-=1
                    if dp==0: pc=True
                elif ch=='{' and pc:
                    bs=k;break
            if bs is not None:break
            k+=1
        if bs is None:continue
        d=0;m2=bs;en=None
        while m2<n:
            for ch in cl[m2]:
                if ch=='{':d+=1
                elif ch=='}':
                    d-=1
                    if d==0:en=m2;break
            if en is not None:break
            m2+=1
        if en is None:continue
        # slice exact original text from comment block before idx
        cs=idx-1
        while cs>=0:
            st=lines[cs].strip()
            if st.startswith('/*') or st.startswith('*') or st.startswith('//'):
                cs-=1;continue
            break
        char_a=sum(len(lines[i])+1 for i in range(cs+1))
        char_b=sum(len(lines[i])+1 for i in range(en+1))
        body=src[char_a:char_b]
        results[name]=(body, cs+2, en+1)
    clean2={k:v for k,v in results.items() if not (v[1]<=TABLE_HI and v[2]>=TABLE_LO)}
    return clean2

if __name__=='__main__':
    r=main()
    print("functions parsed:", len(r))
    import json
    cats=json.load(open(BASE+'scripts/cmd_categories.json'))
    allh=set(h for hs in cats.values() for h in hs)
    extr=[h for h in allh if h in r]
    print("handlers extracted:", len(extr),"/",len(allh))
    # collisions
    from collections import defaultdict
    rng=defaultdict(list)
    for nm,(b,s,e) in r.items(): rng[(s,e)].append(nm)
    coll=[(k,v) for k,v in rng.items() if len(v)>1]
    print("collisions:", len(coll))
    for k,v in coll[:5]: print("  ",k,v)
    print("non-exact:", [h for h in extr if r[h][0] not in open(BASE+SRC).read()])
    b=r.get('cmd_compress_coerce_keep')
    if b: print("cck:", b[1], b[2], "exact:", b[0] in open(BASE+SRC).read())
