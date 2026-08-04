#!/usr/bin/env python3
"""
gap_triage.py — rank REAL_GAP modules by closure value.

WHY: the parity scanner emits 1,958 REAL_GAP across 230 modules, but not all
gaps are equally actionable. A module whose gaps are small pure functions
(with no IO/network/DB deps) can be closed in minutes with oracle
verification; an IO-heavy module needs a whole harness. This tool scores every
module so the next closure pass starts with the highest-value targets.

Scoring (higher = close it first):
  - gap size (log-scaled so a 79-gap module doesn't swamp everything)
  - pure ratio: fraction of gaps that are simple `def fn(...)` with no
    async/class/decorators and a short body — these are the mechanical closes
  - oracle availability: modules with an existing oracle harness are worth
    more (closures get verified, not just asserted)
  - ported base: already-ported modules are warm (infra exists to extend)

Usage:
  python3 scripts/gap_triage.py              # full ranked table
  python3 scripts/gap_triage.py --top 20     # top N by score
  python3 scripts/gap_triage.py --pure       # only modules with pure-leaf gaps
  python3 scripts/gap_triage.py --json       # machine-readable
"""
import argparse
import json
import math
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCAN = os.path.join(REPO, "live_parity_scan.json")
ORACLE_DIR = os.path.join(REPO, "tests", "oracle", "fixtures")

# Body-size heuristic: a gap is "pure" if its Python source line span is small.
# We can't cheaply parse AST here; the scanner JSON carries line numbers, so
# we approximate purity by (a) not async, (b) no parent class, (c) no
# decorators, (d) name starts with underscore or is a plain helper.
def load_scan():
    with open(SCAN, encoding="utf-8") as f:
        return json.load(f)


def module_gaps(mods, key):
    m = mods.get(key)
    if not m:
        return []
    return [g for g in m.get("gaps", []) if g.get("classification") == "REAL_GAP"]


def is_pure(gap):
    feat = gap.get("python_feature", {})
    if feat.get("is_async"):
        return False
    if feat.get("parent_class"):
        return False
    if feat.get("decorators"):
        return False
    return True


def has_oracle(mod_key):
    """Oracle fixture dir exists for this module?"""
    name = mod_key.split("/")[-1].replace(".py", "")
    return os.path.isdir(os.path.join(ORACLE_DIR, name))


def score(mod_key, gaps):
    n = len(gaps)
    if n == 0:
        return 0.0
    pure = sum(1 for g in gaps if is_pure(g))
    pure_ratio = pure / n
    oracle = 1.5 if has_oracle(mod_key) else 0.0
    # log-scaled size + pure bonus + oracle bonus
    return round(math.log1p(n) * 10 + pure_ratio * 30 + oracle, 1)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--top", type=int, default=0, help="show only top N")
    ap.add_argument("--pure", action="store_true",
                    help="only modules with >=50% pure-leaf gaps")
    ap.add_argument("--json", action="store_true", help="JSON output")
    args = ap.parse_args()

    scan = load_scan()
    mods = scan.get("modules", {})
    rows = []
    for key in mods:
        gaps = module_gaps(mods, key)
        if not gaps:
            continue
        n = len(gaps)
        pure = sum(1 for g in gaps if is_pure(g))
        oracle = has_oracle(key)
        rows.append((score(key, gaps), n, pure, oracle, key))
    rows.sort(reverse=True)
    if args.pure:
        rows = [r for r in rows if r[2] >= max(1, r[1] // 2)]
    if args.top:
        rows = rows[: args.top]

    if args.json:
        print(json.dumps(
            [{"score": r[0], "real_gaps": r[1], "pure": r[2],
              "oracle": r[3], "module": r[4]} for r in rows], indent=1))
        return

    total_pure = sum(r[2] for r in rows)
    print(f"=== gap triage: {len(rows)} modules with REAL_GAP "
          f"({sum(r[1] for r in rows)} gaps, {total_pure} pure-leaf) ===")
    print(f"{'SCORE':>5} {'GAPS':>4} {'PURE':>4}  ORACLE  MODULE")
    print("-" * 60)
    for s, n, pure, oracle, key in rows:
        print(f"{s:5.1f} {n:4d} {pure:4d}  {'Y' if oracle else '-':5}  {key}")


if __name__ == "__main__":
    main()
