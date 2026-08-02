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
project uses). BANNER.md is a workflow doc with no counts (legacy box-art was
retired); the sentinel blocks in the walkway files are the only count surface.
"""
import json
import os
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCANNER = os.path.join(REPO, "tests", "slermes_parity_battleground.py")
# Fail-closed gate: the Python ground-truth must be checked out, or we refuse
# to emit. This is the systemic cure for "silent 0-file scan -> confident lie".
TRUTH = os.path.join(REPO, "scripts", "parity_truth.py")
SENT_OPEN = "<!-- PARITY:AUTO -->"
SENT_CLOSE = "<!-- /PARITY:AUTO -->"

# Walkway files that carry a canonical count block (project-level + user-level).
WALKWAY_FILES = [
    os.path.join(REPO, ".hermes", "state.md"),
    os.path.join(REPO, ".hermes", "battleship.md"),
    os.path.join(REPO, ".hermes", "roadmap.md"),
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
    # Next-session prompts under ~/.hermes/walkway/ (the files agents actually
    # read). Globbed so new versions are covered without editing this list.
] + sorted(
    os.path.expanduser(os.path.join("~/.hermes/walkway", f))
    for f in os.listdir(os.path.expanduser("~/.hermes/walkway"))
    if f.startswith("v") and f.endswith(".md")
)


def compute():
    # FAIL-CLOSED: delegate to parity_truth.py, which refuses to emit a number
    # unless the Python ground-truth is actually checked out and the scanner
    # actually consumed it. If the gate fails, we abort here (non-zero) so no
    # walkway file is touched with a stale/phantom count.
    r = subprocess.run([sys.executable, TRUTH], cwd=REPO)
    if r.returncode != 0:
        sys.exit(r.returncode)
    with open(os.path.join(REPO, "live_parity_scan.json"), encoding="utf-8") as f:
        doc = json.load(f)
    tot = doc["totals"]
    return dict(ported=tot["ported"], real=tot["real_gaps"],
                partial=tot["partial"], total=tot["total"],
                pct=tot["coverage_pct"],
                stamp=doc.get("_generated_at", "unknown-time"))


def block(d):
    partial_pct = 100.0 * d['partial'] / d['total'] if d['total'] else 0.0
    return (
        f"| PORTED  | {d['ported']:,} / {d['total']:,} ({d['pct']:.1f}%) |\n"
        f"| REAL_GAP| {d['real']:,} ({100.0 * d['real'] / d['total']:.1f}%) — no N/A |\n"
        f"| PARTIAL | {d['partial']:,} ({partial_pct:.1f}%) |\n"
        f"\n"
        f"**Phase (v667):** PARITY project — the C11 binary is the deliverable: "
        f"faithful, oracle-verified, usable standalone across operating systems. "
        f"Closing REAL_GAPs is the path; the AGI-OS integration consumes the "
        f"binary, not the Python tree.\n"
        f"\n"
        f"_Generated {d['stamp']} from live scanner "
        f"`{os.path.relpath(SCANNER, REPO)}` — "
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


def main():
    d = compute()
    inner = block(d)
    for f in WALKWAY_FILES:
        ok = inject(f, inner)
        print(f"{'OK  ' if ok else 'MISS'} {f}")
    print(
        f"Live: PORTED {d['ported']:,} REAL_GAP {d['real']:,} "
        f"PARTIAL {d['partial']:,} (total {d['total']:,})"
    )


if __name__ == "__main__":
    main()
