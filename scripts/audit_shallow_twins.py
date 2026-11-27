"""Measure gates whose twin cannot be parity evidence, whatever the cup says.

A generated twin hands the fixture string straight to the Python function:

    result = fn(inp)

That is only a fair comparison when the Python function takes one positional string
with that meaning. For a method that takes keyword-only booleans, a config dict, or
three unrelated arguments, the call raises or silently answers a different question,
and the gate's "match" is as meaningless as the port's "answer" it compares against.

can_change_plan was the textbook case: the twin passed the raw string to a lax dummy
receiver, the port printed a constant, and the gate agreed with itself.

This audit reads each gate's twin.py and each Python signature and classifies:

  RAW-OK          the twin passes the fixture straight through and the Python
                  function accepts exactly one positional argument
  SHALLOW         the twin passes the fixture raw but the Python function needs
                  something else (keyword-only, >1 required, dict/config object)
  SHAPE           the twin builds arguments itself (custom trace) -- inspect by hand
  NO-TWIN         twin.py missing or has no trace

Usage:
    python scripts/audit_shallow_twins.py [--list SHALLOW] [--json out.json]
"""
from __future__ import annotations

import argparse
import ast
import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
GATES = REPO / "tests" / "watercup" / "gates"
QUARRY = Path(r"C:\Users\eman5\hermes-agent")

_sig_cache: dict[tuple[str, str], str] = {}


def py_signature(module: str, fn: str) -> str:
    key = (module, fn)
    if key in _sig_cache:
        return _sig_cache[key]
    path = QUARRY / module
    verdict = "MISSING"
    if path.exists():
        try:
            tree = ast.parse(path.read_text(errors="ignore"))
        except SyntaxError:
            tree = None
        if tree is not None:
            for node in ast.walk(tree):
                if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name == fn:
                    a = node.args
                    pos = [p.arg for p in a.posonlyargs + a.args]
                    required = len(pos) - len(a.defaults)
                    has_kwonly = bool(a.kwonlyargs)
                    req_kwonly = [k.arg for k, d in zip(a.kwonlyargs, a.kw_defaults) if d is None]
                    if has_kwonly or required != 1:
                        bits = [f"pos={pos}", f"required={required}"]
                        if has_kwonly:
                            bits.append(f"kwonly={[k.arg for k in a.kwonlyargs]}")
                        if req_kwonly:
                            bits.append(f"required_kwonly={req_kwonly}")
                        verdict = "NEEDS-ARGS " + " ".join(bits)
                    else:
                        verdict = "ONE-POSITIONAL"
                    break
    _sig_cache[key] = verdict
    return verdict


def twin_shape(path: Path) -> str:
    if not path.exists():
        return "NO-TWIN"
    txt = path.read_text(errors="ignore")
    # a custom trace() that builds arguments is doing real work
    if re.search(r"def trace\(", txt):
        return "SHAPE"
    if "TwinBase" in txt and re.search(r"_t\s*=\s*_Twin\(\)", txt):
        return "RAW"
    return "NO-TWIN"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--json")
    ap.add_argument("--list", dest="show")
    a = ap.parse_args()

    rows = []
    for gd in sorted(GATES.iterdir()):
        gj = gd / "gate.json"
        if not gj.is_file():
            continue
        try:
            g = json.loads(gj.read_text())
        except Exception:
            continue
        mod, fn = (g.get("py_module") or "").strip(), (g.get("py_fn") or "").strip()
        shape = twin_shape(gd / "twin.py")
        need = py_signature(mod, fn) if mod and fn else "MISSING"
        if shape == "RAW":
            cls = "RAW-OK" if need == "ONE-POSITIONAL" else "SHALLOW"
        elif shape == "SHAPE":
            cls = "SHAPE"
        else:
            cls = "NO-TWIN"
        rows.append({"gate": gd.name, "class": cls, "twin": shape, "py_sig": need,
                     "py_module": mod, "py_fn": fn})

    counts: dict[str, int] = {}
    for r in rows:
        counts[r["class"]] = counts.get(r["class"], 0) + 1
    for k, v in sorted(counts.items(), key=lambda kv: -kv[1]):
        print(f"{v:6d}  {k}")
    print(f"{len(rows):6d}  gates examined")

    if a.json:
        Path(a.json).write_text(json.dumps(rows, indent=1))
        print(f"detail -> {a.json}")
    if a.show:
        for r in rows:
            if r["class"] == a.show:
                print(f'  {r["gate"][:70]:72s} {r["py_module"]}::{r["py_fn"]}  {r["py_sig"]}')
    return 0


if __name__ == "__main__":
    sys.exit(main())
