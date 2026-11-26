#!/usr/bin/env python3
"""Catch objects that are newer than their source but built from older source.

make decides by mtime. Every `git checkout -- src/x.c`, every copy that restores an older mtime, and
every patch that lands while the object is being written leaves an object that *looks* current and
contains old code. That is not hypothetical: src/tools/kanban_adapter.o was six hours newer than
src/tools/kanban_adapter.c and did not contain kanban_create_task at all, so `kanban_create` answered
from a placeholder in src/infrastructure_stubs.c and every tool that read the board returned nonsense
while `make` reported the build up to date.

    python scripts/stale_object_guard.py                # report
    python scripts/stale_object_guard.py --touch        # bump mtime on changed sources so make rebuilds

A stamp of source hashes is kept in build/.src_hashes.json. Sources whose hash changed since the stamp
are stale even if their mtime says otherwise; --touch makes make notice them.
"""
from __future__ import annotations

import hashlib
import json
import os
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
STAMP = REPO / "build" / ".src_hashes.json"
ROOTS = ("src", "lib")


def sources() -> list[Path]:
    out = []
    for root in ROOTS:
        for dirpath, _dirs, names in os.walk(REPO / root):
            if ".git" in dirpath:
                continue
            for n in names:
                if n.endswith(".c"):
                    out.append(Path(dirpath) / n)
    return out


def digest(p: Path) -> str:
    h = hashlib.sha256()
    with p.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    touch = "--touch" in sys.argv
    old = json.loads(STAMP.read_text()) if STAMP.exists() else {}
    new: dict[str, str] = {}
    changed, new_files = [], []
    for p in sources():
        key = str(p.relative_to(REPO)).replace("\\", "/")
        d = digest(p)
        new[key] = d
        if key not in old:
            new_files.append(key)
        elif old[key] != d:
            changed.append(key)

    obj_missing = [k for k in new if not (REPO / (k[:-2] + ".o")).exists()]

    print(f"sources: {len(new)}  tracked before: {len(old)}")
    print(f"changed since the last stamp: {len(changed)}")
    for k in changed[:10]:
        print(f"  {k}")
    if len(changed) > 10:
        print(f"  ... {len(changed) - 10} more")
    print(f"not in the stamp (first run or new): {len(new_files)}")
    print(f"without an object: {len(obj_missing)}")

    if touch:
        for k in changed:
            os.utime(REPO / k, None)
        print(f"touched {len(changed)} source(s) so make rebuilds them")

    STAMP.parent.mkdir(parents=True, exist_ok=True)
    STAMP.write_text(json.dumps(new, indent=0, sort_keys=True))
    print(f"stamp written: {STAMP.relative_to(REPO)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
