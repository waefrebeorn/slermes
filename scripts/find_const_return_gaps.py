#!/usr/bin/env python3
"""Find constant-return feature gaps in slermes /ports/*.c.

A *void-no-op* ({(void)0;}) is the shallowest stub. The next, deeper gap is a
function whose body is essentially `return <constant>;` where the Python
source function of the SAME name performs REAL work (calls, dict/string/list
ops, control flow with side effects, etc.).

These are NOT honest architectural seams: Python demonstrably computes
something, and the C port silently collapses it to a hardcoded value. That is
a feature-parity gap, not a "no C subsystem" seam.

Detection:
  1. Parse every function body in built src/ports/*.c (mirrors find_void_noops).
  2. Flag bodies that are ONLY: optional (void)x; suppressions + exactly one
     `return <LIT>;` where LIT is a literal/constant with no function call or
     real computation.
  3. For each flagged C fn, locate the Python `def <name>(` inside
     agent/ tools/ hermes_cli/ gateway/ and measure "real work" markers in the
     Python body (calls, attr chains, control flow, dict/list/str ops,
     regex/json/file ops). If Python's body has real work -> CANDIDATE GAP.

Usage:
    python3 scripts/find_const_return_gaps.py [SLERMES_ROOT] [--min-work N]

Prints candidate gaps sorted by descending python-work-score. This is a triage
list for human judgment, NOT an auto-fix (some constant returns are legit).
"""
import ast
import os
import re
import sys

KEYWORDS = {'if','for','while','switch','return','do','catch','typeof','sizeof'}

def find_function_bodies(src):
    res = []
    n = len(src)
    paren_def = re.compile(r'([A-Za-z_][A-Za-z0-9_]*)\s*\(([^;{]*?)\)\s*\{')
    for m in paren_def.finditer(src):
        name = m.group(1)
        if name in KEYWORDS:
            continue
        start = m.end() - 1
        depth = 0
        j = start
        while j < n:
            c = src[j]
            if c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
                if depth == 0:
                    res.append((name, src[start+1:j], src[:start].count('\n') + 1))
                    break
            j += 1
    return res

def strip_c_comments(body):
    s = re.sub(r'/\*.*?\*/', '', body, flags=re.S)
    s = re.sub(r'//[^\n]*', '', s)
    return s

CONST_RE = re.compile(
    r'^\s*'
    r'((?:\(void\)[^;]*;\s*)*)'          # optional (void)x; suppressions
    r'return\s+'
    r'('
    r'NULL|nullptr|TRUE|FALSE|true|false'   # known constants
    r'|\-?\d+\.?\d*[fFfLlUu]?'              # numeric literal
    r'|0x[0-9a-fA-F]+'                      # hex
    r'|""'                                  # empty string
    r'|"[^"]*"'                             # string literal
    r"|''"                                  # empty char/str
    r"|'[^']*'"                             # char literal
    r'|[A-Z_][A-Z0-9_]*'                    # CONSTANT_LIKE macro/enum identifier
    r'|strdup\s*\(\s*(""|"[^"]*")\s*\)'     # strdup of constant
    r')\s*;?\s*$'
)

def is_constant_return(body):
    s = strip_c_comments(body).strip()
    if not s:
        return False
    m = CONST_RE.match(s)
    if not m:
        return False
    # require the constant not to hide a function call / real computation
    const_expr = m.group(2)
    if re.search(r'\(', const_expr) and not const_expr.startswith('strdup'):
        return False
    # ensure nothing else (no assignment, no ->/. work beyond the single return)
    # strip the matched suppression + return; remainder must be empty
    remainder = s[m.end():].strip()
    return remainder == ''

