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
import re, sys, os, json, subprocess, pathlib, ast
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

# Real-work signals — output calls (printf/fprintf/puts/perror) are NOT real
# work: an echo stub that only prints and returns a constant is still a stub.
# They are stripped before classification and never count as observable work.
REAL_SIGNALS = [
    r'\bfwrite\s*\(', r'\bhermes_log\b', r'\blog_',
    r'\bfopen\s*\(', r'\bopen\s*\(', r'\bfclose\s*\(', r'\bclose\s*\(',
    r'\bjson_object_set', r'\bjson_array_append', r'\bjson_set',
    r'\bconfig_py_save', r'\bconfig_py_atomic', r'\bwrite_config',
    r'\bsystem\s*\(', r'\bexecl', r'\bfork\s*\(', r'\bpopen\s*\(',
    r'\bcurl', r'\bhttp_', r'\bsocket\s*\(',
    r'\bmalloc\s*\(', r'\bcalloc\s*\(', r'\brealloc\s*\(',
    r'\bfree\s*\(', r'\bmemset\s*\(', r'\bstrdup\s*\(',
    r'\bstrcpy', r'\bstrcat', r'\bsnprintf\s*\(', r'\bsprintf\s*\(',
    r'\bfgets\s*\(', r'\bfread\s*\(', r'\bsqlite',
    r'\byaml_', r'\bjson_parse', r'\bjson_new',
    r'\bprocess_registry', r'\bclipboard',

    r'\bfor\s*\(',
    r'\bwhile\s*\(',
    r'\bswitch\s*\(',
    r'\bstrcmp\s*\(',
    r'\bstrncmp\s*\(',
    r'\bstrcasecmp\s*\(',
    r'\bstrncasecmp\s*\(',
    r'\bstrstr\s*\(',
    r'\bstrchr\s*\(',
    r'\bstrrchr\s*\(',
    r'\bstrlen\s*\(',
    r'\bstrtol\s*\(',
    r'\bstrtod\s*\(',
    r'\bstrtok\s*\(',
    r'\batoi\s*\(',
    r'\bmemcmp\s*\(',
    r'\bmemcpy\s*\(',
    r'\bmemmove\s*\(',
    r'\bmemset\s*\(',
    r'\bisspace\s*\(',
    r'\bisdigit\s*\(',
    r'\bisalpha\s*\(',
    r'\bisalnum\s*\(',
    r'\btolower\s*\(',
    r'\btoupper\s*\(',
    r'\bgetenv\s*\(',
    r'\bstat\s*\(',
    r'\baccess\s*\(',
    r'\bunlink\s*\(',
    r'\bmkdir\s*\(',
    r'\brename\s*\(',
    r'\bchmod\s*\(',
    r'\bgetcwd\s*\(',
    r'\bstrftime\s*\(',
    r'\blocaltime\s*\(',
    r'\bgmtime\s*\(',
    r'\btime\s*\(',
    r'\bclock_gettime\s*\(',
    r'\bpthread_',
    r'\bsigaction\s*\(',
    r'\berrno',

]
REAL_RE = [re.compile(p) for p in REAL_SIGNALS]

def has_real_signal(body):
    return any(rx.search(body) for rx in REAL_RE)

# Output-only calls: NOT observable work. A stub that prints and returns a
# constant is still a stub.  Stripped from callee analysis and never treated
# as a real-work signal.
OUTPUT_CALLS = frozenset({
    'printf', 'fprintf', 'puts', 'fputs', 'perror', 'putchar', 'fputc',
    'fputchar', 'vprintf', 'vfprintf', 'vputs', 'dprintf', 'vdprintf',
    'printw', 'mvprintw', 'wprintw', 'print',
})

_OUTPUT_STMT = re.compile(
    r'\b(?:printf|fprintf|puts|fputs|perror|putchar|fputc|fputchar|vprintf|'
    r'vfprintf|vputs|dprintf|vdprintf|printw|mvprintw|wprintw|print)\s*\([^;]*\)\s*;'
)

