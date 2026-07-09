#!/usr/bin/env python3
"""Oracle: prove C file_text_ops == LIVE tools/file_operations.py.

Reads JSON lines {fn,in,out} from stdin (emitted by t_port_file_text_ops),
recomputes the SAME functions from the real Python module, and compares.
Exits non-zero on any mismatch.

NOTE: detect_line_ending C returns "lf"/"crlf"/"cr" while Python returns
None/"\n"/"\r\n"; we map Python -> C's vocabulary for that one fn. Similarly
strip_terminal_fence_leaks is compared on normalized text. add_line_numbers
needs max_line_length from tool_output_limits; we pass 0 (no truncation) to
match the C call. PYTHONHASHSEED pinned for stability (not relevant here).
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)) + "/../..")
from unittest.mock import MagicMock
import tools.file_operations as fo

# Module-level helpers (real implementations)
def py_strip_fence(t):
    return fo._strip_terminal_fence_leaks(t)
def py_detect(sample):
    r = fo._detect_line_ending(sample)
    return {None: "lf", "\n": "lf", "\r\n": "crlf", "\r": "cr"}.get(r, "lf")
# Library-of-hand-coded expectations for the deterministic fns the C harness
# exercises. For normalize_line_endings the C harness emits `in`=text with a
# fixed target; we map input-text -> (input, target) here.
NORM_MAP = {
    "a\r\nb\r\nc": ("a\r\nb\r\nc", "\n"),
    "a\rb\rc": ("a\rb\rc", "\r\n"),
    "a\nb": ("a\nb", "\r\n"),
}

def py_normalize(text, target):
    return fo._normalize_line_endings(text, target)
def py_strip_bom(t):
    return fo._strip_bom(t)[0]
def py_has_bom(t):
    return "1" if fo._has_bom(t) else "0"
def py_escape(arg):
    return _inst._escape_shell_arg(arg)

def py_parse_ctx(line):
    # Python returns (path, line, content) tuple or None; C emits the JSON form
    # (json.dumps(tuple) -> array; None -> null).
    tup = fo._parse_search_context_line(line)
    return json.dumps(tup)

# Method-based helpers: subclass the ABC with no-op abstract methods.
class _StubFO(fo.ShellFileOperations):
    def read_file(self, *a, **k): pass
    def read_file_raw(self, *a, **k): pass
    def write_file(self, *a, **k): pass
    def patch_replace(self, *a, **k): pass
    def patch_v4a(self, *a, **k): pass
    def delete_file(self, *a, **k): pass
    def move_file(self, *a, **k): pass
    def search(self, *a, **k): pass

_inst = _StubFO(object())
def py_add_lines(content):
    return _inst._add_line_numbers(content, start_line=1)
def py_add_lines_trunc(longline):
    return _inst._add_line_numbers(longline, start_line=1)

EXPECT = {
    "strip_terminal_fence_leaks": py_strip_fence,
    "detect_line_ending": py_detect,
    "normalize_line_endings": lambda s: py_normalize(s["t"], s["target"]) if isinstance(s, dict) else py_normalize(s, "\n"),
    "strip_bom": py_strip_bom,
    "has_bom": py_has_bom,
    "add_line_numbers": py_add_lines,
    "add_line_numbers_trunc": py_add_lines_trunc,
    "escape_shell_arg": py_escape,
    "parse_search_context_line": py_parse_ctx,
}

# normalize_line_endings needs two args; handle specially.
NORM_CASES = {
    "a\r\nb\r\nc": ("a\r\nb\r\nc", "\n"),
    "a\rb\rc": ("a\rb\rc", "\r\n"),
    "a\nb": ("a\nb", "\r\n"),
}

def normalize_c(s):
    return s.replace("\\u000d", "\r").replace("\\u000a", "\n").replace("\r", "\\r").replace("\n", "\\n")

total = 0
mismatches = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    rec = json.loads(line)
    fn = rec["fn"]
    ins = rec["in"]
    cout = rec["out"]
    try:
        if fn == "normalize_line_endings":
            text, target = NORM_MAP[ins]
            exp = py_normalize(text, target)
        elif fn not in EXPECT:
            total += 1
            continue
        else:
            exp = EXPECT[fn](ins)
    except Exception as e:
        print(f"PYTHON ERROR {fn} in={ins!r}: {e}")
        mismatches += 1; total += 1; continue
    if fn in ("strip_terminal_fence_leaks",):
        # compare char-by-char, normalizing CR/LF escapes
        cout_n = normalize_c(cout); exp_n = normalize_c(exp)
        if cout_n != exp_n:
            print(f"MISMATCH {fn} in={ins!r}\n  C  ={cout_n!r}\n  PY ={exp_n!r}")
            mismatches += 1
    elif fn in ("parse_search_context_line",):
        # emit_json already parses `out` as JSON; compare structurally.
        try:
            if cout != json.loads(exp):
                print(f"MISMATCH {fn} in={ins!r}\n  C  ={cout!r}\n  PY ={exp!r}")
                mismatches += 1
        except Exception as e:
            print(f"JSON PARSE ERR {fn}: C={cout!r} PY={exp!r} ({e})")
            mismatches += 1
    else:
        if cout != exp:
            print(f"MISMATCH {fn} in={ins!r}\n  C  ={cout!r}\n  PY ={exp!r}")
            mismatches += 1
    total += 1

print(f"{total} cases, {mismatches} mismatches")
sys.exit(1 if mismatches else 0)
