#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cli_backtrack_browse.py (gate w26_clibacktra_0c7253).

The pager's pure geometry is called on the live module: the terminal size and
prompt_toolkit's cell widths are the seams.  ``shutil.get_terminal_size`` is
replaced by a fixed size per case (so the answer is a function of the values,
not of the console the oracle happens to run in); the footer's cell width is
``get_cwidth``, which equals the code-point count for the ASCII/arrow hints the
module builds.

  * ``scroll_into_view`` -- the smallest scroll that shows the selection;
  * ``pager_geometry``   -- (inner width, visible rows) for a terminal size;
  * ``fit_browse_hint``  -- the footer that survives a narrowing width;
  * the style constants the pager paints its rows with.
"""
import json
import os
import shutil
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_clibacktra_0c7253"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli import cli_backtrack_browse as live  # noqa: E402

WANTED = {"scroll", "geometry", "fit_hint", "constants"}

_SIZES = {"cur": None}


def _fixed_terminal_size(fallback=(100, 24)):
    size = _SIZES["cur"]
    return shutil.os.terminal_size(size if size is not None else fallback)


shutil.get_terminal_size = _fixed_terminal_size


def scroll(req):
    return live.scroll_into_view(int(req["offset"]), tuple(req["span"]),
                                 int(req["visible"]), int(req["total"]))


def geometry(req):
    _SIZES["cur"] = (int(req["columns"]), int(req["lines"]))
    return list(live.pager_geometry(int(req["reserved_below"]), int(req.get("hint_rows", 1))))


def fit_hint(req):
    return live.fit_browse_hint(req["hint"], int(req["width"]))


def constants():
    return {
        "prompt_style": live.PROMPT_STYLE,
        "selected_style": live.SELECTED_STYLE,
        "reply_style": live.REPLY_STYLE,
        "border": live._BORDER,
    }


def expected(r):
    f = r.get("func")
    if f == "scroll":
        return scroll(r)
    if f == "geometry":
        return geometry(r)
    if f == "fit_hint":
        return fit_hint(r)
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
