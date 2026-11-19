#!/usr/bin/env python3
"""Honest watercup verdict reporting.

The gate classifier stamps classification=PORTED when a harness EXISTS for a
tagged C function (fix_all_gates.py / fix_ported_gates.py) — existence, not
equivalence. This tool reports what the cups actually proved:

  VERIFIED   ok:true  and mismatches==0   (both sides ran, cups identical)
  MISMATCH   mismatches>0                 (real divergence — port or oracle bug)
  BROKEN     ok:false and mismatches==0   (a side errored; nothing proven)

and cross-tabs against the claimed classification to expose false greens.
Read-only; safe to run while a fleet is in flight (partial results honest).
"""
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
RESULTS = REPO / "tests/watercup/build/results.jsonl"


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 and not sys.argv[1].startswith("-") else RESULTS
    rows = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        try:
            rows.append(json.loads(line))
        except json.JSONDecodeError:
            pass
    verdict = Counter()
    claimed = Counter()
    false_green = []
    mismatch_gates = []
    for r in rows:
        ok = bool(r.get("ok"))
        mm = int(r.get("mismatches") or 0)
        cl = r.get("classification", "?")
        claimed[cl] += 1
        if ok and mm == 0:
            v = "VERIFIED"
        elif mm > 0:
            v = "MISMATCH"
            mismatch_gates.append(r.get("gate"))
        else:
            v = "BROKEN"
        verdict[v] += 1
        if cl == "PORTED" and v != "VERIFIED":
            false_green.append(r.get("gate"))
    total = len(rows)
    print(f"watercup truth: {total} gate results")
    for v in ("VERIFIED", "MISMATCH", "BROKEN"):
        print(f"  {v:9s} {verdict[v]:6d}")
    print(f"claimed classifications: {dict(claimed)}")
    print(f"false greens (claimed PORTED, not verified): {len(false_green)}")
    if "--list" in sys.argv:
        out = REPO / "tests/watercup/build/truth_lists.json"
        out.write_text(json.dumps(
            {"false_green": false_green, "mismatch": mismatch_gates}, indent=1),
            encoding="utf-8")
        print("lists:", out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
