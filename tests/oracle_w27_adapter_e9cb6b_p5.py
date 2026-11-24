#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/matrix/adapter.py (gate p5).

The HTML core is driven directly: ``_html_escape`` is the module's ``html.escape``
binding and ``_markdown_to_html_fallback`` is a pure str->str method.  The
``markdown`` package path (`_markdown_to_html`) and the adapter_html sanitizer
are separate modules and stay seamed.
"""

import json
import os
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_e9cb6b_p5"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("HERMES_HOME", tempfile.mkdtemp(prefix="w27-oracle-home-"))

from plugins.platforms.matrix import adapter as live  # noqa: E402

ADAPTER = live.MatrixAdapter

WANTED = {"html_escape", "markdown_to_html_fallback"}


def _escape(r):
    return live._html_escape(r.get("text") or "")


def _fallback(r):
    return ADAPTER._markdown_to_html_fallback(r.get("text") or "")


CASES = {"html_escape": _escape, "markdown_to_html_fallback": _fallback}


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
            py = CASES[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
