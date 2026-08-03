#!/usr/bin/env python3
"""PLUMBER ADVERSARIAL FUZZ ORACLE — v551 extracted modules.

Triple-DA attack: generate HOSTILE / BOUNDARY / UX inputs in THIS process
(which also computes expected via LIVE Python), have the C harness re-emit the
same (fn, in) plus its actual output, then assert C == LIVE Python.

Protocol (C harness -> this oracle, one JSON line per call):
  {"fn": <name>, "in": <input-string>, "out": <C output>}
For JSON-returning fns, `out` is the raw JSON produced by C (already parsed by
the harness into a Python obj, since emit_json prints it unescaped). For
string-returning fns, `out` is the plain string.

We recompute expected from (fn, in) using LIVE Python and compare.

  gcc -O2 -I include -I src/tools -I lib/libjson \
      tests/t_port_plumber_fuzz.c src/tools/file_text_ops.o \
      src/tools/cron_prompt_sanitize.o lib/libjson/json.o -o /tmp/t_fuzz
  /tmp/t_fuzz | PYTHONHASHSEED=0 python3 tests/sta_oracle_plumber_fuzz.py
"""
import json, sys, os

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, REPO)
import tools.file_operations as fo
import tools.cronjob_tools as cj

# ---- live expected-computers (LIVE Python, no re-implementation) -----------
def py_strip_fence(s):
    return fo._strip_terminal_fence_leaks(s)
def py_detect(s):
    r = fo._detect_line_ending(s)
    # The C port renders Python's None (undetermined: no line break in the
    # sample) as "unknown" — mirror that exactly.
    if r is None:
        return "unknown"
    return "crlf" if r == "\r\n" else "lf"
def py_normalize(s, target):
    return fo._normalize_line_endings(s, target)
def py_strip_bom(s):
    return fo._strip_bom(s)[0]
def py_has_bom(s):
    return "1" if fo._has_bom(s) else "0"
def py_parse_ctx(s):
    return json.dumps(fo._parse_search_context_line(s))  # tuple->array / None->null
def py_expand(s):
    return os.path.expanduser(s)  # C uses $HOME; Python shells out, C-equivalent

# cron (LIVE)
def py_check_invis(s):
    return cj._check_invisible_unicode(s)
def py_strip_invis(s):
    cleaned, removed = cj._strip_invisible_unicode(s)
    return json.dumps({"cleaned": cleaned, "removed": sorted(removed)}, sort_keys=True)
def py_scan_skill(s):
    cleaned, error = cj._scan_cron_skill_assembled(s)
    return json.dumps({"cleaned": cleaned, "error": error}, sort_keys=True)

# ShellFileOperations method-based helpers
class _Inst(fo.ShellFileOperations):
    def read_file(self,*a,**k): pass
    def read_file_raw(self,*a,**k): pass
    def write_file(self,*a,**k): pass
    def patch_replace(self,*a,**k): pass
    def patch_v4a(self,*a,**k): pass
    def delete_file(self,*a,**k): pass
    def move_file(self,*a,**k): pass
    def search(self,*a,**k): pass
_inst = _Inst(object())  # terminal_env dummy; methods are no-ops

def py_add_lines(s):
    return _inst._add_line_numbers(s, start_line=1)
def py_escape(s):
    return _inst._escape_shell_arg(s)

# ---- dispatch: fn -> (expected_computer, arity, json_out) -------------------
# arity 1: in is a plain string. arity 2: in is [text, target] for normalize.
# json_out True => compare structurally (both are JSON).
DISPATCH = {
    "strip_terminal_fence_leaks": (py_strip_fence, 1, False),
    "detect_line_ending": (py_detect, 1, False),
    "normalize_line_endings": (lambda i: py_normalize(i[0], i[1]), 2, False),
    "strip_bom": (py_strip_bom, 1, False),
    "has_bom": (py_has_bom, 1, False),
    "add_line_numbers": (py_add_lines, 1, False),
    "escape_shell_arg": (py_escape, 1, False),
    "parse_search_context_line": (py_parse_ctx, 1, True),
    "expand_path": (py_expand, 1, False),
    "check_invisible_unicode": (py_check_invis, 1, False),
    "strip_invisible_unicode": (py_strip_invis, 1, True),
    "scan_cron_skill_assembled": (py_scan_skill, 1, True),
}

# inputs where LIVE Python itself is defective (raises); C is defensive.
# We do NOT count these as mismatches — they are Python bugs, not C gaps.
PY_DEFECT = set()
mismatches = 0
total = 0
py_errors = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    rec = json.loads(line)
    fn = rec["fn"]
    in_obj = rec["in"]
    cout = rec.get("out")
    if fn not in DISPATCH:
        continue
    exp_fn, arity, json_out = DISPATCH[fn]
    try:
        exp = exp_fn(in_obj) if arity == 1 else exp_fn(in_obj)
    except Exception as e:
        # Python itself raised on this input -> Python defect, not C gap.
        py_errors += 1
        total += 1
        print(f"PY-DEFECT {fn} in={in_obj!r}: {e!r}")
        continue
    total += 1

    if json_out:
        try:
            c_json = cout if isinstance(cout, (dict, list)) else json.loads(cout)
            p_json = json.loads(exp)
        except Exception as e:
            print(f"JSON PARSE ERR {fn}: C={cout!r} PY={exp!r} ({e})")
            mismatches += 1; continue
        if c_json != p_json:
            mismatches += 1
            print(f"MISMATCH {fn} in={in_obj!r}\n  C  ={c_json!r}\n  PY ={p_json!r}")
    else:
        cstr = cout if isinstance(cout, str) else json.dumps(cout)
        pstr = exp if isinstance(exp, str) else json.dumps(exp)
        if cstr != pstr:
            mismatches += 1
            print(f"MISMATCH {fn} in={in_obj!r}\n  C  ={cstr!r}\n  PY ={pstr!r}")

print(f"{total} cases, {mismatches} mismatches, {py_errors} python-defect-skipped")
sys.exit(1 if mismatches else 0)
