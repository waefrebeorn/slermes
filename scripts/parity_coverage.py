#!/usr/bin/env python3
"""parity_coverage.py — which reference modules the C tree actually covers.

The battleground scanner and the strict scorer disagree on what "covered"
means, and neither answers the question that matters when picking the next
port: WHICH modules have no port at all, and which of those are imported by
the most other modules (i.e. the most reusable)?

Answers, from the PoP annotations in src/**/*.c:

  --fanin        uncovered modules ranked by importer count (the reusable list)
  --dirs         covered/total rollup per top-level directory
  --by-module    one line per module: covered | defs | importers | module
  --filter SUB   only modules under this path prefix
  --json         machine-readable

The annotation is the unit of credit: a module is covered when at least one
`/* PoP: <cfunc> @ <module>:<func> */` names it. Mis-credited annotations
(naming a re-exporting module instead of the defining one) therefore hide real
coverage — run scripts/audit_annotations.py first.
"""
import argparse
import ast
import glob
import io
import json
import os
import re
import sys
from collections import defaultdict

HERMES = "C:/Users/eman5/hermes-agent"
REPO = "C:/Users/eman5/slermes"
SKIP = ("tests/", "optional-skills/", "skills/", "scripts/")
PREFIXES = ("agent", "tools", "gateway", "cron", "hermes_cli", "cli",
            "hermes_state", "pm", "plugins", "tui_gateway", "hermes_platform",
            "acp_adapter", "provider")


def load_reference():
    """module -> {defs, importers}; importers computed from the import graph."""
    defs = {}
    trees = {}
    for sub in PREFIXES:
        root = os.path.join(HERMES, sub)
        for p in glob.glob(root + "/**/*.py", recursive=True):
            rel = os.path.relpath(p, HERMES).replace("\\", "/")
            if rel.startswith(SKIP):
                continue
            try:
                src = io.open(p, encoding="utf-8", errors="replace").read()
                tree = ast.parse(src)
            except (SyntaxError, PermissionError, OSError):
                continue
            names = {n.name for n in ast.walk(tree)
                     if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef))}
            defs[rel] = names
            trees[rel] = tree

    files = set(defs)

    def resolve(path):
        return path if path in files else (path + ".py" if path + ".py" in files
                                           else None)

    fanin = defaultdict(set)
    for mod, tree in trees.items():
        for n in ast.walk(tree):
            if isinstance(n, ast.Import):
                for a in n.names:
                    t = resolve(a.name.replace(".", "/"))
                    if t:
                        fanin[t].add(mod)
            elif isinstance(n, ast.ImportFrom):
                if not n.module or n.level:
                    continue
                base = n.module.replace(".", "/")
                t = resolve(base)
                if t:
                    fanin[t].add(mod)
                    continue
                for a in n.names:
                    sub = resolve(base + "/" + a.name.replace(".", "/"))
                    if sub:
                        fanin[sub].add(mod)
    return defs, fanin


def load_corpus():
    """Every module named by a PoP annotation in the C tree."""
    corpus = set()
    for p in glob.glob(REPO + "/src/**/*.c", recursive=True):
        t = io.open(p, encoding="utf-8", errors="replace").read()
        for m in re.finditer(r"PoP:\s*\S+\s*@\s*([\w\./]+):(\w+)", t):
            if m.group(1).endswith(".py"):
                corpus.add(m.group(1))
    return corpus


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--fanin", action="store_true",
                    help="uncovered modules ranked by importer count")
    ap.add_argument("--dirs", action="store_true",
                    help="covered/total rollup per top-level directory")
    ap.add_argument("--by-module", action="store_true")
    ap.add_argument("--filter", default=None)
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    defs, fanin = load_reference()
    corpus = load_corpus()

    modules = {m: v for m, v in defs.items()
               if args.filter is None or m.startswith(args.filter)}
    covered = {m for m in modules if m in corpus}
    uncovered = sorted(set(modules) - covered)

    if args.json:
        print(json.dumps({
            "modules": len(modules),
            "covered": len(covered),
            "uncovered": len(uncovered),
            "by_module": {m: {"defs": len(defs[m]),
                              "importers": len(fanin.get(m, ())),
                              "covered": m in corpus} for m in modules},
        }, indent=2))
        return 0

    if args.dirs:
        roll = defaultdict(lambda: [0, 0])
        for m in modules:
            key = m.split("/")[0] if "/" in m else "."
            roll[key][1] += 1
            if m in corpus:
                roll[key][0] += 1
        print(f'{"covered":>8} {"total":>6} {"pct":>5}  area')
        for k in sorted(roll, key=lambda k: -roll[k][1]):
            c, t = roll[k]
            print(f'{c:8d} {t:6d} {100.0 * c / t:4.0f}%  {k}')
        return 0

    if args.by_module:
        for m in sorted(modules):
            print(f'{"COVERED" if m in corpus else "       "} '
                  f'{len(defs[m]):4d} defs {len(fanin.get(m, ())):4d} imp  {m}')
        return 0

    # default / --fanin
    rows = sorted(((len(fanin.get(m, ())), len(defs[m]), m) for m in uncovered),
                  reverse=True)
    print(f'modules: {len(modules)}  covered: {len(covered)}  '
          f'uncovered: {len(uncovered)}')
    print(f'{"importers":>10} {"defs":>5}  module')
    for imp, nd, m in rows[:60]:
        print(f'{imp:10d} {nd:5d}  {m}')
    return 0


if __name__ == "__main__":
    sys.exit(main())
