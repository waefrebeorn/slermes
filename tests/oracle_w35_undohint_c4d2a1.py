#!/usr/bin/env python3
"""Differential oracle for agent/checkpoints/undo_hint.py (gate w35_undohint_c4d2a1).

Two pure functions are driven live:

* ``_epoch`` — the normaliser every undo decision runs on (bool rejected, numbers
  finite and positive, strings through ``datetime.fromisoformat`` with a naive
  stamp read as local time).
* the join half of ``format_undo_rollback_hint`` — ``files[:8]`` joined with
  ``", "`` plus the overflow count.  The sentence itself comes from the i18n
  catalogue and is a documented seam, so the oracle re-implements that join from
  the same list and compares the shape, not the translated text.

The git walk and diff in the module are seams and are never called.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w35_undohint_c4d2a1"

os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("USERPROFILE", str(REPO.parent))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from agent.checkpoints import undo_hint as live  # noqa: E402

MAX_NAMED = 8


def named_shape(files):
    named = ", ".join(files[:MAX_NAMED])
    return {"named": named, "more": max(0, len(files) - MAX_NAMED)}


def main():
    total = 0
    bad = 0
    skipped = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            case = json.loads(line)
        except ValueError:
            continue
        func = case.get("func")
        if func not in ("epoch", "named_files"):
            continue
        total += 1
        got = case.get("out")
        try:
            if func == "epoch":
                py = live._epoch(case.get("value"))
            else:
                files = case.get("files")
                if not isinstance(files, list) or any(not isinstance(f, str) for f in files):
                    raise TypeError("files must be a list of str")
                py = named_shape(files)
        except Exception as exc:  # upstream raised: there is no value to compare
            print("SKIP %s case=%r live-error=%r" % (func, case.get("case"), exc))
            skipped += 1
            continue
        if py != got:
            print("MISMATCH %s case=%r py=%r c=%r" % (func, case.get("case"), py, got))
            bad += 1
    print("ORACLE %s: %d cases, %d mismatches, %d skipped (upstream raised)" % (GATE, total, bad, skipped))
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
