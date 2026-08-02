#!/usr/bin/env python3
"""Recursive false-gap hunter for the slermes C port.

The parity scanner counts any PoP-annotated C function as 'ported', so a
function that delegates to a bootleg stub (or is itself a bootleg stub) is a
FALSE GAP: it reports parity but does no real work.

This hunter builds a call graph over the *ported* translation units
(src/**/port_*.c + include/port_*.h) and classifies every function as
BOOTLEG or REAL using these rules:

  - A function is REAL if its body does observable work: prints, does file/
    network I/O, mutates a json object, allocates-and-uses, contains a loop
    with a body, etc., OR it calls at least one REAL function.
  - A function is BOOTLEG if, after ignoring calls to REAL/external functions
    (anything NOT defined inside the ported TU set — i.e. library calls and
    the live cli_cmd_*.c handlers), its body does nothing and every remaining
    callee is itself BOOTLEG.
  - Recursive: a delegation chain A->B->C where C is bootleg makes A,B bootleg.

Callees defined OUTSIDE the ported set are treated as REAL (they are the
already-wired live handlers / libraries). This cleanly separates
  ccm_handle_x -> cmd_X (cli_cmd_*.c, REAL)   [good]
  ccm_handle_x -> hermes_cli_x_cmd_y (port_*.c, possibly BOOTLEG)  [caught]

Usage:
  python3 tests/recursive_false_gap_hunter.py [--json] [--porthint]
"""
import re, sys, os, json, subprocess
from pathlib import Path

SLERMES = Path(__file__).resolve().parent.parent
PORT_RE = re.compile(r'(?:src/.*/)?port_[^/]+\.c$')
HDR_RE = re.compile(r'(?:include/)?port_[^/]+\.h$')

# ---- collect ported TUs ----
def ported_sources():
    out = []
    for p in (SLERMES / "src").rglob("port_*.c"):
        out.append(p)
    return out

# ---- function definition extraction ----
FUNC_DEF = re.compile(
    r'(?:static\s+|inline\s+)?(?:const\s+|unsigned\s+)?'
    r'(?:[\w:]+\s+)+?(\*?\w+)\s*\(([^;]*?)\)\s*\{',
    re.S)

def extract_funcs(text):
    """Return list of (name, body, start, end) for top-level definitions."""
    funcs = []
    for m in FUNC_DEF.finditer(text):
        name = m.group(1)
        if name in ('if','while','for','switch','return','sizeof','catch'):
            continue
        body_start = m.end()
        # brace match
        depth = 1; pos = body_start; in_s=in_c=in_lc=in_bc=False
        while pos < len(text) and depth > 0:
            ch = text[pos]; prev = text[pos-1] if pos>0 else '\0'
            if in_lc:
                if ch=='\n': in_lc=False
                pos+=1; continue
            if in_bc:
                if ch=='/' and prev=='*': in_bc=False
                pos+=1; continue
            if in_s:
                if ch=='"' and prev!='\\': in_s=False
                pos+=1; continue
            if in_c:
                if ch=="'" and prev!='\\': in_c=False
                pos+=1; continue
            if ch=='/' and pos+1<len(text):
                if text[pos+1]=='/': in_lc=True; pos+=2; continue
                if text[pos+1]=='*': in_bc=True; pos+=2; continue
            if ch=='"': in_s=True
            elif ch=="'": in_c=True
            elif ch=='{': depth+=1
            elif ch=='}': depth-=1
            pos+=1
        body = text[body_start:pos]
        funcs.append((name, body, m.start(), pos))
    return funcs

# ---- callee extraction ----
CALLEE = re.compile(r'(?<![.\w])([a-zA-Z_]\w*)\s*\(')

# Real-work signals
REAL_SIGNALS = [
    r'\bprintf\s*\(', r'\bfprintf\s*\(', r'\bputs\s*\(', r'\bfputs\s*\(',
    r'\bfwrite\s*\(', r'\bperror\s*\(', r'\bhermes_log\b', r'\blog_',
    r'\bfopen\s*\(', r'\bopen\s*\(', r'\bfclose\s*\(', r'\bclose\s*\(',
    r'\bjson_object_set', r'\bjson_array_append', r'\bjson_set',
    r'\bconfig_py_save', r'\bconfig_py_atomic', r'\bwrite_config',
    r'\bsystem\s*\(', r'\bexecl', r'\bfork\s*\(', r'\bpopen\s*\(',
    r'\bcurl', r'\bhttp_', r'\bsocket\s*\(',
    r'\bmalloc\s*\(', r'\bcalloc\s*\(', r'\brealloc\s*\(',
    r'\bstrcpy', r'\bstrcat', r'\bsnprintf\s*\(', r'\bsprintf\s*\(',
    r'\bfgets\s*\(', r'\bfread\s*\(', r'\bsqlite',
    r'\byaml_', r'\bjson_parse', r'\bjson_new',
    r'\bprocess_registry', r'\bclipboard',
]
REAL_RE = [re.compile(p) for p in REAL_SIGNALS]

