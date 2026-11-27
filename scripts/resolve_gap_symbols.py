#!/usr/bin/env python3
"""resolve_gap_symbols.py — extend the PoP symbol map with unique name matches.

`convert_gap_gate.py` refuses a gate with "no unambiguous PoP symbol": the gate's
annotation does not name a C symbol. Measured on 120 gates that screened as
shape-convertible, that refusal is why ALL of them stayed GAP. Two different causes
hide behind it:

  (a) the function genuinely has no C implementation (the real gap), and
  (b) it is implemented under another name and the map cannot see it.

This script only touches (b), and only when the evidence is unambiguous: the
Python function name, with leading underscores stripped, must match exactly ONE
exported port symbol (equality or suffix after a prefix like `rt_`/`cl2_`). A
unique match is then offered to the converter -- which still has to reproduce the
Python answers cup for cup. A wrong match cannot pass silently: the gate goes red
with a mismatch, which is the finding we want.

Usage: python scripts/resolve_gap_symbols.py [--out .testbin/pop_symbol_map_aug.json]
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "scripts"))
GATES = REPO / "tests" / "watercup" / "gates"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--map", default=str(REPO / ".testbin" / "pop_symbol_map.json"))
    ap.add_argument("--out", default=str(REPO / ".testbin" / "pop_symbol_map_aug.json"))
    ap.add_argument("--gates", nargs="*", help="restrict to these gates")
    args = ap.parse_args()

    from _gate_audit_common import port_symbols

    port = port_symbols()
    by_suffix: dict[str, list[str]] = defaultdict(list)
    for sym in port:
        if sym.startswith("__"):
            continue
        by_suffix[sym.lower()].append(sym)

    # The converter reads the C signature out of the map's `src`, so a name match
    # is useless without the file that defines it.
    def defining_src(sym: str) -> str | None:
        pat = re.compile(r"^[A-Za-z_][^;{}()]*\b%s\s*\(" % re.escape(sym), re.M)
        for root in ("src", "lib"):
            for c in (REPO / root).rglob("*.c"):
                try:
                    if pat.search(c.read_text(errors="ignore")):
                        return str(c.relative_to(REPO))
                except OSError:
                    continue
        return None

    base = json.loads(Path(args.map).read_text()) if Path(args.map).exists() else {}
    aug = dict(base)
    added = ambiguous = unimpl = 0
    names = [n.strip() for n in (args.gates or [])] or [p.name for p in GATES.iterdir() if (p / "gate.json").is_file()]
    for gate in names:
        if gate in aug:
            continue
        try:
            g = json.loads((GATES / gate / "gate.json").read_text())
        except OSError:
            continue
        fn = (g.get("py_fn") or "").strip().split(".")[-1]
        if not fn:
            unimpl += 1
            continue
        bare = fn.lstrip("_").lower()
        if not bare:
            unimpl += 1
            continue
        # candidates: exact, or <prefix>_<bare>
        exact = [s for s in port if s.lower() == bare]
        suff = [s for s in port if s.lower().endswith("_" + bare)]
        cands = sorted(set(exact) | set(suff))
        if len(cands) == 1:
            src = defining_src(cands[0])
            if src is None:
                unimpl += 1
                continue
            aug[gate] = {"symbol": cands[0], "src": src}
            added += 1
        elif len(cands) > 1:
            ambiguous += 1
        else:
            unimpl += 1

    Path(args.out).write_text(json.dumps(aug, indent=1))
    print("%d gates in map, +%d unique name matches, %d ambiguous, %d no C symbol"
          % (len(base), added, ambiguous, unimpl))
    print("wrote", args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
