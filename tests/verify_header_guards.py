#!/usr/bin/env python3
"""Gate: no two headers may share an include-guard macro.

An include guard only works if it is unique. When two headers use the same
macro, including both in one translation unit silently discards the second --
no error, no warning, just missing declarations. That is how
include/doctor.h and include/hermes_doctor.h both came to carry
HERMES_DOCTOR_H: the collision was latent (no TU included both), so nothing
ever caught it.

Also reports headers with no guard at all, which are unprotected.

Usage: python tests/verify_header_guards.py [--root REPO]
Exit 0 = PASS, 1 = FAIL.
"""
import collections
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
GUARD = re.compile(r"#\s*ifndef\s+([A-Za-z_]\w*)")


def main() -> int:
    by_guard = collections.defaultdict(list)
    unguarded = []
    total = 0
    for h in sorted((REPO / "include").rglob("*.h")):
        total += 1
        text = h.read_text(encoding="utf-8", errors="replace")
        m = GUARD.search(text)
        if not m:
            if "#pragma once" not in text:
                unguarded.append(h.relative_to(REPO).as_posix())
            continue
        by_guard[m.group(1)].append(h.relative_to(REPO).as_posix())

    dupes = {g: v for g, v in by_guard.items() if len(v) > 1}
    print(f"header-guard gate: {total} headers under include/, "
          f"{len(by_guard)} distinct guards")
    print(f"  duplicate guards : {len(dupes)}")
    print(f"  unguarded headers: {len(unguarded)}")

    for g, v in sorted(dupes.items()):
        print(f"    DUPLICATE {g}: {', '.join(v)}")
    for u in unguarded[:10]:
        print(f"    UNGUARDED {u}")

    ok = not dupes
    print("header-guard gate:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
