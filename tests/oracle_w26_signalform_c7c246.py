#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/signal/signal_format.py (gate w26_signalform_c7c246).

Signal carries plain text plus ``bodyRanges``; every function compared here is a
pure string transform from the live module (no adapter, no socket, no client):

  * ``_utf16_len``              -- the code-unit ruler style offsets are printed in;
  * ``_split_fenced``           -- prose / fenced alternation;
  * ``_normalize_bullet_markers``-- Markdown bullets -> Unicode bullets outside fences;
  * ``markdown_to_signal``      -- the whole text + ``start:length:STYLE`` pass;
  * the literal pattern tables and the style vocabulary.

``markdown_to_signal`` re-aligns GFM tables through ``wcwidth``; that width
table is a seam the C port does not carry, so a table outside a fence is
reported by the C port as ``{"error": "table_seam"}`` and no fixture contains
one.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_signalform_c7c246"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# The adapter chain this module lives in resolves the Hermes home at import
# time; give it the repo parent when the shell carries no profile home.
import os  # noqa: E402

if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(QUARRY.parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = str(QUARRY.parent)

from plugins.platforms.signal import signal_format as live  # noqa: E402

WANTED = {"utf16_len", "split_fenced", "normalize_bullets", "markdown_to_signal", "constants"}


def text_of(r):
    text = r.get("text")
    return text if isinstance(text, str) else ""


def utf16_len(r):
    return live._utf16_len(text_of(r))


def split_fenced(r):
    return live._split_fenced(text_of(r))


def normalize_bullets(r):
    return live._normalize_bullet_markers(text_of(r))


def markdown_to_signal(r):
    text, styles = live.markdown_to_signal(text_of(r))
    return {"text": text, "styles": styles}


def constants():
    return {
        "table_width": live._TABLE_WIDTH,
        "styles": sorted({style for _pat, style in live._INLINE_PATTERNS} | {"MONOSPACE"}),
        "inline_patterns": [[pat.pattern, style] for pat, style in live._INLINE_PATTERNS],
        "code_block_pattern": live._CODE_BLOCK_RE.pattern,
        "fence_split_pattern": live._FENCE_SPLIT_RE.pattern,
        "heading_pattern": live._HEADING_RE.pattern,
    }


def expected(r):
    f = r.get("func")
    if f == "utf16_len":
        return utf16_len(r)
    if f == "split_fenced":
        return split_fenced(r)
    if f == "normalize_bullets":
        return normalize_bullets(r)
    if f == "markdown_to_signal":
        return markdown_to_signal(r)
    if f == "constants":
        return constants()
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
