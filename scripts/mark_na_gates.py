#!/usr/bin/env python3
"""mark_na_gates.py — declare the gates that cannot discriminate, with a reason.

A water-cup gate compares two implementations over a list of inputs. If the Python
function takes no arguments, or never reads the one it has, no fixture can move the
answer: the gate's cups are one constant on both sides and it can never prove
anything. Those gates are not evidence and they are not defects either — they are
N/A, and the audit now honours an explicit `na_reason` so the fleet stops counting
them as PORT.

Two airtight categories only:

  zero-params   the Python function declares no parameters at all
  ignores-param the Python function never mentions its parameter in its body

Neither says the port is right. Both say this gate cannot be the thing that proves
it. A hand-written check that observes the function's real effect (a file, a
global, a return of a stateful object) is the only way to cover those, and that is
a different gate, not this one.

Usage:
  python scripts/mark_na_gates.py            # dry run, prints what it would mark
  python scripts/mark_na_gates.py --apply    # write na_reason into gate.json
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

_cache: dict[str, object] = {}


def _tree(rel: str):
    if rel not in _cache:
        try:
            _cache[rel] = ast.parse((QUARRY / rel).read_text(encoding="utf-8",
                                                           errors="ignore"))
        except (OSError, SyntaxError):
            _cache[rel] = None
    return _cache[rel]


def classify(g: dict) -> tuple[str, str] | None:
    rel = (g.get("py_module") or "").strip()
    fn = (g.get("py_fn") or "").strip()
    if not rel.endswith(".py") or not fn:
        return None
    tree = _tree(rel)
    if tree is None:
        return None
    name = fn.split(".")[-1]
    node = None
    for n in tree.body:
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name == name:
            node = n
            break
    if node is None:
        return None
    args = [a.arg for a in node.args.posonlyargs + node.args.args + node.args.kwonlyargs
            if a.arg not in ("self", "cls")]
    if not args:
        return ("zero-params",
                "Python fn takes no arguments, so no fixture can vary the answer; "
                "the gate is one constant on both sides and cannot prove parity")
    used = {x.id for x in ast.walk(node) if isinstance(x, ast.Name)}
    if not any(a in used for a in args):
        return ("ignores-param",
                "Python fn never reads its parameter, so no fixture can vary the "
                "answer; a check of its real effect (file/global/state) is a "
                "different gate")
    return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    args = ap.parse_args()

    rows = {r["gate"]: r for r in json.loads(PROV.read_text())}
    counts: dict[str, int] = {}
    marked = 0
    for gd in sorted(GATES.iterdir()):
        gj = gd / "gate.json"
        if not gj.is_file():
            continue
        row = rows.get(gd.name)
        if not row or row["class"] != "PORT":
            continue
        g = json.loads(gj.read_text())
        if (g.get("na_reason") or "").strip():
            continue
        verdict = classify(g)
        if not verdict:
            continue
        kind, reason = verdict
        counts[kind] = counts.get(kind, 0) + 1
        marked += 1
        if args.apply:
            g["na_reason"] = reason
            g["na_class"] = kind
            gj.write_text(json.dumps(g, indent=2) + "\n")
    for k, v in sorted(counts.items()):
        print("%-14s %5d" % (k, v))
    print("%d gate(s) %s" % (marked, "marked N/A" if args.apply else "would be marked N/A"))
    if args.apply and marked:
        print("re-run scripts/audit_gate_provenance.py to refresh the classes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
