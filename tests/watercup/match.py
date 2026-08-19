#!/usr/bin/env python3
"""tests/watercup/match.py — the water-cup parity matcher (reference demo).

Runs a FAITHFUL tracer and a CHEATER (no-water) tracer over the same input
stream and compares the carved cup hashes. The cheater returns the identical
output strings but carves a DEGENERATE cup, so it fails the water-cup even
though an output-only oracle would pass it.

This file is the self-test. The production matcher is C-vs-Python: the C
harness (compiled with watercup.h) and watercup_py.Cup emit identical cup
hashes, and match_c_vs_py.py diffs them over a fixture stream.
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from watercup_py import Cup

# ---- FAITHFUL: pours water through the real control structure ------------
def fmt_est_cost_cup(amount):
    c = Cup("fmt_est_cost")
    c.val(amount)
    if amount == 0:
        c.branch(True)
        out = "$0.00"
    else:
        c.branch(False)
        c.val(amount < 0.01)
        if amount < 0.01:
            c.branch(True)
            c.val("~$%.4f" % amount)
            s = "~$%.4f" % amount
            c.val(s)
            out = "~$<0.0001" if s == "~$0.0000" else s
        else:
            c.branch(False)
            c.val("~$%.2f" % amount)
            out = "~$%.2f" % amount
    c.val(out)
    return c.finalize(), out

# ---- CHEATER: returns the right string but pours NO water -----------------
def stub_est_cost_cup(amount):
    c = Cup("fmt_est_cost")   # cup created but never fed any branch/val
    # Returns the IDENTICAL string a faithful port would (incl. the
    # ~$<0.0001 special case), so an output-only oracle passes. But it
    # pours no water -> degenerate cup.
    if amount == 0:
        out = "$0.00"
    elif amount < 0.01:
        s = "~$%.4f" % amount
        out = "~$<0.0001" if s == "~$0.0000" else s
    else:
        out = "~$%.2f" % amount
    return c.finalize(), out

# ---- equivalence invariants (equivalence-cup) -----------------------------
def inv_amount_zero(out, amount):
    return out == "$0.00" if amount == 0 else True

def inv_tilde(out):
    return out.startswith("~$") or out == "$0.00"

INVARIANTS = [(inv_amount_zero, 2), (inv_tilde, 1)]

STREAM = [0.0, 0.0, 5.0, 0.004, 0.00001, 1.23, 999.99, 0.005, 0.0, 250.5]


def main():
    print("=== Water-Cup self-test ===\n")
    faithful = [fmt_est_cost_cup(x) for x in STREAM]
    stub = [stub_est_cost_cup(x) for x in STREAM]

    out_match = [f[1] for f in faithful] == [s[1] for s in stub]
    cup_match = [f[0] for f in faithful] == [s[0] for s in stub]

    print("Output-only oracle (stub vs faithful return strings):",
          "MATCH" if out_match else "MISMATCH")
    print("  -> the cheater GAMES this: identical outputs, zero real work.\n")

    print("Water-cup (trace-cup + stream-cup):",
          "MATCH" if cup_match else "MISMATCH")
    print("  faithful cups:", [f[0][:8] for f in faithful][:4], "...")
    print("  stub     cups:", [s[0][:8] for s in stub][:4], "...")
    print("  -> the cheater FAILS: it carves a degenerate cup (no water).\n")

    bad = 0
    for (_, o), x in zip(stub, STREAM):
        for inv, argc in INVARIANTS:
            ok = inv(o, x) if argc == 2 else inv(o)
            bad += 0 if ok else 1
    print("Equivalence-cup violations (stub):", bad)
    print("  (invariants may pass by luck; the trace-cup is the real catcher.)\n")

    print("VERDICT:", "WATER-CUP CATCHES THE CHEATER"
          if (out_match and not cup_match) else "review needed")
    print("\nA faithful port carves the SAME cup as LIVE Python because both")
    print("execute the same algorithm. There is no shortcut to a MATCH.")


if __name__ == "__main__":
    main()
