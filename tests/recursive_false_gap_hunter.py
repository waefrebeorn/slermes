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

# ---- function definition extraction (brace-aware, shape-agnostic) ----
# The old FUNC_DEF regex could not match `char **name(`, multi-line
# signatures, attributes, etc. — 2,757 exported functions were invisible
# to the census (proven via `nm` on the built objects). This extractor is
# a single-pass top-level scanner: it finds `name(` at brace depth 0 where
# name is a C identifier (not a keyword, not a function-pointer/`->`/`.`/
# `]` member, not a macro), then brace-matches the body with full
# string/comment awareness. Shape-agnostic by construction.
FUNC_KEYWORDS = frozenset({
    'if','while','for','switch','return','sizeof','catch','do','else',
    'case','struct','union','enum','typedef','goto','static_assert',
    'void','int','char','bool','float','double','long','short','unsigned',
    'signed','const',
})
FUNC_PREV_BAD = set('(>.-]')   # function ptr / member / array contexts

def _skip_ws_comments(text, pos):
    n = len(text)
    while pos < n:
        if text[pos].isspace():
            pos += 1
        elif text[pos] == '/' and pos + 1 < n and text[pos + 1] == '/':
            nl = text.find('\n', pos)
            pos = n if nl < 0 else nl + 1
        elif text[pos] == '/' and pos + 1 < n and text[pos + 1] == '*':
            e = text.find('*/', pos + 2)
            pos = n if e < 0 else e + 2
        else:
            break
    return pos

def _match_group(text, pos, open_ch, close_ch):
    """text[pos] is open_ch; return index just past the matching close_ch.
    String/comment aware, handles nesting."""
    n = len(text)
    depth = 0
    i = pos
    while i < n:
        ch = text[i]
        if ch == '"':
            i += 1
            while i < n:
                if text[i] == '\\':
                    i += 2; continue
                if text[i] == '"':
                    i += 1; break
                i += 1
            continue
        if ch == "'":
            i += 1
            while i < n:
                if text[i] == '\\':
                    i += 2; continue
                if text[i] == "'":
                    i += 1; break
                i += 1
            continue
        if ch == '/' and i + 1 < n:
            if text[i + 1] == '/':
                nl = text.find('\n', i)
                i = n if nl < 0 else nl + 1
                continue
            if text[i + 1] == '*':
                e = text.find('*/', i + 2)
                i = n if e < 0 else e + 2
                continue
        if ch == open_ch:
            depth += 1
        elif ch == close_ch:
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return n