# ---- Python side: measure "real work" in a function def ----
WORK_MARKERS = [
    re.compile(r'\b\w+\s*\('),                 # function call
    re.compile(r'\.\w+\s*\('),                 # method call
    re.compile(r'\['),                          # subscript / indexing
    re.compile(r'\.\w+'),                       # attribute access
    re.compile(r'for\s+'), re.compile(r'while\s+'),
    re.compile(r'if\s+'), re.compile(r'with\s+'),
    re.compile(r'return\s+(?!None|True|False|-?\d|""|\'\'|0|1|\[\]| \{\}|NULL|nullptr|true|false)'),
    re.compile(r'\+=|-=|\*=|/=|='),            # assignment
    re.compile(r're\.'), re.compile(r'json\.'),
    re.compile(r'open\s*\('), re.compile(r'with\s+open'),
    re.compile(r'\.get\(|\.update\(|\.append\(|\.items\(|keys\(|\.values\('),
    re.compile(r'await\s+'), re.compile(r'yield\s+'),
]

def python_real_work(py_src, fn_name):
    """Return (score, snippet) of real work for def fn_name in py_src, or (0,None)."""
    try:
        tree = ast.parse(py_src)
    except Exception:
        return 0, None
    best = (0, None)
    for node in ast.walk(tree):
        if isinstance(node, ast.FunctionDef) and node.name == fn_name:
            seg = py_src[node.lineno-1:node.end_lineno]
            score = 0
            for rx in WORK_MARKERS:
                score += len(rx.findall(seg))
            # subtract trivial-return markers (return None/False/etc are not work)
            if score > best[0]:
                best = (score, seg.strip().splitlines()[0] if seg.strip() else '')
    return best

def main():
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser('~/hermes-agent-dev/slermes')
    min_work = 3
    for a in sys.argv[2:]:
        if a.startswith('--min-work='):
            min_work = int(a.split('=')[1])
    mk_path = os.path.join(root, 'build/objects.mk')
    mk = open(mk_path).read() if os.path.exists(mk_path) else ''
    built = []
    for dirpath, _, files in os.walk(os.path.join(root, 'src')):
        for f in files:
            if f.endswith('.c'):
                rel = os.path.join(dirpath, f)[len(root)+1:]
                if (rel[:-2] + '.o') in mk:
                    built.append(os.path.join(dirpath, f))
    only_ports = [p for p in built if '/ports/' in p]

    # index python defs
    py_dirs = [os.path.join(root, d) for d in ('agent','tools','hermes_cli','gateway')]
    py_funcs = {}  # name -> list of (pyfile, src)
    for d in py_dirs:
        for dp, _, fs in os.walk(d):
            for fn in fs:
                if fn.endswith('.py'):
                    try:
                        src = open(os.path.join(dp, fn)).read()
                    except Exception:
                        continue
                    try:
                        tree = ast.parse(src)
                    except Exception:
                        continue
                    for node in ast.walk(tree):
                        if isinstance(node, ast.FunctionDef):
                            py_funcs.setdefault(node.name, []).append((os.path.relpath(os.path.join(dp,fn), root), src))

    candidates = []
    for p in only_ports:
        try:
            src = open(p).read()
        except Exception:
            continue
        rel = p.split('slermes/')[1] if 'slermes/' in p else p
        for name, body, ln in find_function_bodies(src):
            if is_constant_return(body):
                # find python counterpart
                matches = py_funcs.get(name)
                if not matches:
                    continue
                max_score = 0
                pyf = None
                for pyfile, psrc in matches:
                    sc, _ = python_real_work(psrc, name)
                    if sc > max_score:
                        max_score = sc
                        pyf = pyfile
                if max_score >= min_work:
                    candidates.append((max_score, rel, ln, name, pyf))

    candidates.sort(reverse=True)
    print(f"# constant-return C functions whose Python twin does real work (min_work={min_work})")
    print(f"# total candidates: {len(candidates)}\n")
    for score, rel, ln, name, pyf in candidates:
        print(f"{score:4d}  {rel}:{ln}  {name}()   <- py:{pyf}")
    return 0

if __name__ == '__main__':
    sys.exit(main())
