#!/usr/bin/env python3
"""Split commands.c by command-category into cli_cmd_<cat>.c modules.

GUARANTEES:
- The COMMANDS[] dispatch table (lines TABLE_LO..TABLE_HI) is NEVER moved or
  deleted. It stays in commands.c.
- Dispatch infrastructure (commands_count/list_json/dispatch) stays in commands.c.
- Shared helpers (used by >1 handler) stay in commands.c.
- Each handler + its PRIVATE helpers (used only by that handler) move into
  cli_cmd_<cat>.c. commands.c includes all cli_cmd_<cat>.h so the dispatch
  table's .handler=cmd_x references resolve.
- Forward-declarations of moved handlers are removed from commands.c (the
  category header declares them now).
"""
import re, json, os

BASE = '/home/wubu/hermes-agent-dev/slermes/'
SRC  = 'src/cli/commands.c'
TABLE_LO, TABLE_HI = 535, 632  # command_def_t COMMANDS[] = { ... };

def in_str(s, i):
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
        if m: starts.append((idx,m.group(1)))
    funcs={}
    for idx,name in starts:
        if idx+1>TABLE_HI and idx<TABLE_LO:  # never start inside the table
            pass
        # find opening brace
        depth=0; k=idx; fp=False; started=False
        while k<min(n,idx+80):
            line=lines[k]
            for kk,ch in enumerate(line):
                if in_str(line,kk): continue
                if ch=='(': fp=True; depth+=1
                elif ch==')' and fp: depth-=1
                elif ch=='{': started=True; depth=0  # reset at brace
                elif ch=='}' and started:
                    funcs[name]=(idx+1,k+1); break
            if name in funcs: break
            k+=1
        # attach preceding comment block (/* ... */ or // lines)
        ls=src.splitlines()
        cs=idx-1
        while cs>=0:
            st=ls[cs].strip()
            if st.startswith('/*') or st.startswith('*') or st.startswith('//'): cs-=1; continue
            break
        body="\n".join(ls[cs+1:idx]) + "\n" + "\n".join(ls[idx:funcs[name][1]]) + "\n" if name in funcs else None
        if body: funcs[name]=(body, cs+2, funcs[name][1])
    return funcs

def call_graph(name, funcs, all_fns):
    seen=set(); stack=[name]; local=set()
    while stack:
        cur=stack.pop()
        b=funcs.get(cur,(None,0,0))[0]
        if not b: continue
        for callee in set(re.findall(r'\b([a-z_][\w]*)\s*\(', b)):
            if callee in all_fns and callee!=cur and callee not in seen:
                seen.add(callee); local.add(callee); stack.append(callee)
    return local

def main():
    cats=json.load(open(os.path.join(BASE,'scripts/cmd_categories.json')))
    src=open(BASE+SRC).read()
    lines=src.splitlines()
    # Validate the table is where we think
    assert 'COMMANDS[] = {' in lines[TABLE_LO-1] or 'COMMANDS[] = {' in lines[TABLE_LO-1], "table start not at expected line"
    funcs=extract_functions(src)
    all_fns=set(funcs.keys())
    handlers=set(h for hs in cats.values() for h in hs)
    # dispatch infrastructure stays in commands.c
    keep_in_commands=set(['commands_count','commands_list_json','commands_dispatch',
                          'commands_register','commands_audit','commands_init'])
    # shared helpers (used by >1 handler) stay
    helper_users={}
    for h in handlers:
        for callee in call_graph(h,funcs,all_fns):
            if callee not in handlers and callee not in keep_in_commands:
                helper_users.setdefault(callee,set()).add(h)
    shared=set(f for f,u in helper_users.items() if len(u)>1)
    print("shared helpers kept in commands.c:", sorted(shared))

    written=[]; prune=[]
    for cat,hlist in cats.items():
        catkey='cli_cmd_'+re.sub(r'[^a-z0-9]','',cat.lower())
        members=set()
        for h in hlist:
            if h not in funcs:
                print("WARN handler missing:",h); continue
            members.add(h)
            for callee in call_graph(h,funcs,all_fns):
                if callee not in handlers and callee not in shared and callee not in keep_in_commands:
                    members.add(callee)
        guard='SLERMES_%s_H'%(catkey.upper())
        hfile=f"#ifndef {guard}\n#define {guard}\n\n#include <stdbool.h>\n#include <stdio.h>\n#include \"hermes.h\"\n\n"
        for h in sorted(members):
            if h in handlers:
                hfile+=f"void {h}(const char *args, agent_state_t *state);\n"
        hfile+=f"\n#endif /* {guard} */\n"
        open(BASE+f'src/cli/{catkey}.h','w').write(hfile)
        cfile=f"/*\n * {catkey}.c — {cat} slash-command handlers extracted from commands.c.\n * Self-contained command-category module.\n */\n\n#include \"{catkey}.h\"\n#include \"hermes.h\"\n\n"
        for h in sorted(members):
            body=funcs[h][0]
            body=re.sub(r'^(static\s+)+','',body,count=1,flags=re.MULTILINE)
            body=re.sub(r'\bstatic\s+',' ',body)
            cfile+=body+"\n"
            s,e=funcs[h][1],funcs[h][2]
            # never prune across the table
            if not (s<=TABLE_HI and e>=TABLE_LO):
                prune.append((s,e))
        open(BASE+f'src/cli/{catkey}.c','w').write(cfile)
        written.append(catkey)
        print(f"  {catkey}: {len(members)} members")

    # prune handler+private-helper defs from commands.c (keep table, dispatch, shared)
    ls=list(lines)
    for s,e in sorted(prune,reverse=True):
        if s<=TABLE_HI and e>=TABLE_LO:  # safety: never delete table
            continue
        del ls[s-1:e]
    out="\n".join(ls)+"\n"
    out=re.sub(r'\n{4,}','\n\n\n',out)
    # remove forward-declarations block for moved handlers (now in category headers)
    # (find the block of `static void cmd_*(...);` lines and drop them)
    out=re.sub(r'\n(?:/\* Port of Python[^\n]*\n)?static (?:void|int|bool|char \*|const char \*)\s+cmd_\w+\([^;]*\);\n','\n',out)
    # add category includes after #include "hermes.h"
    inc_block="\n".join([f'#include "{w}.h"' for w in written])+"\n"
    out=out.replace('#include "hermes.h"\n', '#include "hermes.h"\n'+inc_block,1)
    open(BASE+SRC,'w').write(out)

    # ASSERTIONS
    assert 'COMMANDS[] = {' in open(BASE+SRC).read(), "FATAL: COMMANDS table lost!"
    assert 'commands_dispatch' in open(BASE+SRC).read(), "FATAL: dispatch lost!"
    # check no cli_cmd file stole the table
    for w in written:
        c=open(BASE+f'src/cli/{w}.c').read()
        assert 'COMMANDS[] = {' not in c, f"FATAL: table leaked into {w}.c"
    print("ASSERTIONS PASSED. commands.c now", len(open(BASE+SRC).readlines()), "lines")
    json.dump(written, open(os.path.join(BASE,'scripts/cmd_written.json'),'w'))

if __name__=='__main__':
    main()
