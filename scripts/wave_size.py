#!/usr/bin/env python3
"""wave_size.py -- size-first planning: port the biggest modules that still have
no C code, decomposed into per-function-group gates.

The wave planner used to walk the gap inventory by *function count* (1-5, then
6-9, ...), which quietly skipped every large module: adapters and stores are
big precisely because they have 50-370 methods. That ladder ran dry with 468
modules and 10 MB of Python still unported, 9 of them over 100 KB.

This planner ranks by bytes instead:

    python scripts/wave_size.py --list 20           # biggest unported modules
    python scripts/wave_size.py --emit --wave 29 --per-wave 12

One module = one owner = one src/<module>.c (no two children ever write the
same port file). A module with more than --per-gate defs (default 18) is split
into several gates -- `<gate>_p1`, `_p2`, ... -- each with its own header,
harness, oracle and fixture, all linked against that one port object.

Assignment files (`docs/.size-assign-<i>.txt`) carry one line per module:
    <module>\t<gate1>,<gate2>,...
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


def inventory():
    gap = json.load(open(os.path.join(REPO, "docs", "gap-c-homes.json"), encoding="utf-8"))
    planned = set()
    for path in glob.glob(os.path.join(REPO, "docs", "wave*-plan.json")):
        planned.update(json.load(open(path, encoding="utf-8")).values())
    rows = []
    for mod in gap:
        parts = mod.split("/")
        if os.path.basename(mod) in ("__init__", "__main__"):
            continue
        if "tests" in parts or "scripts" in parts or parts[0] in ("scripts", "nix"):
            continue
        src = os.path.join(QUARRY, mod + ".py")
        stub = os.path.join(REPO, "src", mod + ".c")
        if not os.path.isfile(src):
            continue
        if os.path.isfile(stub) and os.path.getsize(stub) >= 900:
            continue  # already a real port
        try:
            tree = ast.parse(open(src, encoding="utf-8", errors="replace").read())
        except SyntaxError:
            continue
        defs = sum(1 for n in ast.walk(tree)
                   if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)))
        rows.append({"module": mod, "bytes": os.path.getsize(src), "defs": defs,
                     "planned": mod in planned})
    rows.sort(key=lambda r: -r["bytes"])
    return rows


def gate_base(wave, module):
    base = os.path.basename(module).replace("_", "")[:10]
    return "w%d_%s_%s" % (wave, base, hashlib.md5(module.encode()).hexdigest()[:6])


def gates_for(wave, module, defs, per_gate):
    n = max(1, -(-defs // per_gate))
    base = gate_base(wave, module)
    return [base] if n == 1 else ["%s_p%d" % (base, i + 1) for i in range(n)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", type=int, metavar="N")
    ap.add_argument("--emit", action="store_true")
    ap.add_argument("--wave", type=int)
    ap.add_argument("--per-wave", type=int, default=12)
    ap.add_argument("--per-gate", type=int, default=18, help="defs per gate")
    ap.add_argument("--per-child", type=int, default=12, help="modules per assignment batch")
    args = ap.parse_args()

    rows = inventory()
    if args.list:
        for r in rows[:args.list]:
            print("%8d B  %3d defs  planned=%d  %s" % (r["bytes"], r["defs"], r["planned"], r["module"]))
        print("---- %d unported modules, %d unplanned, %.1f MB"
              % (len(rows), sum(1 for r in rows if not r["planned"]),
                 sum(r["bytes"] for r in rows) / 1e6))
        return 0

    if not args.emit:
        print("nothing to do: pass --list N or --emit --wave N")
        return 0
    if not args.wave:
        print("--emit needs --wave N", file=sys.stderr)
        return 2

    chosen = [r for r in rows if not r["planned"]][:args.per_wave]
    if not chosen:
        print("no unplanned modules left", file=sys.stderr)
        return 1
    plan = {}
    for r in chosen:
        for gate in gates_for(args.wave, r["module"], r["defs"], args.per_gate):
            plan[gate] = r["module"]
    path = os.path.join(REPO, "docs", "wave%d-plan.json" % args.wave)
    with open(path, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(plan, fh, indent=2, sort_keys=True)
        fh.write("\n")

    # one assignment line per module: module<TAB>gate,gate,...
    per_mod = {}
    for gate, mod in plan.items():
        per_mod.setdefault(mod, []).append(gate)
    lines = ["%s\t%s" % (mod, ",".join(sorted(gates))) for mod, gates in per_mod.items()]
    nb = max(1, -(-len(lines) // args.per_child))
    for i in range(nb):
        with open(os.path.join(REPO, "docs", ".size%d-assign-%d.txt" % (args.wave, i)), "w",
                  encoding="utf-8", newline="\n") as fh:
            fh.write("\n".join(lines[i::nb]) + "\n")
    print("wave %d: %d modules, %d gates, %d children, %.1f MB of Python"
          % (args.wave, len(per_mod), len(plan), nb,
             sum(r["bytes"] for r in chosen) / 1e6))
    for mod, gates in sorted(per_mod.items(), key=lambda kv: -len(kv[1]))[:6]:
        print("  %-60s %2d gates" % (mod, len(gates)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