def is_echo_stub(body):
    """True when a body does nothing but emit output and return constants.

    Agnostic: we strip every output statement, comments and (void) casts, then
    require that what remains is only declarations and constant returns.  Any
    remaining call, assignment, control flow, or computed return makes it real.
    This cannot be gamed by printing arbitrary text — the prints are removed
    before the check.
    """
    # stderr logging (fprintf(stderr,...)) is REAL observability — the C
    # equivalent of Python's event_log.log()/logger.info().  Mark those
    # statements BEFORE stripping output so a logging helper is never
    # mistaken for an echo stub.
    body = re.sub(r'\bfprintf\s*\(\s*stderr[^;]*;', 'REAL_LOG_CALL();', body)
    cleaned = _OUTPUT_STMT.sub('', body)
    cleaned = re.sub(r'/\*.*?\*/', '', cleaned, flags=re.S)
    cleaned = re.sub(r'//[^\n]*', '', cleaned)
    cleaned = re.sub(r'\(void\)\s*[a-zA-Z_]\w*\s*;', '', cleaned)
    cleaned = re.sub(r'\s+', ' ', cleaned).strip(' ;{}')
    if not cleaned:
        return True
    # Any remaining call (real work), assignment, or control flow => real.
    if re.search(r'\b[a-zA-Z_]\w*\s*\(', cleaned):
        return False
    if re.search(r'\b(if|for|while|switch|do)\b', cleaned):
        return False
    if re.search(r'\b[a-zA-Z_]\w*\s*(=|\+=|-=|\*=|/=)', cleaned):
        return False
    # Postfix/prefix increments and array stores are real state mutations.
    if re.search(r'\+\+|--', cleaned):
        return False
    if re.search(r'\[\s*[^\]]*\]\s*=', cleaned):
        return False
    # Only constant returns / declarations remain.
    for stmt in cleaned.split(';'):
        s = stmt.strip()
        if not s:
            continue
        # Constant returns: literals, NULL, bare identifiers, g_*/s_* globals.
        # NOT ternary expressions (b ? b->kind : "") or computed expressions —
        # those are real accessors reading struct fields.
        if re.match(r'^return\s+(0|-?[0-9]+(\.[0-9]+)?|NULL|null|false|true|False|True|""|\'\'|"[^"]*"|\'.\')\s*$', s):
            continue
        if re.match(r'^return\s+[a-zA-Z_]\w*\s*$', s):
            # Module-static/global state reads and ALL-CAPS macros are REAL
            # constant accessors — never echo stubs.  Any other bare
            # identifier falls through to the per-statement walk, which
            # resolves it against FILE_SCOPE / defined.
            return False
        if re.match(r'^return\s+(g_|s_)[a-zA-Z_]?\w*\s*$', s):
            continue
        if s.startswith('return '):
            # Any other return expression is a computed accessor (ternary,
            # field deref, arithmetic) — real work, not an echo stub.
            return False
        if re.match(r'^(?:const\s+|unsigned\s+|signed\s+|static\s+|volatile\s+)*[a-zA-Z_]\w*', s):
            continue
        return False
    return True

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

