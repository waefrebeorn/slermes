#!/usr/bin/env python3
"""Fix oracles that resolve the dev repo one level too high.

`here = os.path.dirname(os.path.abspath(__file__))` is <slermes>/tests, so
dirname^3(here) is C:/Users -- not the Hermes checkout. The __file__-based
form (dirname^3 of the file path) is correct and is left alone.

The robust source of truth is DEV_ROOT, which _oracle_boot.py already
computes; this makes the boot export it as SLERMES_DEV_ROOT and repoints the
affected oracles at that, so the layout is not re-derived by hand.

Run after the suite is idle: it edits oracle files in place.
"""
import re
import sys
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
BAD = "os.path.dirname(os.path.dirname(os.path.dirname(here)))"
GOOD = 'os.environ.get("SLERMES_DEV_ROOT") or os.path.dirname(os.path.dirname(here))'


def main() -> int:
    # 1. the boot exports DEV_ROOT
    boot = REPO / "tests/oracle/_oracle_boot.py"
    text = boot.read_text(encoding="utf-8")
    if "SLERMES_DEV_ROOT" not in text:
        anchor = 'sys.path.insert(0, DEV_ROOT)'
        assert anchor in text, "DEV_ROOT anchor not found"
        text = text.replace(
            anchor,
            'os.environ.setdefault("SLERMES_DEV_ROOT", DEV_ROOT)\n' + anchor)
        boot.write_text(text, encoding="utf-8")
        print("boot: exports SLERMES_DEV_ROOT")
    else:
        print("boot: already exports SLERMES_DEV_ROOT")

    # 2. repoint the here-based oracles
    fixed = 0
    for f in sorted((REPO / "tests").glob("sta_oracle_*.py")):
        t = f.read_text(encoding="utf-8")
        if BAD not in t:
            continue
        f.write_text(t.replace(BAD, GOOD), encoding="utf-8")
        fixed += 1
        print("fixed", f.name)
    print("oracles repointed:", fixed)
    return 0


if __name__ == "__main__":
    sys.exit(main())
