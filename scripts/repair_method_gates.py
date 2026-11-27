#!/usr/bin/env python3
"""repair_method_gates.py — point a gate's py_fn at Class.method when it names a method bare.

931 PORT gates name a Python function that is not module-level: it is a method (or a
nested function) in the file. A twin that resolves `can_change_plan` at module level
finds nothing, returns an ERROR cup for every input, and the gate can never match --
it is one of the main sources of "the twin returns no comparable value".

This walks the file, finds the enclosing class of the named function, and rewrites
gate.json's py_fn to `Class.name` when exactly one class contains it. Ambiguity is
refused (two classes with the same method name), not guessed.

Verification is the gate's own: after rewriting, re-run the gate and keep the change
only when it builds and the cups are not an ERROR/TWIN BLIND artifact.

Usage:
  python scripts/repair_method_gates.py [--apply] [gate ...]
"""
from __future__ import annotations

import argparse
import ast
import json
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
GATES = REPO / "tests" / "watercup" / "gates"
PROV = REPO / "tests" / "watercup" / "build" / "gate_provenance.json"
QUARRY = Path(r"C:\Users\eman5\hermes-agent")


def owners(tree, name: str) -> list[str]:
    """Dotted paths (Class.method) for every definition of `name`, at any depth."""
    out: list[str] = []

    def walk(node, prefix):
        for child in ast.iter_child_nodes(node):
            if isinstance(child, ast.ClassDef):
                walk(child, prefix + [child.name])
            elif isinstance(child, (ast.FunctionDef, ast.AsyncFunctionDef)):
                if child.name == name and prefix:
                    out.append(".".join(prefix + [name]))
                walk(child, prefix)

    walk(tree, [])
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("gates", nargs="*")
    ap.add_argument("--apply", action="store_true")
    args = ap.parse_args()

    rows = [r for r in json.loads(PROV.read_text()) if r["class"] == "PORT"]
    names = [n.strip() for n in args.gates] or [r["gate"] for r in rows]
    if args.gates:
        rows = [r for r in rows if r["gate"] in names]

    fixed = amb = gone = 0
    cache: dict[str, object] = {}
    for r in rows:
        gj = GATES / r["gate"] / "gate.json"
        if not gj.is_file():
            continue
        g = json.loads(gj.read_text())
        rel = (g.get("py_module") or "").strip()
        fn = (g.get("py_fn") or "").strip()
        if not rel.endswith(".py") or not fn or "." in fn:
            continue
        if rel not in cache:
            try:
                cache[rel] = ast.parse((QUARRY / rel).read_text(encoding="utf-8",
                                                               errors="ignore"))
            except (OSError, SyntaxError):
                cache[rel] = None
        tree = cache[rel]
        if tree is None:
            gone += 1
            continue
        cands = owners(tree, fn)
        if not cands:
            gone += 1
            continue
        if len(cands) > 1:
            amb += 1
            print("ambiguous %-56s %s -> %s" % (r["gate"], fn, ", ".join(cands[:3])))
            continue
        fixed += 1
        if args.apply:
            g["py_fn"] = cands[0]
            g["py_fn_repaired_from"] = fn
            gj.write_text(json.dumps(g, indent=2) + "\n")
    print("%d repairable (unique Class.method), %d ambiguous, %d no such name, of %d"
          % (fixed, amb, gone, len(rows)))
    if not args.apply:
        print("dry run: pass --apply to write gate.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