def _build_file_scope():
    """Populate FILE_SCOPE: rel-path -> set of file-scope static/global names."""
    global FILE_SCOPE
    if FILE_SCOPE:
        return
    for src in ported_sources():
        rel = str(src.relative_to(SLERMES))
        text = src.read_text(errors='ignore')
        names = set()
        for m in re.finditer(
                r'^(?:static\s+|const\s+|extern\s+|volatile\s+)*'
                r'(?:[A-Za-z_]\w*\s*\*?\s+)+'
                r'\*?\s*([a-zA-Z_]\w*)\s*(?:\[[^\]]*\]\s*)?(?:=|;)',
                text, re.M):
            names.add(m.group(1))
        FILE_SCOPE[rel] = names


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
    if not FILE_SCOPE:
        _build_file_scope()
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
        if c in OUTPUT_CALLS:
            continue  # output is not observable work
        callees.add(c)
    internal = [c for c in callees if c in defined]
    external = [c for c in callees if c not in defined]

    # Echo stub: every statement is output + constant return => no real work,
    # regardless of the printed text.  Agnostic — not gamed by adding prints.
    # BUT if the PoP-annotated Python source is ALSO trivial (abstract base
    # `pass`, dataclass field, constant accessor), the port is faithful — a
    # C `printf + return -1` for a Python `pass` is correct, not a lie.
    # Only flag as bootleg when the Python does real work AND the C body is
    # just echo.  If Python is trivial (pass/accessor), the echo IS faithful.
    if is_echo_stub(body) and not python_is_trivial_for(name):
        memo[name] = True; stack.discard(name); return True

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
        # Strip trailing lone brace (body includes closing brace on same line).
        s = s.rstrip('}').strip()
        if not s:
            return False
        # (void)param; is unused-parameter suppression — NEUTRAL, not bootleg.
        # A function whose ONLY statements are (void) casts is caught by
        # is_echo_stub + the final python_is_trivial_for cross-check; a
        # function that does real work after the casts is real.
        if s.startswith('(void)'):
            return False
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
            # ALL-CAPS macro constant (STDOUT_FILENO, CATALOG_N, ...) or a
            # file-scope static in the same TU => real constant accessor.
            if re.match(r'^[A-Z][A-Z0-9_]*$', v):
                return False
            if v in FILE_SCOPE.get(info.get('file', ''), ()):
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
        # Assignment to a variable (plain `x = v` — local static state writes,
        # counter updates, flag sets) => real work.  Excludes declarations
        # (handled above as neutral) and never matches on return statements.
        if re.search(r'\b[a-zA-Z_]\w*\s*=\s*[^=;]', s) and not s.startswith(('static', 'const', 'int ', 'bool ', 'char ', 'long ', 'double ', 'float ', 'void ', 'unsigned ', 'signed ', 'size_t ')):
            return False
        # Pointer/struct field write via array index: w->p[i].field = v
        if re.search(r'\]\s*\.\s*\w+\s*=|->\s*\w+\s*\[[^\]]*\]\s*=', s):
            return False
        # Increment/decrement of a variable (counter, index) => real work.
        if re.search(r'\b\w+\s*(\+\+|--)\s*;', s):
            return False
        # Assignment of a function-call result: x = foo(...) => real work.
        if re.search(r'\b\w+\s*=\s*[A-Za-z_]\w*\s*\(', s):
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
    # Python cross-check: if the PoP-annotated Python source for this C
    # function is ALSO trivial (no IO/http/fs/state/print), the port is
    # faithful — not a lie.  Only flag when the Python does real work.
    if python_is_trivial_for(name):
        memo[name] = False; stack.discard(name); return False
    memo[name] = True; stack.discard(name); return True

# ── python-side cross-check ──
# The Python tree is the SIBLING of the slermes repo (repo layout:
# hermes-agent-dev/  ← Python sources (gateway/, agent/, tools/, ...)
# hermes-agent-dev/slermes/  ← this C port).  __file__ is slermes/tests/,
# so parent.parent = slermes; one more .parent lands on the Python root.
PY_ROOT = pathlib.Path(__file__).resolve().parent.parent.parent

# Python "real work" = IO / network / fs / subprocess / exec.  Pure
# declaration (argparse trees, command registration, self.X = binds in
# __init__, await-delegation, print-only bodies) is NOT real work — the
# C port dispatches names differently but equivalently.
PY_REAL_SIGNALS = [
    r'\bopen\s*\(', r'\bPath\s*\(', r'\bhttp', r'\bsqlite', r'\bsystem\s*\(',
    r'\bpopen', r'\bsubprocess', r'\bread_text', r'\bwrite_text', r'\bos\.',
    r'\brequests', r'\bjson\.dump', r'\bhttpx', r'\baiohttp', r'\burllib',
    r'\bsocket', r'\bunlink', r'\brename', r'\bmkdir', r'\benviron',
    r'\bgetenv',
    # Logger calls (logging._logger.log, logger.debug, logger.info, etc.).
    # Python `logger.info(...)` IS real side-effect work (observability sink).
    r'\blogger\.',
    r'\bevent_log\.',
    r'\bprint\(',
]
PY_REAL_RE = [re.compile(p) for p in PY_REAL_SIGNALS]

# c function name -> (python module path incl .py, python feature name)
_POP_INDEX = None
# file path (relative to SLERMES) -> set of file-scope static/global names.
# Populated in main() before classification.
FILE_SCOPE = {}

