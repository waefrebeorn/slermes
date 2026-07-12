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

def signature_of(body, name):
    """Extract the declaration `ret name(args)` from a handler body so the
    category header matches the real definition (return type included)."""
    for line in body.splitlines():
        s=line.strip()
        if name+'(' in s and ('{' not in s or s.index('(') < s.index('{') if '{' in s else True):
            # strip trailing '{' and anything after
            sig=s.split('{')[0].strip()
            # strip a leading static/const/inline/extern
            sig=re.sub(r'^(?:static|const|inline|extern)\s+','',sig)
            return sig+';'
    return f"void {name}(const char *args, agent_state_t *state);"

def _clang_extents(src_text):
    """Return src with comments and string/char literals blanked, so brace
    counting is never confused by braces inside strings/comments."""
    out=[]; i=0; n=len(src); mode=0  # 0 normal, 1 line-comment, 2 block-comment, 3 dquote, 4 squote
    while i<n:
        c=src[i]; nxt=src[i+1] if i+1<n else ''
        if mode==0:
            if c=='/' and nxt=='/': mode=1; out.append(' '); i+=2; continue
            if c=='/' and nxt=='*': mode=2; out.append(' '); i+=2; continue
            if c=='"': mode=3; out.append(' '); i+=1; continue
            if c=="'": mode=4; out.append(' '); i+=1; continue
            out.append(c); i+=1; continue
        elif mode==1:
            if c=='\n': mode=0; out.append('\n')
            i+=1; continue
        elif mode==2:
            if c=='*' and nxt=='/': mode=0; out.append(' '); i+=2; continue
            i+=1; continue
        elif mode==3:
            if c=='\\': i+=2; continue
            if c=='"': mode=0
            i+=1; continue
        elif mode==4:
            if c=='\\': i+=2; continue
            if c=="'": mode=0
            i+=1; continue
    return ''.join(out)

