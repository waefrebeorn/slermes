#!/usr/bin/env python3
"""tests/watercup/match_c_vs_py.py — prove Python-vs-C cup identity.

Compiles demo_c.c (with watercup.h) and runs it over the demo stream, then
compares each cup hash to the Python Cup output. If every cup matches, the
two languages carve the SAME shape -> the water-cup oracle is sound across
the Python/C boundary (which is exactly what slermes parity needs).
"""
import os, sys, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from watercup_py import Cup

STREAM = [0.0, 0.0, 5.0, 0.004, 0.00001, 1.23, 999.99, 0.005, 0.0, 250.5]

# Python side (must mirror demo_c.c's canonical encodings)
def py_cup(amount):
    c = Cup("fmt_est_cost")
    c.valstr("%.17g" % amount)
    if amount == 0.0:
        c.branch(True)
        out = "$0.00"
    else:
        c.branch(False)
        c.valstr(str(amount < 0.01))   # "True"/"False"
        if amount < 0.01:
            c.branch(True)
            s = "~$%.4f" % amount
            c.valstr(s)
            if s == "~$0.0000":
                c.branch(True)
                out = "~$<0.0001"
            else:
                c.branch(False)
                out = s
        else:
            c.branch(False)
            c.valstr("%.17g" % amount)
            out = "~$%.2f" % amount
    c.valstr(out)   # mirror demo_c.c line 40 (WCUP_VALSTR(out) after if/else)
    return c.finalize()

def main():
    # compile
    cc = subprocess.run(
        ["cc", "-I", HERE, os.path.join(HERE, "demo_c.c"),
         "-o", os.path.join(HERE, "demo_c")],
        capture_output=True, text=True)
    if cc.returncode != 0:
        print("COMPILE FAIL:\n", cc.stderr)
        return 1
    # run C over stream
    args = [os.path.join(HERE, "demo_c")] + [repr(x) for x in STREAM]
    r = subprocess.run(args, capture_output=True, text=True)
    c_cups = r.stdout.strip().split("\n")
    py_cups = [py_cup(x) for x in STREAM]

    print("=== Python-vs-C water-cup identity ===")
    ok = True
    for i, (x, pc, cc_) in enumerate(zip(STREAM, py_cups, c_cups)):
        match = (pc == cc_)
        ok = ok and match
        print(f"  x={x!s:>8}  py={pc}  c={cc_}  {'OK' if match else 'DIFF'}")
    print("\nVERDICT:", "CUPS IDENTICAL ACROSS PYTHON & C" if ok
          else "MISMATCH — cup format diverged")
    return 0 if ok else 1

if __name__ == "__main__":
    sys.exit(main())
