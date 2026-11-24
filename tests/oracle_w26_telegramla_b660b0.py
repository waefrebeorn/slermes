#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_latex.py
(gate w26_telegramla_b660b0).

The module is pure text: ``normalize_latex_macros(prose)`` and
``replace_macros_outside(prose, kept_spans)`` are called on the fixture's exact
strings, with the spans the rich client would keep.  No transport, no plugin,
nothing to fake.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_telegramla_b660b0"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# The platform package's import chain resolves the Hermes home at import time.
import os  # noqa: E402
os.environ.setdefault("USERPROFILE", str(QUARRY.parent))
os.environ.setdefault("HOME", str(QUARRY.parent))

from plugins.platforms.telegram import telegram_latex as live  # noqa: E402

WANTED = {"normalize", "replace_outside", "constants"}


def expected(r):
    f = r.get("func")
    if f == "normalize":
        return live.normalize_latex_macros(r.get("prose"))
    if f == "replace_outside":
        spans = r.get("kept_spans")
        if not isinstance(spans, list):
            raise TypeError("kept_spans must be a list")
        return live.replace_macros_outside(r.get("prose"), [tuple(s) for s in spans])
    if f == "constants":
        return {
            "macro_count": len(live._MACROS),
            "inline_math_body_max": 120,
            "sample": {
                "to": live._MACROS["to"],
                "leq": live._MACROS["leq"],
                "alpha": live._MACROS["alpha"],
                "leftrightarrow": live._MACROS["leftrightarrow"],
                "infty": live._MACROS["infty"],
            },
            "ellipsis": "\u2026",
        }
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
        except TypeError:
            py = {"error": "TypeError"}
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
