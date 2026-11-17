#!/usr/bin/env python3
"""Roll util_rename_replace out to every atomic-write call site.

Blanket by design: util_rename_replace() behaves exactly like rename() when the
destination does not exist, so a plain `rename(a, b)` is always safe to swap.
Only the overwrite case changes, and that is the bug.

Idempotent; adds the include when missing.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
CALL = re.compile(r"\brename\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*,\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)")
INC = '#include "port_utils_ports.h"'


def fix(path: Path) -> str:
    raw = path.read_bytes()
    nl = "\r\n" if b"\r\n" in raw else "\n"
    text = raw.decode("utf-8", errors="replace").replace("\r\n", "\n")
    new, n = CALL.subn(lambda m: "util_rename_replace(%s, %s)" % (m.group(1), m.group(2)),
                       text)
    if n == 0:
        return "no calls"
    if INC not in new:
        lines = new.split("\n")
        last = 0
        for i, ln in enumerate(lines[:80]):
            if ln.startswith("#include "):
                last = i
        lines.insert(last + 1, INC)
        new = "\n".join(lines)
    path.write_bytes(new.replace("\n", nl).encode("utf-8"))
    return "rewrote %d" % n


if __name__ == "__main__":
    pats = sys.argv[1:] or ["src"]
    files = []
    for p in pats:
        q = REPO / p
        files.extend(q.rglob("*.c") if q.is_dir() else [q])
    total = 0
    for f in sorted(set(files)):
        r = fix(f)
        if r.startswith("rewrote"):
            print(f.relative_to(REPO), r)
            total += 1
    print("files changed:", total)
