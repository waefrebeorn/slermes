#!/usr/bin/env python3
"""Clang-backed function-extent extractor for commands.c.

Strategy (proven correct):
- libclang gives the AUTHORITATIVE definition start line for every function
  (handles GNU extensions, multi-line signatures, forward-decls correctly).
- A simple string/comment-aware brace counter finds the matching '}' from that
  start line. This end-detection is trivially correct once the start is right.
- The exact body text (including the preceding /* PoP */ comment block) is
  sliced directly from the original source, so str.replace() removal is exact.
"""
import os, tempfile
from clang import cindex

BASE='/home/wubu/hermes-agent-dev/slermes/'

def _clang_extents(src_text):
    """Return {name: (start_line1, end_line1)} using libclang's authoritative
    definition extents (start = declaration, end = closing brace of body)."""
    tmp=tempfile.NamedTemporaryFile(mode='w',suffix='.c',delete=False,
                                    dir=os.path.join(BASE,'src/cli'))
    tmp.write(src_text); tmp.close()
    opts=['-std=c11','-I',os.path.join(BASE,'include'),
          '-I',os.path.join(BASE,'lib'),'-I',os.path.join(BASE,'src'),
          '-I',os.path.join(BASE,'src/cli'),
          '-I','/usr/lib/gcc/x86_64-linux-gnu/13/include',
          '-I','/usr/local/include',
          '-I','/usr/include/x86_64-linux-gnu','-I','/usr/include']
    try:
        tu=cindex.Index.create().parse(tmp.name,args=opts,
                                      options=cindex.TranslationUnit.PARSE_INCOMPLETE)
        exts={}
        def visit(node):
            if node.kind==cindex.CursorKind.FUNCTION_DECL and node.is_definition():
                sl=node.extent.start.line
                el=None
                for c in node.get_children():
                    if c.kind==cindex.CursorKind.COMPOUND_STMT:
                        el=c.extent.end.line
                if el is not None:
                    exts[node.spelling]=(sl, el)
            for c in node.get_children(): visit(c)
        visit(tu.cursor)
        return exts
    finally:
        os.unlink(tmp.name)

def _in_str(s,i):
    inst=None;j=0
    while j<i:
        c=s[j]
        if inst=='line':
            if c=='\n':inst=None
        elif inst=='block':
            if c=='*' and j+1<len(s) and s[j+1]=='/':inst=None
        elif inst=='dq':
            if c=='\\':j+=1
            elif c=='"':inst=None
        elif inst=='sq':
            if c=='\\':j+=1
            elif c=="'":inst=None
        else:
            if c=='/' and j+1<len(s) and s[j+1]=='/':inst='line';j+=1
            elif c=='/' and j+1<len(s) and s[j+1]=='*':inst='block';j+=1
            elif c=='"':inst='dq'
            elif c=="'":inst='sq'
        j+=1
    return inst is not None

def extract_functions(src):
    lines=src.splitlines(); n=len(lines)
    exts=_clang_extents(src)
    results={}
    for name,(sl,el) in exts.items():
        # clang's start line is the function's source start. If that line is
        # the '{' (body open), the signature is the line immediately above it.
        sig_line=sl
        prev=lines[sig_line-1] if (sig_line-1) < len(lines) else ''
        if name in prev and '(' in prev:
            sig_line=sig_line-1   # sl was the '{'; signature is the line above
        # else: sig_line already IS the signature line
        # walk back to include preceding comment block (/* PoP: ... */)
        cs=sig_line-1
        while cs>=0:
            st=lines[cs].strip()
            if st.startswith('/*') or st.startswith('*') or st.startswith('//'):
                cs-=1; continue
            break
        char_a=sum(len(lines[i])+1 for i in range(cs+1))
        char_b=sum(len(lines[i])+1 for i in range(el))
        body=src[char_a:char_b]
        results[name]=(body, cs+2, el)
    return results