def extract_functions(src):
    """Delegate to the clang-backed extractor (scripts/cmd_extract.py)."""
    import sys
    sys.path.insert(0, os.path.join(BASE, 'scripts'))
    from cmd_extract import extract_functions as _cf
    return _cf(src)


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
    # A handler is "moveable" only if its extracted body is UNIQUE (not shared
    # with another function's body). Collided extractions (two sigs resolving
    # to the same range) share a body and would corrupt commands.c if moved, so
    # they are left in commands.c as facade handlers.
    body_to_names={}
    for nm,(b,s,e) in funcs.items():
        body_to_names.setdefault(b,set()).add(nm)
    moveable=set(h for h in handlers if h in funcs
                 and len(body_to_names[funcs[h][0]])==1)
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
    moved_all=set()
    for cat,hlist in cats.items():
        catkey='cli_cmd_'+re.sub(r'[^a-z0-9]','',cat.lower())
        members=set()
        for h in hlist:
            if h not in funcs or h not in moveable:
                if h in handlers: print("WARN leaving in commands.c:",h)
                continue
            members.add(h)
            for callee in call_graph(h,funcs,all_fns):
                if callee not in handlers and callee not in shared and callee not in keep_in_commands:
                    members.add(callee)
        guard='SLERMES_%s_H'%(catkey.upper())
        hfile=f"#ifndef {guard}\n#define {guard}\n\n#include <stdbool.h>\n#include <stdio.h>\n#include \"hermes.h\"\n\n"
        for h in sorted(members):
            if h in handlers:
                sig=signature_of(funcs[h][0], h)
                hfile+=sig+"\n"
        hfile+=f"\n#endif /* {guard} */\n"
        open(BASE+f'src/cli/{catkey}.h','w').write(hfile)
        cfile=(
            "/*\n * %s.c — %s slash-command handlers extracted from commands.c.\n"
            " * Self-contained command-category module.\n */\n\n"
            "#include <%s.h>\n#include <dirent.h>\n#include <sys/stat.h>\n"
            "#include <utmpx.h>\n#include <unistd.h>\n"
            "#include \"%s.h\"\n#include \"commands_shared.h\"\n#include \"hermes.h\"\n\n"
            % (catkey, cat, catkey, catkey)
        )
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

    # Remove handler+private-helper DEFINITIONS from commands.c via EXACT string
    # replacement (not line ranges — robust against brace imbalance). Keep the
    # COMMANDS[] table, dispatch fns, shared helpers, forward-decls intact here.
    out=src
    for cat,hlist in cats.items():
        for h in hlist:
            if h not in funcs or h not in moveable: continue
            body=funcs[h][0]
            s,e=funcs[h][1],funcs[h][2]
            if s<=TABLE_HI and e>=TABLE_LO: continue  # never touch the table
            out=out.replace(body, "", 1)
    # collapse 3+ blank lines
    out=re.sub(r'\n{3,}','\n\n',out)
    # Remove forward-declarations ONLY for handlers we actually extracted
    # (moved into a category module — declared there via the .h). Handlers we
    # failed to extract (e.g. unusual signatures) keep their forward-decl so the
    # COMMANDS[] table still resolves them.
    extracted_handlers=set()
    for cat,hlist in cats.items():
        for h in hlist:
            if h in moveable:
                extracted_handlers.add(h)
    # Remove forward-declarations for handlers we moved into category modules
    # (they are now declared in the category .h). Match both static and extern
    # forward-decls of ANY moved handler name, and drop the preceding PoP
    # comment line too.
    fwd_re=re.compile(r'^\s*(?:/\*[^\n]*\*/\s*)?(?:static|extern)\s+[^;]*\b([A-Za-z_]\w*)\s*\([^)]*\)\s*;\s*$')
    out_lines=out.splitlines()
    kept=[]
    i=0
    while i<len(out_lines):
        line=out_lines[i]
        m=fwd_re.match(line)
        if m and m.group(1) in extracted_handlers:
            # also drop a preceding standalone PoP/comment line
            if kept and kept[-1].strip().startswith('/*'):
                kept.pop()
            i+=1; continue
        kept.append(line); i+=1
    out="\n".join(kept)+"\n"
    # add category includes + shared header after #include "hermes.h"
    inc_block="\n".join([f'#include "{w}.h"' for w in written])+"\n#include \"commands_shared.h\"\n"
    out=out.replace('#include "hermes.h"\n', '#include "hermes.h"\n'+inc_block,1)

    # Make COMMANDS / g_busy_mode visible to the extracted handler modules.
    out=out.replace('static const command_def_t COMMANDS[]', 'const command_def_t COMMANDS[]')
    out=out.replace('static int g_busy_mode', 'int g_busy_mode')
    # Drop the macro/typedef defs that now live in commands_shared.h to avoid
    # redefinition errors (typedefs cannot be redefined).
    out=re.sub(r'#define CMD_COMPRESS_DEFAULT_KEEP_LAST\s+\d+\n','',out)
    out=re.sub(r'#define CMD_COMPRESS_MAX_KEEP_LAST\s+\d+\n','',out)
    out=re.sub(r'typedef struct\s*\{[^}]*\}\s*handoff_entry_t;\s*','',out)
    out=re.sub(r'typedef struct\s*\{[^}]*\}\s*list_t;\s*','',out)
    open(BASE+SRC,'w').write(out)

    # Register the new cli_cmd_*.o objects in build/objects.mk (CLI_OBJ) so they
    # are compiled and linked. Idempotent: only adds entries not already present.
    obj_mk=os.path.join(BASE,'build/objects.mk')
    mk=open(obj_mk).read()
    obj_lines=mk.splitlines()
    new_objs=[f'src/cli/{w}.o' for w in written]
    for i,l in enumerate(obj_lines):
        if l.startswith('CLI_OBJ ='):
            existing=l
            added=False
            for o in new_objs:
                if o not in existing:
                    existing=existing.rstrip()+' '+o
                    added=True
            if added:
                obj_lines[i]=existing
            break
    open(obj_mk,'w').write('\n'.join(obj_lines)+'\n')

    # (Re)create the shared header so the split is self-contained.
    shared_h=(
        "/*\n * commands_shared.h - symbols shared between commands.c (dispatch\n"
        " * facade) and the cli_cmd_<category>.c handler modules.\n"
        " */\n"
        "#ifndef SLERMES_COMMANDS_SHARED_H\n#define SLERMES_COMMANDS_SHARED_H\n\n"
        "#include \"hermes.h\"\n#include <dirent.h>\n#include <sys/stat.h>\n"
        "#include <utmpx.h>\n#include <unistd.h>\n\n"
        "#define CMD_COMPRESS_DEFAULT_KEEP_LAST 2\n"
        "#define CMD_COMPRESS_MAX_KEEP_LAST 100\n\n"
        "extern int g_busy_mode;\n"
        "extern int g_verbose;\n"
        "extern char *g_home_channel;\n"
        "extern char *g_current_skin;\n"
        "extern int g_statusbar_on;\n"
        "extern int g_indicator_style;\n"
        "extern const command_def_t COMMANDS[];\n\n"
        "typedef struct {\n    char *id;\n    char *platform;\n"
        "    char *session_id;\n    char *requester;\n    char *status;\n} handoff_entry_t;\n\n"
        "typedef struct {\n    void **items;\n    int count;\n"
        "    int capacity;\n} list_t;\n\n"
        "#endif /* SLERMES_COMMANDS_SHARED_H */\n"
    )
    open(BASE+'src/cli/commands_shared.h','w').write(shared_h)

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
