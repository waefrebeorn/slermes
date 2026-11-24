#!/usr/bin/env python3
"""wave_backlog.py -- keep a queue of ready-to-dispatch waves, so seeding the
pipeline never costs a thinking pause.

Selection (same rules every wave, so the queue cannot drift):
  * module is in docs/gap-c-homes.json and not already a value of any
    docs/wave*-plan.json;
  * src/<module>.c exists and is under 900 bytes (that is the stub);
  * the live Python source exists in the quarry and is under 40 KB;
  * 1..5 top-level def/async def (6+ is not a wave unit);
  * no `tests/`, `scripts/` or `nix/` path segment (those are harnesses, not
    library code, and their names carry hyphens that break gate ids).
Gate id is mechanical: w<N>_<basename without underscores, first 10 chars>_<md5(module)[:6]>.

    python scripts/wave_backlog.py --ahead 3 --size 5      # waves up to max+3
    python scripts/wave_backlog.py --list                  # what is staged

Writes docs/wave<N>-plan.json plus docs/.w<N>-assign-<i>.txt (snake-dealt by
descending function count so each batch carries a mix).
"""
from __future__ import annotations

import argparse
import ast
import glob
import hashlib
import json
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
QUARRY = "C:/Users/eman5/hermes-agent"


def existing():
    plans, used = {}, set()
    for path in sorted(glob.glob(os.path.join(REPO, "docs", "wave*-plan.json"))):
        base = os.path.basename(path)
        try:
            wave = int(base[len("wave"):-len("-plan.json")])
        except ValueError:
            continue
        plan = json.load(open(path, encoding="utf-8"))
        plans[wave] = plan
        used.update(plan.values())
    return plans, used


def gate_id(wave, module):
    base = os.path.basename(module).replace("_", "")[:10]
    return "w%d_%s_%s" % (wave, base, hashlib.md5(module.encode()).hexdigest()[:6])


def candidates(used, want, lo=1, hi=5):
    gap = json.load(open(os.path.join(REPO, "docs", "gap-c-homes.json"), encoding="utf-8"))
    rows = []
    for mod in gap:
        if mod in used:
            continue
        parts = mod.split("/")
        if os.path.basename(mod) in ("__init__", "__main__"):
            continue
        if "tests" in parts or "scripts" in parts or parts[0] in ("scripts", "nix"):
            continue
        stub = os.path.join(REPO, "src", mod + ".c")
        src = os.path.join(QUARRY, mod + ".py")
        if not (os.path.isfile(stub) and os.path.getsize(stub) < 900):
            continue
        if not (os.path.isfile(src) and os.path.getsize(src) < 40000):
            continue
        try:
            tree = ast.parse(open(src, encoding="utf-8", errors="replace").read())
        except SyntaxError:
            continue
        fns = [n for n in tree.body if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef))]
        if not (lo <= len(fns) <= hi):
            continue
        rows.append((len(fns), os.path.getsize(src), mod))
    rows.sort(key=lambda t: (t[0], t[1]))
    return rows[:want]


def pick(used, want, max_fns=9):
    """Fewest-function modules first; widen the band only when a band runs dry."""
    rows = []
    for lo, hi in ((1, 5), (6, 7), (8, max_fns)):
        if len(rows) >= want or lo > max_fns:
            break
        rows += candidates(used, want - len(rows), lo, min(hi, max_fns))
    rows.sort(key=lambda t: (t[0], t[1]))
    return rows[:want]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ahead", type=int, default=2, help="keep this many unspent waves staged")
    ap.add_argument("--per-wave", type=int, default=60)
    ap.add_argument("--size", type=int, default=5, help="gates per batch (subagent unit)")
    ap.add_argument("--max-fns", type=int, default=9,
                    help="widen the selection band to this many top-level functions")
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()

    plans, used = existing()
    if args.list:
        for wave in sorted(plans):
            print("wave %-3d %2d gates  %s" % (wave, len(plans[wave]),
                                               os.path.basename("docs/wave%d-plan.json" % wave)))
        return 0

    top = max(plans) if plans else 0
    # Frontier = the newest wave the queue has actually taken work from. The
    # backlog keeps that many waves staged *beyond* it, so seeding the next
    # wave is always a file read, never a generation pause.
    frontier = top
    qstate = os.path.join(REPO, ".testbin", "wave_queue.json")
    if os.path.exists(qstate):
        waves = [b["wave"] for b in json.load(open(qstate, encoding="utf-8"))["batches"]]
        if waves:
            frontier = max(waves)
    target_top = frontier + args.ahead

    made = []
    while top < target_top:
        top += 1
        rows = pick(used, args.per_wave, args.max_fns)
        if not rows:
            print("gap-c-homes exhausted for 1-5 function modules", file=sys.stderr)
            return 1
        plan = {gate_id(top, m): m for _f, _s, m in rows}
        with open(os.path.join(REPO, "docs", "wave%d-plan.json" % top), "w",
                  encoding="utf-8", newline="\n") as fh:
            json.dump(plan, fh, indent=2, sort_keys=True)
            fh.write("\n")
        order = sorted(rows, key=lambda t: (-t[0], t[1]))
        inv = {v: k for k, v in plan.items()}
        nb = max(1, -(-len(order) // args.size))  # ceil: batches of <=size gates
        batches = [order[i::nb] for i in range(nb)]
        for i, batch in enumerate(batches):
            with open(os.path.join(REPO, "docs", ".w%d-assign-%d.txt" % (top, i)), "w",
                      encoding="utf-8", newline="\n") as fh:
                fh.write("\n".join("%s\t%s" % (inv[m], m) for _f, _s, m in batch) + "\n")
        used.update(plan.values())
        made.append((top, len(plan)))

    for wave, n in made:
        print("staged wave %d: %d gates" % (wave, n))
    if not made:
        print("backlog full: waves staged through %d" % top)
    return 0


if __name__ == "__main__":
    sys.exit(main())
