#!/usr/bin/env python3
"""Differential oracle for agent/auxiliary_reasoning_floor.py (gate w26_auxiliaryr_2d56c8).

Every function compared here is pure over its arguments:

  * ``_route_key``            -- provider/base_url -> the retry's memo key;
  * ``_is_disabled``          -- is this reasoning_config a thinking-off shape;
  * ``floor_reasoning_config``-- the disabled config lifted to the floor;
  * ``with_reasoning_floor``  -- kwargs copy with every disabled encoding lifted;
  * ``known_reasoning_floor`` -- the same lift, gated on the two seams the live
                                 function reads (the in-process memo and the
                                 /v1/models catalog), which the request carries
                                 as ``known`` / ``catalog`` booleans;
  * the module constants.

The memo set, the catalog, the HTTP probe and the logger never run: what is
compared is the value those answers decide.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_auxiliaryr_2d56c8"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from agent import auxiliary_reasoning_floor as live  # noqa: E402

WANTED = {"route_key", "is_disabled", "floor_config", "with_floor", "known_floor", "constants"}


def route_key(r):
    return live._route_key(r.get("provider"), r.get("base_url"))


def is_disabled(r):
    return live._is_disabled(r.get("config"))


def floor_config(r):
    return live.floor_reasoning_config(r.get("config"))


def with_floor(r):
    kwargs = r.get("kwargs")
    return live.with_reasoning_floor(dict(kwargs) if isinstance(kwargs, dict) else kwargs)


def known_floor(r):
    """``known_reasoning_floor`` with its two read seams (memo, catalog) fixed."""
    config = r.get("config")
    if not live._is_disabled(config):
        return config
    if not (r.get("known") or r.get("catalog")):
        return config
    return live.floor_reasoning_config(config)


def constants():
    return {
        "floor_effort": live.REASONING_FLOOR_EFFORT,
        "disabled_efforts": sorted(live._DISABLED_EFFORTS),
    }


def expected(r):
    f = r.get("func")
    if f == "route_key":
        return route_key(r)
    if f == "is_disabled":
        return is_disabled(r)
    if f == "floor_config":
        return floor_config(r)
    if f == "with_floor":
        return with_floor(r)
    if f == "known_floor":
        return known_floor(r)
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
