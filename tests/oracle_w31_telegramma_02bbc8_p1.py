#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_markdown.py
(gate w31_telegramma_02bbc8_p1): the MarkdownV2 send path.

``to_markdown_v2`` is pure text in, pure text out.  The module is imported as a
package module (its step-0 imports reach two sibling modules), no transport is
touched, and the fixture's exact string is what is compared.

Declared seam: the two cross-module passes step 0 delegates to --
``normalize_latex_macros`` (telegram_latex.py) and ``convert_table_to_bullets``
(gateway/platforms/helpers.py), each with its own C home and gate -- are identity
for this fixture corpus, which carries no ``\\macro`` name and no pipe table.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w31_telegramma_02bbc8_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import os  # noqa: E402
os.environ.setdefault("USERPROFILE", str(QUARRY.parent))
os.environ.setdefault("HOME", str(QUARRY.parent))

from plugins.platforms.telegram import telegram_markdown as live  # noqa: E402

WANTED = {"to_markdown_v2"}


def expected(r):
    f = r.get("func")
    if f == "to_markdown_v2":
        content = r.get("content")
        if not isinstance(content, str):
            raise TypeError("content must be a string")
        return live.to_markdown_v2(content)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
