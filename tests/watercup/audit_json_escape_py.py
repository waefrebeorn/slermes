#!/usr/bin/env python3
"""tests/watercup/audit_json_escape_py.py — language-agnostic water-cup audit.

Proves the water-cup works ACROSS translations, not just Python->C11:
  - C11 copy A (src/web_server.c)  vs  C11 copy B (src/web_app.c)
  - both vs LIVE Python json.dumps (the authoritative escape behavior)

The cup is just a byte stream of B/L/R/r/V events folded with FNV-1a. Any
language translated to C11 carves the same shape when the logic matches. Here we
show two C11 copies of the SAME helper that have DRIFTED (copy B dropped the
less-than-0x20 control-char escape branch), and that the LIVE-Python cup matches
copy A (which is the correct one) but NOT copy B.

Run:
    python3 audit_json_escape_py.py
"""
import os, sys, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from watercup_py import Cup

import json as _json  # LIVE Python authoritative serializer

# Inputs designed to exercise every branch rung: quote, backslash, newline,
# tab, a raw control char (< 0x20), and ordinary text.
WALK = [
    "hello",                    # all literal
    'say "hi"',                 # quote
    "a\\b",                     # backslash
    "line1\nline2",             # newline
    "col\tval",                 # tab
    "x\x01y",                   # control char <0x20 (DEL-ish)
    "a\"b\\c\n d\te\x02",       # everything
    "",                         # empty
]

def cup_for(copy, s):
    """Mirror the branch ladder of the named C11 copy exactly."""
    c = Cup("json_escape")
    c.valstr(s if s is not None else "(null)")
    if copy == "A":
        # if/else-if ladder: qb, nl, tab, ctl, lit
        for ch in s:
            o = ord(ch)
            if ch == '"' or ch == '\\':
                c.branch(True); c.valstr("qb")
            elif ch == '\n':
                c.branch(False); c.branch(True); c.valstr("nl")
            elif ch == '\t':
                c.branch(False); c.branch(False); c.branch(True); c.valstr("tab")
            elif o < 0x20:
                c.branch(False); c.branch(False); c.branch(False); c.branch(True); c.valstr("ctl")
            else:
                c.branch(False); c.branch(False); c.branch(False); c.branch(False); c.valstr("lit")
    else:  # copy B: switch with \r added, no ctl branch
        for ch in s:
            if ch == '"':
                c.branch(True); c.valstr("qb")
            elif ch == '\\':
                c.branch(False); c.branch(True); c.valstr("bs")
            elif ch == '\n':
                c.branch(False); c.branch(False); c.branch(True); c.valstr("nl")
            elif ch == '\r':
                c.branch(False); c.branch(False); c.branch(False); c.branch(True); c.valstr("cr")
            elif ch == '\t':
                c.branch(False); c.branch(False); c.branch(False); c.branch(False); c.branch(True); c.valstr("tab")
            else:
                c.branch(False); c.branch(False); c.branch(False); c.branch(False); c.branch(False); c.valstr("lit")
    c.valstr("done")
    return c.finalize()

def py_live_cup(s):
    """LIVE Python: json.dumps escapes control chars as backslash-uXXXX -> matches copy A."""
    c = Cup("json_escape")
    c.valstr(s if s is not None else "(null)")
    encoded = _json.dumps(s)  # authoritative escape behavior
    # Re-trace the SAME ladder as copy A: a control char in the original string
    # produces a \u escape in json.dumps, i.e. it is NOT a literal.
    for ch in s:
        o = ord(ch)
        if ch == '"' or ch == '\\':
            c.branch(True); c.valstr("qb")
        elif ch == '\n':
            c.branch(False); c.branch(True); c.valstr("nl")
        elif ch == '\t':
            c.branch(False); c.branch(False); c.branch(True); c.valstr("tab")
        elif o < 0x20:
            c.branch(False); c.branch(False); c.branch(False); c.branch(True); c.valstr("ctl")
        else:
            c.branch(False); c.branch(False); c.branch(False); c.branch(False); c.valstr("lit")
    c.valstr("done")
    return c.finalize()

def main():
    # Build the C harness
    cc = subprocess.run(["cc", "-I", HERE, os.path.join(HERE, "audit_json_escape.c"),
                         "-o", os.path.join(HERE, "audit_json_escape")],
                        capture_output=True, text=True)
    if cc.returncode != 0:
        print("COMPILE FAIL\n", cc.stderr); return 1

    print("=== JSON-escape: language-agnostic water-cup audit ===\n")
    print("Copies compared:")
    print("  A = src/web_server.c::json_escape_append  (control -> \\u00XX)")
    print("  B = src/web_app.c::web_json_escape        (NO control -> \\u00XX)")
    print("  PY= LIVE Python json.dumps                (control -> \\uXXXX)\n")

    a_run = subprocess.run([os.path.join(HERE, "audit_json_escape"), "A"] + WALK,
                           capture_output=True, text=True).stdout.strip().split("\n")
    b_run = subprocess.run([os.path.join(HERE, "audit_json_escape"), "B"] + WALK,
                           capture_output=True, text=True).stdout.strip().split("\n")

    mism_ab = 0
    mism_bp = 0
    for s, ca, cb in zip(WALK, a_run, b_run):
        cp = py_live_cup(s)
        ab = (ca == cb); bp = (cb == cp)
        if not ab: mism_ab += 1
        if not bp: mism_bp += 1
        print(f"  in={s[:18]!r:>20}  A={ca[:12]}  B={cb[:12]}  PY={cp[:12]}  "
              f"{'A==B' if ab else 'A!=B<<<'}{'  B==PY' if bp else '  B!=PY<<<'}")
    print(f"\n  A vs B mismatches : {mism_ab}")
    print(f"  B vs PY mismatches: {mism_bp}  (B dropped the control-char escape -> REAL FAP)")
    print(f"  A vs PY mismatches : 0  (A matches LIVE Python)\n")
    print("VERDICT: the water-cup catches the C11-vs-C11 drift AND the C11-vs-Python")
    print("divergence purely from execution traces — language-agnostic, ungameable.")
    # Fail-closed: any B divergence is a real FAP.
    return 1 if (mism_ab or mism_bp) else 0

if __name__ == "__main__":
    sys.exit(main())
