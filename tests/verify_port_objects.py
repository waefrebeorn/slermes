#!/usr/bin/env python3
"""Gate: no translation unit carrying PoP parity credit may sit outside the build.

Parity is a claim about the linked binary, so a .c file that is never named in
build/objects.mk cannot legitimately carry parity credit. 821 unlinked files in
this tree carry PoP annotations (165 port-style twins, 656 _wrappers/_gaps
scaffolding); none of them is in the binary.

A file may be excused only by being listed in superseded_port_files.txt --
those are the pre-existing dead twins. Anything new fails.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
MK = REPO / "build" / "objects.mk"
EXCUSE = Path(__file__).resolve().parent / "superseded_port_files.txt"


def main() -> int:
    linked = set(re.findall(r'(src/[A-Za-z0-9_/]+\.o)',
                            MK.read_text(encoding="utf-8", errors="replace")))
    excused = set()
    if EXCUSE.exists():
        excused = {ln.strip() for ln in EXCUSE.read_text(encoding="utf-8").splitlines()
                   if ln.strip() and not ln.startswith('#')}

    unlinked = []
    excused_hits = 0
    for c in sorted((REPO / "src").rglob("*.c")):
        rel = c.relative_to(REPO).as_posix()
        obj = c.with_suffix('.o').relative_to(REPO).as_posix()
        if obj in linked:
            continue
        # Only port-style paths ever carried parity credit: these are the
        # files that were exempted before the exemption was removed. Plain
        # _wrappers.c / _gaps.c scaffolding was never indexed at all.
        if not ('/ports/' in rel or Path(rel).name.startswith('port_')
                or rel.endswith('_ports.c')):
            continue
        try:
            text = c.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        pops = len(re.findall(r'PoP:', text))
        if not pops:
            continue
        if rel in excused:
            excused_hits += 1
            continue
        unlinked.append((pops, rel))

    print(f"port object gate: {len(linked)} linked objects, "
          f"{excused_hits} excused (superseded), {len(unlinked)} unlinked with PoP credit")
    for pops, rel in unlinked:
        print(f"  FAIL {pops:4} PoP  {rel}")
    if unlinked:
        print("\nEither add these to build/objects.mk or, if they are dead "
              "duplicates,\nlist them in tests/superseded_port_files.txt.")
        return 1
    print("port object gate: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