def extract_funcs(text):
    """Return list of (name, body, start, end) for top-level definitions.

    body includes the closing brace (same convention as the old regex
    extractor). Robust against pointer-to-pointer returns, multi-line
    signatures, comments/strings, and __attribute__ between params and
    body. Ignores macros (#define), function-pointer typedefs, and calls.
    """
    funcs = []
    n = len(text)
    depth = 0
    i = 0
    while i < n:
        ch = text[i]
        if ch == '"':
            i += 1
            while i < n:
                if text[i] == '\\':
                    i += 2; continue
                if text[i] == '"':
                    i += 1; break
                i += 1
            continue
        if ch == "'":
            i += 1
            while i < n:
                if text[i] == '\\':
                    i += 2; continue
                if text[i] == "'":
                    i += 1; break
                i += 1
            continue
        if ch == '/' and i + 1 < n:
            if text[i + 1] == '/':
                nl = text.find('\n', i)
                i = n if nl < 0 else nl + 1
                continue
            if text[i + 1] == '*':
                e = text.find('*/', i + 2)
                i = n if e < 0 else e + 2
                continue
        if ch == '{':
            depth += 1; i += 1; continue
        if ch == '}':
            depth -= 1; i += 1; continue
        if depth != 0:
            i += 1
            continue
        # top level: candidate identifier
        if ch.isalpha() or ch == '_':
            j = i
            while j < n and (text[j].isalnum() or text[j] == '_'):
                j += 1
            name = text[i:j]
            k = _skip_ws_comments(text, j)
            if k < n and text[k] == '(':
                prev = text[i - 1] if i > 0 else '\0'
                line_start = text.rfind('\n', 0, i) + 1
                if (name not in FUNC_KEYWORDS
                        and prev not in FUNC_PREV_BAD
                        and text[line_start:line_start + 1] != '#'):
                    p = _match_group(text, k, '(', ')')
                    q = _skip_ws_comments(text, p)
                    # function-pointer-returning functions:
                    #   RET (*name(params))(extra) { ... }
                    # after the params group the (*name(...)) wrapper closes
                    # with ')', then come extra parameter groups.
                    while q < n and text[q] in '()':
                        if text[q] == ')':
                            q = _skip_ws_comments(text, q + 1)
                        else:
                            q = _skip_ws_comments(text, _match_group(text, q, '(', ')'))
                    # tolerate trailing __attribute__((...)) before the body
                    while q < n and (text.startswith('__attribute__', q)
                                     or text.startswith('__asm__', q)):
                        a = _skip_ws_comments(text, q + len('__attribute__'))
                        if a < n and text[a] == '(':
                            q = _skip_ws_comments(text, _match_group(text, a, '(', ')'))
                        else:
                            break
                    if q < n and text[q] == '{':
                        close = _match_group(text, q, '{', '}')
                        body = text[q + 1:close]
                        funcs.append((name, body, i, close + 1))
                i = j
                continue
        i += 1
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
    # Loop with a body => REAL: iterating over data is observable
    # computation (string scans, parsers, table walks). The bootleg stub
    # shape never contains a loop — only (void)arg guards and zero returns.
    # (The docstring has always promised this rule; has_loop_or_branch was
    # dead code before this call was added.)
    if re.search(r'\b(for|while|do)\s*\(', body):
        memo[name] = False; stack.discard(name); return False

    nb = body_nonblank_lines(body)
    if not nb:
        memo[name] = True; stack.discard(name); return True

    def stmt_is_bootleg(s):
        s = s.strip()
        if not s or s in ('{', '}'):
            return False  # neutral braces
        if s.startswith('(void)'):
            return True
        if re.match(r'^return\s+(0|NULL|null|false|FALSE|"\s*"|\'\\0\')\s*;?$', s):
            return True
        if re.match(r'^return\s+(true|True|-?\d+(\.\d+)?)\s*;?$', s):
            return False  # truthy constant (e.g. check_*_requirements -> 1)
        if re.match(r'^return\s+[\w]+\s*;?$', s):
            m = re.match(r'^return\s+([\w]+)\s*;?$', s)
            v = m.group(1)
            if v in defined:
                return classify_bootleg(v, defined[v], defined, memo, stack)
            # Module-static/global state accessor (g_*/s_*) => real read.
            if re.match(r'^(g_|s_)[A-Za-z0-9_]*$', v):
                return False
            return True
        if re.match(r'^return\s+[\w]+\s*\([^;]*\)\s*;?$', s):
            m = re.match(r'^return\s+([\w]+)\s*\(', s)
            v = m.group(1)
            if v in defined:
                return classify_bootleg(v, defined[v], defined, memo, stack)
            return True
        if re.match(r'^return\s+', s):
            return False  # non-trivial computed value => real accessor
        # State mutation => real: writes into module-static/global state
        # (g_*/s_* fields, pointer derefs, array stores) or alloc-and-store.
        if re.search(r'\b(g_|s_)[A-Za-z0-9_]*\s*(\[|\.|=)|->\s*\w+\s*=|\[\s*[^\]]*\]\s*=', s):
            return False
        if re.search(r'=\s*(malloc|calloc|strdup|realloc)\s*\(', s):
            return False
        # Control-flow headers defer to their body lines; bare calls invoke
        # real code (external, or the callee is judged on its own).
        if re.match(r'^(if|else|for|while|do|switch|case)\b', s):
            return False
        if re.match(r'^[\w][\w \*]*\([^;]*\)\s*;?$', s):
            return False
        return True

    for st in nb:
        if st in ('{', '}'):
            continue  # extractor includes the closing brace in the body
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
    if '--verify' in sys.argv:
        # Self-check the extractor against the built objects: every exported
        # function symbol must be indexed. Stale .o files (no matching .c)
        # are skipped so deleted sources cannot ghost symbols.
        import subprocess
        c_bases = {str(p.relative_to(SLERMES))[:-2]
                   for p in (SLERMES / "src").rglob("*.c")}
        objs = [str(p) for p in (SLERMES / "src").rglob("port_*.o")
                if str(p.relative_to(SLERMES))[:-2] in c_bases]
        nm = subprocess.run(["nm", "--defined-only", "--format=posix"] + objs,
                            capture_output=True, text=True, cwd=str(SLERMES))
        syms = {ln.split()[0] for ln in nm.stdout.splitlines()
                if len(ln.split()) >= 2 and ln.split()[1] == 'T'}
        missed = sorted(syms - set(defined))
        print(f"VERIFY: objects={len(objs)} nm_exported={len(syms)} "
              f"indexed={len(defined)} missed={len(missed)}")
        for m in missed[:20]:
            print(f"  MISSED {m}")
        if missed:
            print("VERIFY FAIL: extractor does not cover every exported symbol")
            sys.exit(1)
        print("VERIFY OK: extractor covers 100% of exported symbols")
    # classify
    memo = {}
    results = {}
    for name in order:
        results[name] = classify_bootleg(name, defined[name], defined, memo)
    # report bootleg
    bootleg = [(n, defined[n]['file']) for n in order if results[n]]
    print(f"Ported TUs scanned: {len(sources)}")
    print(f"Functions indexed: {len(order)}")
    print(f"REAL: {len(order) - len(bootleg)}  BOOTLEG: {len(bootleg)}")
    print(f"BOOTLEG (recursive): {len(bootleg)}")
    print("---")
    for n, f in sorted(bootleg):
        print(f"{n}  [{f}]")
    if '--json' in sys.argv:
        json.dump({'bootleg': [{'name':n,'file':f} for n,f in bootleg]},
                  sys.stdout, indent=2)

if __name__ == '__main__':
    main()
