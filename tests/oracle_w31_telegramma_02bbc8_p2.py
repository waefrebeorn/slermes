#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_markdown.py
(gate w31_telegramma_02bbc8_p2): the pure text primitives the MarkdownV2 path
is built from.

``_escape_mdv2``, ``_url_end``, ``link_target_renders`` and
``_crosses_quote_edge`` are called on the fixture's exact strings (offset
predicates on the fixture's exact byte offsets); no transport, no plugin.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w31_telegramma_02bbc8_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import os  # noqa: E402
os.environ.setdefault("USERPROFILE", str(QUARRY.parent))
os.environ.setdefault("HOME", str(QUARRY.parent))

from plugins.platforms.telegram import telegram_markdown as live  # noqa: E402

WANTED = {"escape_mdv2", "url_end", "link_target_renders", "crosses_quote_edge"}


def expected(r):
    f = r.get("func")
    if f == "escape_mdv2":
        return live._escape_mdv2(r.get("text"))
    if f == "url_end":
        return live._url_end(r.get("url"))
    if f == "link_target_renders":
        return live.link_target_renders(r.get("target"))
    if f == "crosses_quote_edge":
        return live._crosses_quote_edge(r.get("text"), int(r.get("start")), int(r.get("end")))
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
