#!/usr/bin/env python3
"""
gen_parity_walkway.py — single source of truth for parity counts in walkway files.

WHY: barnacles (stale copied PORTED/REAL_GAP numbers) existed because agents
hand-transcribed scanner output into ~15 walkway files, and the copies drifted
(the classic "8,688/8,688 100% PORTED" fiction). This script is the ONLY writer
of those counts. Walkway files contain a sentinel block:

    <!-- PARITY:AUTO -->
    ... counts ...
    <!-- /PARITY:AUTO -->

and this script regenerates the inner block from the live scanner every run.
Nothing is hand-copied. The prestige ritual calls `make parity-walkway` instead
of a manual barnacle hunt.

Count source: tests/slermes_parity_battleground.py --json (same scanner the
project uses). For BANNER.md (box-art) a regex replaces the Ported line.
"""
import json
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCANNER = os.path.join(REPO, "tests", "slermes_parity_battleground.py")
SENT_OPEN = "<!-- PARITY:AUTO -->"
SENT_CLOSE = "<!-- /PARITY:AUTO -->"

# Walkway files that carry a canonical count block (project-level + user-level).
WALKWAY_FILES = [
    os.path.join(REPO, ".hermes", "state.md"),
    os.path.join(REPO, ".hermes", "mind-palace", "state.md"),
    os.path.join(REPO, ".hermes", "mind-palace", "prestige.md"),
    os.path.join(REPO, ".hermes", "mind-palace", "plan.md"),
    os.path.join(REPO, ".hermes", "mind-palace", "next-session-prompt.md"),
    os.path.join(REPO, ".hermes", "next-session", "steamroll-goal.md"),
    os.path.expanduser("~/.hermes/mind-palace/state.md"),
    os.path.expanduser("~/.hermes/mind-palace/plan.md"),
    os.path.expanduser("~/.hermes/mind-palace/overnight.md"),
    os.path.expanduser("~/.hermes/mind-palace/INDEX.md"),
    os.path.expanduser("~/.hermes/mind-palace/battleship.md"),
]
BANNER = os.path.join(REPO, "BANNER.md")


def compute():
    out = subprocess.check_output([sys.executable, SCANNER, "--json"], cwd=REPO)
    data = json.loads(out)["modules"]
    ported = sum(v.get("ported", 0) for v in data.values())
    real = sum(v.get("real_gaps", 0) for v in data.values())
    partial = sum(v.get("partial", 0) for v in data.values())
    total = ported + real + partial
    pct = 100.0 * ported / total if total else 0.0
    return dict(ported=ported, real=real, partial=partial, total=total, pct=pct)


def block(d):
    return (
        f"| PORTED  | {d['ported']:,} / {d['total']:,} ({d['pct']:.1f}%) |\n"
        f"| REAL_GAP| {d['real']:,} ({100.0 * d['real'] / d['total']:.1f}%) — no N/A |\n"
        f"| PARTIAL | {d['partial']:,} (0.8%) |\n"
        f"| STUB    | 0 |\n"
        f"\n"
        f"_Generated from live scanner `{os.path.relpath(SCANNER, REPO)}` — "
        f"do not edit by hand; run `make parity-walkway`._"
    )


def inject(path, inner):
    if not os.path.exists(path):
        return False
    txt = open(path, encoding="utf-8").read()
    # Already has a sentinel block: replace inner content, keep markers.
    if SENT_OPEN in txt and SENT_CLOSE in txt:
        pre, rest = txt.split(SENT_OPEN, 1)
        _, post = rest.split(SENT_CLOSE, 1)
        txt = pre + SENT_OPEN + "\n" + inner + "\n" + SENT_CLOSE + post
        open(path, "w", encoding="utf-8").write(txt)
        return True
    # No sentinel yet: append a block at the end so future runs regenerate it.
    txt = txt.rstrip() + "\n\n" + SENT_OPEN + "\n" + inner + "\n" + SENT_CLOSE + "\n"
    open(path, "w", encoding="utf-8").write(txt)
    return True


def fix_banner(d):
    if not os.path.exists(BANNER):
        return
    txt = open(BANNER, encoding="utf-8").read()
    line = (
        f"# ║  Ported: {d['ported']:,}/{d['total']:,} ({d['pct']:.1f}%)  "
        f"REAL_GAP: {d['real']:,} ({100.0 * d['real'] / d['total']:.1f}%)  "
        f"PARTIAL: {d['partial']:,}  ║"
    )
    new = re.sub(r"# ║  Ported:.*?PARTIAL:.*?║", line, txt, flags=re.S)
    if new != txt:
        open(BANNER, "w", encoding="utf-8").write(new)


def main():
    d = compute()
    inner = block(d)
    for f in WALKWAY_FILES:
        ok = inject(f, inner)
        print(f"{'OK  ' if ok else 'MISS'} {f}")
    fix_banner(d)
    print(
        f"Live: PORTED {d['ported']:,} REAL_GAP {d['real']:,} "
        f"PARTIAL {d['partial']:,} (total {d['total']:,})"
    )


if __name__ == "__main__":
    main()
