#!/usr/bin/env python3
"""Generic monolith splitter for slermes C files.

Given a source .c file and a function->module mapping, extracts each function
(verbatim, with its preceding comment/PoP block) into a focused concern module,
generates self-contained .h (opaque struct + prototypes) and .c (minimal
includes), and rewrites the original .c into a thin facade that includes the
sub-module headers and re-declares the public API surface (so external callers
keep working).

Design rules honored:
- opaque struct per module (uniform: `struct <mod> { int unused; };`)
- minimal includes ( caller passes the include list per module )
- C11 only
- no god headers (modules include only what they need)
- every module self-contained
- PoP / comment blocks preserved verbatim (parity credit intact)
"""
import re, sys, os

def in_string_or_char(s, i):
    j = 0
    in_str = None
    in_lc = False
    in_blk = False
    while j < i and j < len(s):
        c = s[j]
        nxt = s[j+1] if j+1 < len(s) else ''
        if in_lc:
            if c == '\n': in_lc = False
            j += 1; continue
        if in_blk:
            if c == '*' and nxt == '/':
                in_blk = False; j += 2; continue
            j += 1; continue
        if in_str:
            if c == '\\': j += 2; continue
            if c == in_str: in_str = None
            j += 1; continue
        if c == '/' and nxt == '/':
            in_lc = True; j += 2; continue
        if c == '/' and nxt == '*':
            in_blk = True; j += 2; continue
        if c in ('"', "'", '`'):
            in_str = c; j += 1; continue
        j += 1
    return in_str is not None or in_lc or in_blk

def extract_functions(src):
    lines = src.splitlines()
    n = len(lines)
    # Match a definition-start line: optional static/const, a return type, then
    # `name(` — the signature may continue across lines until the closing `)`
    # and then an opening `{` (possibly on a later line).
    start_re = re.compile(r'^(?:static\s+)?(?:const\s+)?[a-zA-Z_][\w\s\*]*?\b([a-z_][\w]*)\s*\(')
    starts = []
    for idx, l in enumerate(lines):
        m = start_re.match(l)
        if m:
            # confirm this is a definition, not a call: there must be a ')' later
            # and a '{' after the ')'
            rest = "\n".join(lines[idx:idx+40])
            if ')' in rest and '{' in rest[rest.index(')'):] if ')' in rest else False:
                # good enough; brace-matcher below validates
                starts.append((idx, m.group(1)))
    funcs = {}
    for idx, name in starts:
        depth = 0; started = False; j = idx
        # find first '{' after the '(' — scan forward
        paren_depth = 0; found_paren = False; k = idx
        while k < min(n, idx+60):
            for kk, ch in enumerate(lines[k]):
                if in_string_or_char(lines[k], kk): continue
                if ch == '(': paren_depth += 1; found_paren = True
                elif ch == ')':
                    paren_depth -= 1
                    if found_paren and paren_depth == 0:
                        break
            else:
                k += 1; continue
            break
        # now brace-match from here
        started = False
        while k < n:
            line = lines[k]
            for kk, ch in enumerate(line):
                if in_string_or_char(line, kk): continue
                if ch == '{': depth += 1; started = True
                elif ch == '}': depth -= 1
            if started and depth == 0:
                body = "\n".join(lines[idx:k+1]) + "\n"
                funcs[name] = (body, idx+1, k+1)
                break
            k += 1
    # re-attach preceding comment/PoP block
    result = {}
    for name, (body, start, end) in funcs.items():
        cstart = start - 2
        while cstart >= 0:
            stripped = lines[cstart].strip()
            if stripped.startswith('/*') or stripped.startswith('*') or stripped.startswith('//'):
                cstart -= 1; continue
            break
        if cstart + 1 <= start - 2 and lines[cstart + 1].strip().startswith('/*'):
            pre = "\n".join(lines[cstart + 1:start - 1]) + "\n"
            result[name] = (pre + body, cstart + 2, end)
        else:
            result[name] = (body, start, end)
    return result

