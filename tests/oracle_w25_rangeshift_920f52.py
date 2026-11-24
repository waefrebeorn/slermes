#!/usr/bin/env python3
"""Differential oracle for agent/lsp/range_shift.py (gate w25_rangeshift_920f52).

The live module is imported as-is and its public helpers are called on plain
values: ``build_line_shift`` over a pre/post text pair, then the closure, then
``shift_diagnostic_range`` and ``shift_baseline`` over diagnostic values.  No
document store, no LSP transport, no editor: the seam is the text pair, and the
opcodes the shift is built from come from the live ``difflib.SequenceMatcher``.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_rangeshift_920f52"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from agent.lsp import range_shift as live  # noqa: E402

WANTED = {"shift_line", "shift_diag", "shift_baseline"}


def _shift(r):
    return live.build_line_shift(r.get("pre_text"), r.get("post_text"))


def shift_line(r):
    return _shift(r)(int(r.get("line", 0)))


def shift_diag(r):
    return live.shift_diagnostic_range(r.get("diag"), _shift(r))


def shift_baseline(r):
    raw = r.get("baseline")
    return live.shift_baseline(raw if raw is not None else [], _shift(r))


def expected(r):
    f = r.get("func")
    if f == "shift_line":
        return shift_line(r)
    if f == "shift_diag":
        return shift_diag(r)
    if f == "shift_baseline":
        return shift_baseline(r)
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
