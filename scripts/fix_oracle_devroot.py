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

GET = 'os.environ.get("SLERMES_DEV_ROOT", '
IMPORT_OS = re.compile(r"^\s*import\s+os(\s|,|\.)", re.M)
# Longer literals that embed the old root as a prefix of a full path.
PREFIX_FORMS = [
    (re.compile(r'"/home/wubu/hermes-agent-dev/([^"]*)"'),
     r'os.environ.get("SLERMES_DEV_ROOT", "/home/wubu/hermes-agent-dev") + "/\1"'),
    (re.compile(r'"/home/wubu/\.hermes/hermes-agent([^"]*)"'),
     r'os.environ.get("SLERMES_DEV_ROOT", "/home/wubu/hermes-agent") + "\1"'),
]
# The bare root, but only where it is not already the fallback argument. The
# literal appears inside its own replacement, so a naive second run wraps
# again; the fixed-width lookbehind keeps this idempotent.
BARE = re.compile(r'(?<!' + re.escape(GET) + r')"/home/wubu/hermes-agent-dev"')
BARE_NEW = GET + '"/home/wubu/hermes-agent-dev")'


def add_os_import(text):
    if "os.environ" not in text or IMPORT_OS.search(text):
        return text
    lines = text.split("\n")
    # Insert after the last import STATEMENT, tracking parenthesis depth so a
    # multi-line `from x import (a, b, c)` is not broken by inserting into its
    # argument list.
    last_end = 0
    for i, l in enumerate(lines[:120]):
        if not re.match(r"^(import|from)\s", l):
            continue
        depth = 0
        j = i
        while j < len(lines):
            depth += lines[j].count("(") - lines[j].count(")")
            if depth <= 0:
                break
            j += 1
        last_end = j
    lines.insert(last_end + 1, "import os")
    return "\n".join(lines)


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
    # 3. the literal forms. 56 oracles hard-code the OLD checkout location, so
    # they raise FileNotFoundError and report ORACLE-BROKEN -- an absent
    # measurement that hides real parity signal behind an infrastructure error.
    changed = 0
    occ = 0
    for f in sorted((REPO / "tests").glob("sta_oracle_*.py")):
        t = f.read_text(encoding="utf-8")
        orig = t
        for pat, repl in PREFIX_FORMS:
            t, n = pat.subn(repl, t)
            occ += n
        t, n = BARE.subn(BARE_NEW, t)
        occ += n
        t = add_os_import(t)
        if t == orig:
            continue
        f.write_text(t, encoding="utf-8")
        changed += 1
        print("literal repointed", f.name)
    print("oracles repointed from literals:", changed, "replacements:", occ)
    return 0


if __name__ == "__main__":
    sys.exit(main())
