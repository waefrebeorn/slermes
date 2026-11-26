#!/usr/bin/env python3
"""Separate the watercup gates that prove something from the ones that cannot fail.

tests/watercup/gates holds one harness per Python function to port. Two kinds exist:

  sentinel  -- the harness contains a gap_sentinel() that returns a constant; it is a placeholder
               that says "no C port exists". It cannot fail, and it does not fail when the port
               lands either, so a rising "gates passing" count made of these is not progress.
  real      -- the harness calls into the product.

This prints both counts, and checks each real gate against the symbols the built binary actually
exports: a real gate whose subject is missing from the binary is a gate that cannot be running
against the product.

    python scripts/classify_watercup_gates.py [--json out.json] [--list real]
"""
from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
GATES = REPO / "tests" / "watercup" / "gates"
NM = Path(r"C:\winlibs\mingw64\bin\nm.exe")
EXE = REPO / "slermes.exe"


def binary_symbols() -> set[str]:
    if not EXE.exists():
        return set()
    out = subprocess.run([str(NM), str(EXE)], capture_output=True, text=True, errors="replace").stdout
    return set(re.findall(r"^[0-9a-fA-F]+\s+[TtWw]\s+(\S+)$", out, re.M))


def main() -> int:
    want_json = None
    if "--json" in sys.argv:
        want_json = Path(sys.argv[sys.argv.index("--json") + 1])
    listing = None
    if "--list" in sys.argv:
        listing = sys.argv[sys.argv.index("--list") + 1]

    harnesses = sorted(GATES.glob("*/harness.c"))
    sentinels, real = [], []
    for h in harnesses:
        text = h.read_text(encoding="utf-8", errors="replace")
        if "GAP sentinel" in text or re.search(r"gap_sentinel\s*\(", text):
            sentinels.append(h)
        else:
            real.append(h)

    print(f"harnesses          : {len(harnesses)}")
    print(f"  GAP sentinels    : {len(sentinels)}  (return a constant; cannot fail)")

    # A real gate names the C function it exercises (usually in a WCUP_BEGIN("...") or an extern).
    syms = binary_symbols()
    if syms:
        missing = []
        for h in real:
            text = h.read_text(encoding="utf-8", errors="replace")
            names = set(re.findall(r'\b([a-z][a-z0-9_]{6,})\s*\(', text))
            externs = set(re.findall(r"extern\s+[A-Za-z_][\w \t\*]*?[\s\*]([a-z][\w]*)\s*\(", text))
            subject = externs or set()
            if subject and not (subject & syms):
                missing.append((str(h.relative_to(REPO)), sorted(subject)[:3]))
        printed = len(real) - len(missing)
        print(f"  real gates       : {len(real)}")
        print(f"    linked         : {printed}  (subject exported by slermes.exe)")
        print(f"    unlinked       : {len(missing)}  (names an extern the binary does not export --"
              f" these build-fail, so they are sentinels in disguise)")
        for path, names in missing[:10]:
            print(f"    {path:72} {', '.join(names)}")
        if want_json:
            want_json.write_text(json.dumps({
                "sentinels": [str(p.relative_to(REPO)) for p in sentinels],
                "real": [str(p.relative_to(REPO)) for p in real],
                "real_missing_subject": missing,
            }, indent=1), encoding="utf-8")
            print(f"wrote {want_json}")
    else:
        print("  (slermes.exe not built; skipping the symbol cross-check)")

    if listing == "sentinel":
        for p in sentinels[:20]:
            print("  ", p.relative_to(REPO))
    return 0


if __name__ == "__main__":
    sys.exit(main())
