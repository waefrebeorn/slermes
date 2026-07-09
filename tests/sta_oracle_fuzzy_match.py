"""
sta_oracle_fuzzy_match.py — oracle for tools/fuzzy_match._map_normalized_positions.
"""
import sys, json, os, ast
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from tools.fuzzy_match import _map_normalized_positions

lines = [l for l in sys.stdin if l.strip().startswith("{")]
mism = 0
for ln in lines:
    rec = json.loads(ln)
    original, normalized = rec["original"], rec["normalized"]
    matches = [(m[0], m[1]) for m in rec["matches"]]
    got = [(s, e) for s, e in rec["out"]]
    exp = _map_normalized_positions(original, normalized, matches)
    if got != exp:
        mism += 1
        print(f"MISMATCH orig={original!r} norm={normalized!r} matches={matches} got={got} exp={exp}")
print(f"FUZZY oracle: {len(lines)} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
