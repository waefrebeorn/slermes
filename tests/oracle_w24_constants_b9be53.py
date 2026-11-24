#!/usr/bin/env python3
"""Differential oracle for agent/pet/constants.py (gate w24_constants_b9be53).

The live module is pure already: geometry literals, the two row taxonomies, the
alias table and the scale/column resolvers.  Nothing is patched -- every case
calls the upstream function on the same value the C side was handed, and the
spritesheet seam (row_count) arrives as a value exactly as the atlas probe would
have produced it.

Python ``round`` is round-half-to-even and ``int()`` truncates, so a mismatch
here is a rounding or coercion defect in the port, not a formatting difference.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_constants_b9be53"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from agent.pet import constants as live  # noqa: E402

WANTED = {
    "clamp_scale", "cols_for_scale", "resolve_cols", "state_aliases_for",
    "state_rows_for_grid", "state_row_index", "constants",
}


def as_list(value):
    """Tuples and lists compare equal in JSON: normalise to a list of strings."""
    return [str(v) for v in value]


def expected(r):
    f = r.get("func")
    if f == "clamp_scale":
        return live.clamp_scale(r.get("scale"))
    if f == "cols_for_scale":
        return live.cols_for_scale(r.get("scale"))
    if f == "resolve_cols":
        return live.resolve_cols(r.get("scale"), r.get("unicode_cols"))
    if f == "state_aliases_for":
        return as_list(live.state_aliases_for(r.get("state")))
    if f == "state_rows_for_grid":
        return as_list(live.state_rows_for_grid(r.get("row_count")))
    if f == "state_row_index":
        return live.state_row_index(r.get("state"), r.get("row_count"))
    if f == "constants":
        return {
            "FRAME_W": live.FRAME_W,
            "FRAME_H": live.FRAME_H,
            "FRAMES_PER_STATE": live.FRAMES_PER_STATE,
            "LOOP_MS": live.LOOP_MS,
            "DEFAULT_SCALE": live.DEFAULT_SCALE,
            "MIN_SCALE": live.MIN_SCALE,
            "MAX_SCALE": live.MAX_SCALE,
            "BASE_UNICODE_COLS": live.BASE_UNICODE_COLS,
            "UNICODE_MIN_COLS": live.UNICODE_MIN_COLS,
            "LEGACY_STATE_ROWS": as_list(live.LEGACY_STATE_ROWS),
            "CODEX_STATE_ROWS": as_list(live.CODEX_STATE_ROWS),
            "STATE_ROWS": as_list(live.STATE_ROWS),
            "STATE_ALIASES": {k: as_list(v) for k, v in live.STATE_ALIASES.items()},
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