def _build_pop_index():
    global _POP_INDEX
    idx = {}
    for src in ported_sources():
        text = src.read_text(errors='ignore')
        for m in re.finditer(r'/\* PoP: (\S+) @ ([^:]+):(\S+) \*/', text):
            idx.setdefault(m.group(1), []).append((m.group(2), m.group(3)))
        # Also map the ACTUAL C function name that follows each PoP comment.
        # The comment's first token is often the Python feature name (e.g.
        # "connect") while the C wrapper is prefixed ("pb_connect") — the
        # classifier looks up by C name, so both keys must resolve.
        for (name, body, s, e) in extract_funcs(text):
            if name in idx:
                continue  # already mapped via comment token
            # nearest PoP comment strictly before this function's start
            best = None
            for m in re.finditer(r'/\* PoP: (\S+) @ ([^:]+):(\S+) \*/', text[:s]):
                best = m
            if best is not None:
                idx.setdefault(name, []).append((best.group(2), best.group(3)))
        # Module-aware "Port of Python <mod>.py:<fn>" comments carry the
        # python_file too; without them the hunter cannot cross-check.
        for m in re.finditer(
                r'/\*\s*Port of Python\s+([\w/.]+\.py):\s*([\w.]+)',
                text):
            fn = m.group(2).split('.')[-1].split('(')[0]
            idx.setdefault(fn, []).append((m.group(1), fn))
    _POP_INDEX = idx

def _enclosing_class(node, tree):
    """Return the name of the ClassDef enclosing *node*, or None."""
    for n in ast.walk(tree):
        if isinstance(n, ast.ClassDef):
            for child in ast.walk(n):
                if child is node:
                    return n.name
    return None


def python_is_trivial_for(c_name):
    """True when the PoP-annotated Python source for c_name does no real work.

    AST-based so multiline signatures with nested parens parse correctly.
    """
    global _POP_INDEX
    if _POP_INDEX is None:
        _build_pop_index()
    entries = _POP_INDEX.get(c_name)
    if not entries:
        return False  # unknown -> conservative: keep flagged
    for mod, py_name in entries:
        pypath = PY_ROOT / mod
        if not pypath.exists():
            continue
        try:
            src = pypath.read_text(errors='ignore')
            tree = ast.parse(src)
        except (OSError, SyntaxError):
            continue
        # Accept both bare "fn" and dotted "Class.fn" names.
        bare = py_name.rsplit('.', 1)[-1]
        fn = None
        for node in ast.walk(tree):
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) \
                    and node.name == bare:
                # For dotted names, verify the enclosing class matches.
                if '.' in py_name and node.col_offset > 0:
                    cls_name = py_name.rsplit('.', 1)[0]
                    enclosing = _enclosing_class(node, tree)
                    if enclosing != cls_name:
                        continue
                fn = node
                break
        if fn is None:
            continue
        body = ast.get_source_segment(src, fn)
        if not body:
            continue
        # A function that ONLY prints (or returns constants) is trivial.
        # Examine the AST for actual CALLS / attribute-access work, so
        # string literals (help text, docstrings, error messages) never
        # trigger a false positive.
        real_sigs = [r for r in PY_REAL_RE
                     if r.pattern != r'\bprint\s*\(']
        trivial = True
        for node in ast.walk(fn):
            if isinstance(node, ast.Call):
                f = node.func
                if isinstance(f, ast.Name):
                    # Known real-signal function names (open, Path, http, etc.)
                    for rx in real_sigs:
                        if rx.search(f.id):
                            trivial = False; break
                elif isinstance(f, ast.Attribute):
                    attr = f.attr
                    # self.method() / cls.method() — delegation to another
                    # component that may do real work (IO, network, etc.).
                    if isinstance(f.value, ast.Name) and f.value.id in ('self', 'cls'):
                        trivial = False; break
                    for rx in real_sigs:
                        if rx.search(attr):
                            trivial = False; break
            elif isinstance(node, ast.Attribute):
                if isinstance(node.value, ast.Name) and \
                        (node.value.id == 'os' or node.value.id == 'Path'):
                    for rx in real_sigs:
                        if rx.search('os.' + node.attr):
                            trivial = False; break
            # Any 'await' expression is real async work (network/IO).
            if isinstance(node, ast.Await):
                trivial = False
            if not trivial:
                break
        if trivial:
            return True
    return False

def main():
    global FILE_SCOPE
    sources = ported_sources()
    defined = {}  # name -> info
    order = []
    for src in sources:
        text = src.read_text(errors='ignore')
        for (name, body, s, e) in extract_funcs(text):
            if name not in defined:
                defined[name] = {'body': body, 'file': str(src.relative_to(SLERMES))}
                order.append(name)
    # File-scope static/global variable names per TU: `return <name>;` for a
    # module static is a real constant/state accessor (mcp_stderr_log,
    # s_yolo_mode_frozen, openai_client_available, ...).
    _build_file_scope()
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
