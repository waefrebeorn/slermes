#!/usr/bin/env python3
"""deput_copy_harness.py — make a COPY gate call the port instead of its own copy.

A COPY gate is a harness that defines the traced function itself (usually `static`).
Its cups measure the agent's transcription: it agrees with Python because both were
written from the same source text, and it says nothing about src/. 14 of the 41 have
a symbol the port exports under the same name.

The transform is deliberately blunt and evidence-gated:

  1. cut the local definition (balanced braces) out of harness.c,
  2. add an `extern` declaration copied from the PORT's own signature,
  3. rebuild, run, and keep the result only when the cups equal the live twin's AND
     discriminate. A disagreement here is the interesting case: it means the
     transcription and the port differ, and the port is what ships.

Anything that fails is restored byte-for-byte.

Usage: python scripts/deput_copy_harness.py [gate ...] [--from-provenance] [--limit N]
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
GATES = REPO / "tests" / "watercup" / "gates"
ROWS = REPO / "tests" / "watercup" / "build" / "gate_provenance.json"
sys.path.insert(0, str(REPO / "tests" / "watercup"))
sys.path.insert(0, str(REPO / "scripts"))


def find_definition(src: str, sym: str):
    """(start, end, signature) of the definition of sym in this harness text."""
    m = re.search(r"^[^\n;{}]*\b%s\s*\([^;{}]*\)\s*\{" % re.escape(sym), src, re.M)
    if not m:
        return None
    start = m.start()
    i = src.index("{", m.end() - 1)
    depth = 0
    while i < len(src):
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
            if depth == 0:
                break
        i += 1
    sig = src[start:src.index("{", start)].strip()
    return start, i + 1, sig


def port_signature(sym: str):
    from convert_gap_gate import c_signature
    from _gate_audit_common import REPO as _R
    for p in list((_R / "src").rglob("*.c")) + list((_R / "lib").rglob("*.c")):
        try:
            txt = p.read_text(errors="ignore")
        except OSError:
            continue
        if sym not in txt:
            continue
        got = c_signature(p, sym)
        if got:
            ret, arity, buf = got
            m = re.search(r"^[^\n;{}]*\b%s\s*\(([^)]*)\)\s*\{" % re.escape(sym), txt, re.M)
            if m:
                return ret, m.group(1).strip(), p
    return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("gates", nargs="*")
    ap.add_argument("--from-provenance", action="store_true")
    ap.add_argument("--limit", type=int, default=0)
    args = ap.parse_args()

    import wc_run
    from _gate_audit_common import port_symbols
    syms = port_symbols()

    names = list(args.gates)
    if args.from_provenance:
        rows = json.loads(ROWS.read_text())
        names += [r["gate"] for r in rows
                  if r["class"] == "COPY" and r["symbol"] in syms]
    if args.limit:
        names = names[:args.limit]

    done, kept = [], []
    for name in names:
        gd = GATES / name
        g = json.loads((gd / "gate.json").read_text())
        src_path = gd / "harness.c"
        src = src_path.read_text()
        rows = {r["gate"]: r for r in json.loads(ROWS.read_text())}
        sym = (rows.get(name) or {}).get("symbol") or (g.get("c_fn") or "").split(".")[-1]
        if "gap_sentinel" in src:
            kept.append((name, "still a GAP sentinel"))
            continue
        found = find_definition(src, sym)
        if not found:
            kept.append((name, "%s is not defined in this harness" % sym))
            continue
        start, end, _local_sig = found
        psig = port_signature(sym)
        if not psig:
            kept.append((name, "cannot read the port's %s() signature" % sym))
            continue
        ret, params, pfile = psig
        decl = "extern %s %s(%s);" % (ret, sym, params)
        patched = src[:start] + decl + src[end:]
        # Put the declaration after the last #include so the type names exist.
        src_path.write_text(patched)
        try:
            out = wc_run.build_harness(gd, g, force=True)
            cups = wc_run.run_harness(out, g)
            twin = wc_run.load_twin(g)
            lbl = wc_run._harness_cup_label(gd, g) or g["c_fn"]
            inst = getattr(twin, "_t", twin)
            inst._cup_label = lbl
            py = list(wc_run._run_twin_inputs(twin, g["inputs"]))
            ok = cups == py and (len(cups) < 2 or len(set(cups)) > 1)
        except Exception as e:
            ok, cups, py = False, [], []
            kept.append((name, "%s: %s" % (type(e).__name__, e)))
            src_path.write_text(src)
            continue
        if ok:
            done.append((name, sym))
            print("DEPUTISED %-52s %s (port: %s)" % (name, sym, pfile.relative_to(REPO)))
        else:
            src_path.write_text(src)
            why = ("cups differ: the transcription and the port disagree"
                   if cups and cups != py else "constant answer" if cups else "no cups")
            kept.append((name, "%s: %s" % (sym, why)))

    for name, why in kept:
        print("kept COPY %-52s %s" % (name, why[:100]))
    print("\n%d deputised, %d kept, of %d tried" % (len(done), len(kept), len(names)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
