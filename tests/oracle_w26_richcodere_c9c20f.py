#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/rich_code_regions.py
(gate w26_richcodere_c9c20f).

The live module is imported and its three public functions are called on the
request's text.  ``transform`` is a fixed input by name: the same callables the
C port implements (identity/upper/lower/bracket/drop) are handed to
``map_outside_code``, so what is compared is the region scan, not the callback.
Spans come back as tuples, so they are widened to lists for the JSON compare.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_richcodere_c9c20f"

# The plugin import chain builds the profile home at import time; the gate runs
# with no ambient HOME, so the repo parent stands in for it.
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("USERPROFILE", str(REPO.parent))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.platforms.telegram import rich_code_regions as live  # noqa: E402

WANTED = {"regions", "strip_code", "map_outside", "constants"}

TRANSFORMS = {
    "identity": lambda s: s,
    "upper": lambda s: s.upper(),
    "lower": lambda s: s.lower(),
    "bracket": lambda s: "[" + s + "]",
    "drop": lambda s: "",
}


def html_flag(req, default=True):
    return req["html"] if "html" in req else default


def regions(req):
    return [list(r) for r in live.rich_code_regions(req.get("text") or "",
                                                    html=html_flag(req))]


def strip_code(req):
    return live.strip_code(req.get("text") or "")


def map_outside(req):
    name = req.get("transform") or "identity"
    return live.map_outside_code(req.get("text") or "", TRANSFORMS[name],
                                 html=html_flag(req))


def constants():
    return {"fence_min_width": 3.0, "block_prefix_groups": 3.0}


def expected(r):
    f = r.get("func")
    if f == "regions":
        return regions(r)
    if f == "strip_code":
        return strip_code(r)
    if f == "map_outside":
        return map_outside(r)
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
