#!/usr/bin/env python3
"""Regenerate tests/superseded_port_files.txt and report the dead-port inventory.

A file lands on the superseded list when it is port-style (a path the old
scanner exemption used to credit: `/ports/` in the path, `port_*` basename, or
`*_ports.c` suffix), carries at least one PoP annotation, and is NOT named in
build/objects.mk.

Those files are dead duplicates: the same Python features are credited again in
a linked translation unit. They are kept rather than deleted because deleting
165 files is not a decision a script should make on its own -- but they must
never regain the ability to move the parity number, which is why the scanner's
`/ports/` exemption is gone and tests/verify_port_objects.py gates the list.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
MK = REPO / "build" / "objects.mk"
OUT = REPO / "tests" / "superseded_port_files.txt"

HEADER = [
    "# Unlinked port-style translation units, kept out of the binary on purpose.",
    "# The scanner only credits PoP from objects.mk members, so these contribute",
    "# nothing to parity either way; many of them are the ONLY C for their Python",
    "# feature and that feature is therefore genuinely missing (REAL_GAP).",
    "# Regenerate with scripts/audit_port_objects.py. Editable by hand.",
]


def is_port_style(rel: str) -> bool:
    return '/ports/' in rel or Path(rel).name.startswith('port_') \
        or rel.endswith('_ports.c')


def main() -> int:
    linked = set(re.findall(r'(src/[A-Za-z0-9_/]+\.o)',
                            MK.read_text(encoding="utf-8", errors="replace")))
    rows = []
    for c in sorted((REPO / "src").rglob("*.c")):
        rel = c.relative_to(REPO).as_posix()
        obj = c.with_suffix('.o').relative_to(REPO).as_posix()
        if obj in linked or not is_port_style(rel):
            continue
        try:
            text = c.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        pops = len(re.findall(r'PoP:', text))
        if pops:
            rows.append((rel, pops))

    OUT.write_text("\n".join(HEADER + [r for r, _ in rows]) + "\n",
                   encoding="utf-8", newline="\n")
    print(f"wrote {OUT.relative_to(REPO)}: {len(rows)} superseded files, "
          f"{sum(n for _, n in rows)} PoP annotations")
    print(f"linked objects: {len(linked)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
