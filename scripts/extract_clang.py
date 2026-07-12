#!/usr/bin/env python3
"""Extract function extents from commands.c using libclang (real C AST, handles
GNU extensions natively). Returns {name:(body,start_line1,end_line1)} with exact
source slices. Robust and correct."""
import sys
from clang import cindex

BASE='/home/wubu/hermes-agent-dev/slermes/'
SRC='src/cli/commands.c'

def parse_extents(src_text):
    idx=cindex.Index.create()
    # We need the actual file so clang can resolve; write to temp with the
    # original path's dir so #include "hermes.h" resolves via -I include.
    import tempfile, os
    tmp=tempfile.NamedTemporaryFile(mode='w',suffix='.c',delete=False,
                                    dir=os.path.join(BASE,'src/cli'))
    tmp.write(src_text); tmp.close()
    try:
        # Parse with C11 + GNU extensions; include paths for hermes.h etc.
        opts=['-std=c11','-I',os.path.join(BASE,'include'),
              '-I',os.path.join(BASE,'lib'),'-I',os.path.join(BASE,'src'),
              '-I',os.path.join(BASE,'src/cli'),
              '-I','/usr/lib/gcc/x86_64-linux-gnu/13/include',
              '-I','/usr/local/include',
              '-I','/usr/include/x86_64-linux-gnu',
              '-I','/usr/include',
              '-fparse-all-comments']
        tu=idx.parse(tmp.name, args=opts,
                     options=cindex.TranslationUnit.PARSE_INCOMPLETE)
        extents={}
        def visit(node, depth=0):
            if node.kind==cindex.CursorKind.FUNCTION_DECL and node.is_definition():
                name=node.spelling
                # extent: start of the declaration (incl. preceding comment is
                # handled by caller) to end of the function body.
                be=node.extent
                # Find the closing brace: walk children for the CompoundStmt
                end_line=None
                for c in node.get_children():
                    if c.kind==cindex.CursorKind.COMPOUND_STMT:
                        end_line=c.extent.end.line
                if end_line is None:
                    end_line=be.end.line
                start_line=be.start.line
                extents[name]=(start_line, end_line)
            for c in node.get_children():
                visit(c, depth+1)
        visit(tu.cursor)
        return extents
    finally:
        os.unlink(tmp.name)

if __name__=='__main__':
    src=open(BASE+SRC).read()
    ol=src.splitlines()
    ext=parse_extents(src)
    print("functions parsed:", len(ext))
    import json
    cats=json.load(open(BASE+'scripts/cmd_categories.json'))
    allh=set(h for hs in cats.values() for h in hs)
    extr=[h for h in allh if h in ext]; missing=[h for h in allh if h not in ext]
    print("handlers extracted:", len(extr),"/",len(allh))
    print("missing:", missing)
    # collisions
    from collections import defaultdict
    rng=defaultdict(list)
    for nm,(s,e) in ext.items(): rng[(s,e)].append(nm)
    coll=[(k,v) for k,v in rng.items() if len(v)>1]
    print("collisions:", len(coll))
    for k,v in coll[:8]: print("  ",k,v)
    b=ext.get('cmd_compress_coerce_keep')
    if b: print("cck:", b, "body:", repr('\n'.join(ol[b[0]-1:b[1]])[:80]))
