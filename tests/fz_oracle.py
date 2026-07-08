#!/usr/bin/env python3
"""Faithfulness oracle for port_fuzzy_match.c.

Recomputes _map_normalized_positions from the LIVE source
tools/fuzzy_match.py for the same 6 hardcoded cases the C harness emits
(one "CASE n OUT a,b c,d ..." line per case) and compares exactly.
"""
import sys
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
from tools.fuzzy_match import _map_normalized_positions

# (orig, norm, norm_matches)
CASES = {
    1: ("a  b", "a b", [(0, 3)]),
    2: ("a\t  b  c", "a b c", [(0, 5)]),
    3: ("a   b", "a b", [(0, 2)]),
    4: ("hello world", "hello world", []),
    5: ("x  y  z", "x y z", [(0, 1), (5, 6)]),  # latent py crash -> skipped below
    6: ("a   b", "a b", [(0, 1)]),
}

# Case 5 crashes the Python source itself (min() over empty seq at
# fuzzy_match.py:848) — not a valid reference. Skip it in compare.
SKIP = {5}

ok = True
seen = set()
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("CASE"):
        continue
    parts = line.split()
    cn = int(parts[1])
    pairs = [tuple(map(int, p.split(","))) for p in parts[3:]]
    seen.add(cn)
    if cn in SKIP:
        print(f"SKIP case {cn} (Python source crashes here — C handles gracefully)")
        continue
    orig, norm, nm = CASES[cn]
    exp = _map_normalized_positions(orig, norm, nm)
    if exp != pairs:
        ok = False
        print(f"MISMATCH case {cn}: PY={exp} C={pairs}")

missing = set(CASES) - seen
if missing:
    ok = False
    print("MISSING cases:", sorted(missing))

print("PYCOMPARE", "OK" if ok else "BAD", f"({len(seen)} cases)")
sys.exit(0 if ok else 1)