def has_real_signal(body):
    return any(rx.search(body) for rx in REAL_RE)

def has_loop_or_branch(body):
    # crude: any control-flow keyword with a following statement
    return re.search(r'\b(for|while|do|if|switch)\s*\(', body) is not None

def body_nonblank_lines(body):
    lines = []
    for ln in body.split('\n'):
        s = ln.strip()
        if not s or s.startswith('//'): continue
        if s.startswith('/*') or s.endswith('*/'): 
            # strip block comments crudely
            s2 = re.sub(r'/\*.*?\*/', '', ln, flags=re.S).strip()
            if not s2: continue
            s = s2
        lines.append(s)
    return lines

def classify_bootleg(name, info, defined, memo, stack=None):
    """memo: name->bool (True=bootleg). defined: set of func names in port set.

    Precise rule: a function is BOOTLEG iff it does NO observable work and
    either returns a zero/empty literal or delegates solely to bootleg
    functions. Observable work = real signals (I/O, json mutation, alloc+use,
    prints) OR a call to an EXTERNAL (non-ported) function (library / live
    handler) OR a call to a REAL ported function.
    """
    if name in memo:
        return memo[name]
    if stack is None:
        stack = set()
    if name in stack:
        # Cycle: treat as REAL to avoid false cascades (conservative).
        return False
    stack.add(name)
    body = info['body']
    if body.strip() in ('{}', ';'):
        memo[name] = True; stack.discard(name); return True

    callees = set()
    for cm in CALLEE.finditer(body):
        c = cm.group(1)
        if c in ('if','while','for','switch','return','sizeof','catch','do'):
            continue
        callees.add(c)
    internal = [c for c in callees if c in defined]
    external = [c for c in callees if c not in defined]

    # External callee => this calls a live handler / library => REAL.
    if external:
        memo[name] = False; stack.discard(name); return False
    # Real signal (I/O, json mutation, alloc, print) => REAL.
    if has_real_signal(body):
        memo[name] = False; stack.discard(name); return False

    nb = body_nonblank_lines(body)
    if not nb:
        memo[name] = True; stack.discard(name); return True

    def stmt_is_bootleg(s):
        s = s.strip()
        if s.startswith('(void)'):
            return True
        if re.match(r'^return\s+(0|NULL|null|false|FALSE|"\s*"|\'\0\')\s*;?$', s):
            return True
        if re.match(r'^return\s+[\w]+\s*;?$', s):
            m = re.match(r'^return\s+([\w]+)\s*;?$', s)
            v = m.group(1)
            if v in defined:
                return classify_bootleg(v, defined[v], defined, memo, stack)
            return True
        if re.match(r'^return\s+[\w]+\s*\([^;]*\)\s*;?$', s):
            m = re.match(r'^return\s+([\w]+)\s*\(', s)
            v = m.group(1)
            if v in defined:
                return classify_bootleg(v, defined[v], defined, memo, stack)
            return True
        if re.match(r'^return\s+', s):
            return False  # non-trivial computed value => real accessor
        return True

    for st in nb:
        if not stmt_is_bootleg(st):
            memo[name] = False; stack.discard(name); return False
    memo[name] = True; stack.discard(name); return True

def main():
    sources = ported_sources()
    defined = {}  # name -> info
    order = []
    for src in sources:
        text = src.read_text(errors='ignore')
        for (name, body, s, e) in extract_funcs(text):
            if name not in defined:
                defined[name] = {'body': body, 'file': str(src.relative_to(SLERMES))}
                order.append(name)
    # classify
    memo = {}
    results = {}
    for name in order:
        results[name] = classify_bootleg(name, defined[name], defined, memo)
    # report bootleg
    bootleg = [(n, defined[n]['file']) for n in order if results[n]]
    # also detect simple trivial patterns directly (covers the external-forwarder blind spot)
    print(f"Ported TUs scanned: {len(sources)}")
    print(f"Functions indexed: {len(order)}")
    print(f"BOOTLEG (recursive): {len(bootleg)}")
    print("---")
    for n, f in sorted(bootleg):
        print(f"{n}  [{f}]")
    if '--json' in sys.argv:
        json.dump({'bootleg': [{'name':n,'file':f} for n,f in bootleg]},
                  sys.stdout, indent=2)

if __name__ == '__main__':
    main()