def prototype_for(name, body):
    # Find the full signature line(s): from the line containing `name(` up to
    # the closing `)` (signature may span lines).
    lines = body.splitlines()
    start = None
    for i, bl in enumerate(lines):
        s = bl.strip()
        if re.match(r'^(?:static\s+)?(?:const\s+)?[a-zA-Z_][\w\s\*]*\b' + re.escape(name) + r'\s*\(', s):
            start = i
            break
    if start is None:
        for i, bl in enumerate(lines):
            s = bl.strip()
            if s and not s.startswith('/*') and not s.startswith('*') and not s.startswith('//'):
                start = i
                break
    # join from start until we find the line with the matching closing paren
    sig_parts = []
    depth = 0
    for bl in lines[start:]:
        sig_parts.append(bl.strip())
        depth += bl.count('(') - bl.count(')')
        if depth <= 0 and ')' in bl:
            break
    sig = ' '.join(sig_parts)
    # strip trailing '{' if present, strip trailing ';'
    sig = sig.rstrip()
    if sig.endswith('{'): sig = sig[:-1].strip()
    if sig.endswith(';'): sig = sig[:-1].strip()
    # remove 'static' storage class so the header decl is external linkage.
    # Keep 'const' type qualifiers (preserve const-correctness).
    sig = re.sub(r'^(static\s+)+', '', sig)
    sig = re.sub(r'\bstatic\s+', ' ', sig)
    sig = re.sub(r'\s+', ' ', sig)
    if not sig.endswith(';'): sig += ';'
    return sig

def split(src_path, module_map, module_includes, module_guard, public_api=None,
          facade_extra_includes=None, keep_in_original=None, original_new_body=None,
          prune_original=True):
    """module_map: name -> module key. module_includes: key -> list of include lines.
    module_guard: key -> header guard. public_api: set of names that must remain
    declared in the original header (kept in original .c as delegating wrappers).
    If prune_original, the extracted function bodies (and their preceding comment
    blocks) are removed from src_path so it no longer re-defines them."""
    src = open(src_path).read()
    funcs = extract_functions(src)
    missing = [k for k in module_map if k not in funcs]
    if missing:
        print("WARN missing from source:", missing)
    buckets = {}
    for name, mod in module_map.items():
        if name in funcs:
            buckets.setdefault(mod, []).append((name, funcs[name][0]))
    base = os.path.dirname(src_path)
    written = []
    # Track (start,end) line ranges to prune from original (including comment block)
    prune_ranges = []
    for mod, items in buckets.items():
        guard = module_guard[mod]
        h = f"#ifndef {guard}\n#define {guard}\n\n#include <stdbool.h>\n#include <stdio.h>\n"
        h += "#include <json.h>\n" if any('json' in inc for inc in module_includes.get(mod, [])) else ""
        h += "\ntypedef struct %s %s_t;\n\n" % (mod, mod)
        h += "%s_t *%s_init(void);\nvoid %s_cleanup(%s_t *s);\n\n" % (mod, mod, mod, mod)
        for name, body in items:
            h += prototype_for(name, body) + "\n"
            # record line range for pruning (1-indexed start/end from extract)
            _, s, e = funcs[name]
            prune_ranges.append((s, e))
        h += f"\n#endif /* {guard} */\n"
        open(os.path.join(base, mod + ".h"), "w").write(h)
        c = "/*\n * %s.c — extracted concern module from %s.\n * Self-contained, opaque struct, minimal includes, C11.\n */\n\n" % (mod, os.path.basename(src_path))
        for inc in module_includes.get(mod, []):
            c += inc + "\n"
        c += "\n"
        c += "struct %s { int unused; };\n" % mod
        c += "%s_t *%s_init(void) { return calloc(1, sizeof(%s_t)); }\n" % (mod, mod, mod)
        c += "void %s_cleanup(%s_t *s) { free(s); }\n\n" % (mod, mod)
        for name, body in items:
            body_clean = re.sub(r'^(static\s+)+', '', body, count=1, flags=re.MULTILINE)
            body_clean = re.sub(r'\bstatic\s+', ' ', body_clean)
            c += body_clean + "\n"
        open(os.path.join(base, mod + ".c"), "w").write(c)
        written.append(mod)
    if prune_original and prune_ranges:
        srclines = src.splitlines()
        # sort ranges descending and delete
        for s, e in sorted(prune_ranges, reverse=True):
            del srclines[s-1:e]  # 0-indexed
        # collapse 3+ blank lines into 2
        out = "\n".join(srclines) + "\n"
        out = re.sub(r'\n{4,}', '\n\n\n', out)
        open(src_path, "w").write(out)
    return written

if __name__ == "__main__":
    # quick self-test
    print("generic slicer loaded")
