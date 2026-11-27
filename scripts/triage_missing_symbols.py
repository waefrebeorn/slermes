#!/usr/bin/env python3
"""triage_missing_symbols.py — gates whose harness calls a symbol the port lacks.

Measured over the 776 MISSING gates (provenance audit):

  634  no definition anywhere   — the harness names a function that never existed
                                  in any source file. It can only fail to link.
  100  static in the port       — the function IS implemented, but `static`, so the
                                  gate cannot call it. Real code with no evidence.
   42  defined-not-exported     — a stub or an unrelated unit defines it.

Two honest outcomes, never a resave:

  --apply-static   drop `static` on the definition, but only when the file also
                   carries a PoP annotation for the gate's (module, function) and
                   the C symbol's base name matches the Python function name. That
                   is the port saying "this is the implementation", not a guess.
  --apply-gap      rewrite the harness as a GAP sentinel, because a harness for a
                   symbol no source defines is not a failing test — it is a
                   placeholder that should read as a gap (and stop being a
                   build failure in every fleet run).

Without flags it only reports.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
GATES = REPO / "tests" / "watercup" / "gates"
ROWS = REPO / "tests" / "watercup" / "build" / "gate_provenance.json"
sys.path.insert(0, str(REPO / "tests" / "watercup"))

DEF = re.compile(r"^(?P<ret>[A-Za-z_][\w\s\*]*?)\b(?P<name>[A-Za-z_]\w*)\s*\([^;{}]*\)\s*\{", re.M)
POP = re.compile(r"PoP:\s*([A-Za-z_]\w*)\s*@\s*([\w./-]+?\.py):([\w.]+)")


def index_defs():
    defs = {}
    for p in list((REPO / "src").rglob("*.c")) + list((REPO / "lib").rglob("*.c")):
        try:
            txt = p.read_text(errors="ignore")
        except OSError:
            continue
        for m in DEF.finditer(txt):
            defs.setdefault(m.group("name"), (m.group("ret").strip(), p, txt))
    return defs


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply-static", action="store_true")
    ap.add_argument("--apply-gap", action="store_true")
    args = ap.parse_args()

    rows = json.loads(ROWS.read_text())
    missing = [r for r in rows if r["class"] == "MISSING"]
    defs = index_defs()

    statics, ghosts, others = [], [], []
    for r in missing:
        sym = r["symbol"]
        if sym not in defs:
            ghosts.append(r)
        else:
            ret, path, txt = defs[sym]
            if re.search(r"\bstatic\b", ret):
                statics.append((r, path, txt))
            else:
                others.append((r, path))

    print("MISSING %d: static %d, no definition %d, other %d"
          % (len(missing), len(statics), len(ghosts), len(others)))

    fixed_static = 0
    if args.apply_static:
        by_file = defaultdict(list)
        for r, path, txt in statics:
            by_file[path].append(r)
        for path, items in by_file.items():
            txt = path.read_text(errors="ignore")
            pops = {(m.group(2), m.group(3)) for m in POP.finditer(txt)}
            original = txt
            for r in items:
                g = json.loads((GATES / r["gate"] / "gate.json").read_text())
                sym = r["symbol"]
                fn = (g.get("py_fn") or "").split(".")[-1]
                if (g.get("py_module"), g.get("py_fn")) not in pops:
                    continue
                # The C symbol is the Python function name, optionally module-
                # prefixed ("classify_write_denial" for "_classify_write_denial").
                # Comparing only the last underscore segment rejected that case
                # (denial != _classify_write_denial) and left 99 of 100 unfixed.
                bare = fn.lstrip("_")
                if not (sym == bare or sym.endswith(bare) or bare.endswith(sym)):
                    continue
                pat = re.compile(r"^(\s*)static(\s+[^\n;{}]*?\b%s\s*\()" % re.escape(sym), re.M)
                new, n = pat.subn(r"\1\2", txt)
                if n:
                    txt = new
                    fixed_static += 1
                    print("de-static %-52s %s" % (sym, path.relative_to(REPO)))
            if txt != original:
                path.write_text(txt, encoding="utf-8")

    fixed_gap = 0
    if args.apply_gap:
        import fix_all_gates as _fag
        for r in ghosts:
            gd = GATES / r["gate"]
            g = json.loads((gd / "gate.json").read_text())
            (gd / "harness.c").write_text(
                _fag.generate_gap_sentinel(g, g.get("py_fn") or r["symbol"]))
            fixed_gap += 1

    for r, path in others[:10]:
        print("  other: %-52s %s defines it, not exported" %
              (r["symbol"], path.relative_to(REPO)))
    print("\n%s" % ("de-static applied to %d symbol(s)" % fixed_static
                    if args.apply_static else "dry run: no static changes"))
    print("%s" % ("sentinel written for %d gate(s)" % fixed_gap
                  if args.apply_gap else "dry run: no harness changes"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
