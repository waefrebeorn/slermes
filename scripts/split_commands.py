import re, json, sys, os
from collections import defaultdict

SRC='src/cli/commands.c'
BASE='/home/wubu/hermes-agent-dev/slermes/'

def in_str(s,i):
    j=0; inst=None; lc=False; blk=False
    while j<i and j<len(s):
        c=s[j]; nxt=s[j+1] if j+1<len(s) else ''
        if lc:
            if c=='\n': lc=False
            j+=1; continue
        if blk:
            if c=='*' and nxt=='/': blk=False; j+=2; continue
            j+=1; continue
        if inst:
            if c=='\\': j+=2; continue
            if c==inst: inst=None
            j+=1; continue
        if c=='/' and nxt=='/': lc=True; j+=2; continue
        if c=='/' and nxt=='*': blk=True; j+=2; continue
        if c in ('"',"'",'`'): inst=c; j+=1; continue
        j+=1
    return inst is not None or lc or blk

def extract_functions(src):
    lines=src.splitlines(); n=len(lines)
    start_re=re.compile(r'^(?:static\s+)?(?:const\s+)?[a-zA-Z_][\w\s\*]*?\b([a-z_][\w]*)\s*\(')
    starts=[]
    for idx,l in enumerate(lines):
        m=start_re.match(l)
        if m:
            rest="\n".join(lines[idx:idx+40])
            if ')' in rest and '{' in (rest[rest.index(')'):] if ')' in rest else ''):
                starts.append((idx,m.group(1)))
    funcs={}
    for idx,name in starts:
        depth=0; k=idx; pd=0; fp=False
        while k<min(n,idx+60):
            for kk,ch in enumerate(lines[k]):
                if in_str(lines[k],kk): continue
                if ch=='(': pd+=1; fp=True
                elif ch==')':
                    pd-=1
                    if fp and pd==0: break
            else:
                k+=1; continue
            break
        started=False
        while k<n:
            line=lines[k]
            for kk,ch in enumerate(line):
                if in_str(line,kk): continue
                if ch=='{': depth+=1; started=True
                elif ch=='}': depth-=1
            if started and depth==0:
                body="\n".join(lines[idx:k+1])+"\n"
                funcs[name]=(body,idx+1,k+1)
                break
            k+=1
    # attach preceding comment block
    result={}
    lines2=src.splitlines()
    for name,(body,start,end) in funcs.items():
        cstart=start-2
        while cstart>=0:
            st=lines2[cstart].strip()
            if st.startswith('/*') or st.startswith('*') or st.startswith('//'):
                cstart-=1; continue
            break
        if cstart+1<=start-2 and lines2[cstart+1].strip().startswith('/*'):
            pre="\n".join(lines2[cstart+1:start-1])+"\n"
            result[name]=(pre+body,cstart+2,end)
        else:
            result[name]=(body,start,end)
    return result

def body_of(name, funcs):
    return funcs.get(name,(None,0,0))[0]

def call_graph(name, funcs, all_fns):
    # BFS over called local functions
    seen=set(); stack=[name]; called_local=set()
    while stack:
        cur=stack.pop()
        b=body_of(cur,funcs)
        if not b: continue
        for callee in set(re.findall(r'\b([a-z_][\w]*)\s*\(', b)):
            if callee in all_fns and callee!=cur and callee not in seen:
                seen.add(callee); called_local.add(callee); stack.append(callee)
    return called_local

def main():
    cats=json.load(open('scripts/cmd_categories.json'))
    src=open(BASE+SRC).read()
    funcs=extract_functions(src)
    all_fns=set(funcs.keys())
    handlers=set(h for hs in cats.values() for h in hs)
    # shared helpers (used by >1 handler) stay in commands.c
    helper_users=defaultdict(set)
    for h in handlers:
        for callee in call_graph(h,funcs,all_fns):
            if callee not in handlers:
                helper_users[callee].add(h)
    shared_helpers=set(f for f,u in helper_users.items() if len(u)>1)
    print("shared helpers (stay in commands.c):", sorted(shared_helpers))

    # For each category: collect handlers + their private (non-shared) helpers
    written=[]
    prune_ranges=[]
    for cat, hlist in cats.items():
        catkey='cli_cmd_'+re.sub(r'[^a-z0-9]','',cat.lower())
        members=set()
        for h in hlist:
            if h not in funcs:
                print("WARN handler missing:",h); continue
            members.add(h)
            for callee in call_graph(h,funcs,all_fns):
                if callee not in handlers and callee not in shared_helpers:
                    members.add(callee)
        # also include helper-of-helper chains already captured by call_graph transitive
        # write module
        guard='SLERMES_%s_H'%(catkey.upper())
        hfile=f"#ifndef {guard}\n#define {guard}\n\n#include <stdbool.h>\n#include <stdio.h>\n#include \"hermes.h\"\n\n"
        # forward declare handlers
        for h in sorted(members):
            if h in handlers:
                hfile+=f"void {h}(const char *args, agent_state_t *state);\n"
        hfile+=f"\n#endif /* {guard} */\n"
        open(BASE+f'src/cli/{catkey}.h','w').write(hfile)
        cfile=f"/*\n * {catkey}.c — {cat} slash-command handlers extracted from commands.c.\n * Self-contained command-category module.\n */\n\n#include \"{catkey}.h\"\n#include \"hermes.h\"\n\n"
        for h in sorted(members):
            body=funcs[h][0]
            body_clean=re.sub(r'^(static\s+)+','',body,count=1,flags=re.MULTILINE)
            body_clean=re.sub(r'\bstatic\s+',' ',body_clean)
            cfile+=body_clean+"\n"
            prune_ranges.append(funcs[h][1:3])
        open(BASE+f'src/cli/{catkey}.c','w').write(cfile)
        written.append((cat,catkey,len(members)))
        print(f"  {catkey}: {len(members)} members")
    # prune original: delete extracted members (keep dispatch table, shared state, shared helpers)
    srclines=src.splitlines()
    for s,e in sorted(prune_ranges,reverse=True):
        del srclines[s-1:e]
    out="\n".join(srclines)+"\n"
    out=re.sub(r'\n{4,}','\n\n\n',out)
    # add includes for all category headers after #include "hermes.h"
    incs="\n".join([f'#include "cli_cmd_{re.sub(chr(91)+chr(93),"","")}' for _ in []])  # placeholder
    open(BASE+SRC,'w').write(out)
    print("PRUNE done. commands.c now", len(srclines), "lines (was", len(src.splitlines()),")")
    # save written list for next step (adding includes)
    json.dump(written, open('scripts/cmd_written.json','w'))

if __name__=='__main__':
    main()
