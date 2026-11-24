#!/usr/bin/env python3
"""Differential oracle for tools/image_generation_managed.py
(gate w23_imagegener_6a7ba9).

Everything runs live.  The module's own catalogs are the in-tree Python data
(``tools.image_generation_catalog.FAL_MODELS`` and
``plugins.image_gen.krea.KREA_MODEL_IDS``), imported by the live functions
themselves, so the id membership the C side transcribes is checked against the
real catalog rather than against a copy.

No seam is stubbed: ``managed_backend_for_model`` / ``managed_route`` are pure
functions of the raw stored config values the fixture supplies.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w23_imagegener_6a7ba9"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import tools.image_generation_managed as live  # noqa: E402

WANTED = {"backend_for_model", "route", "portal_duplicate_of", "is_fal_krea_entry"}


def expected(r):
    f = r.get("func")
    if f == "backend_for_model":
        return live.managed_backend_for_model(r.get("model_id"))
    if f == "route":
        return live.managed_route(r.get("provider"), r.get("model_id"))
    if f == "portal_duplicate_of":
        mid = r.get("model_id")
        return live._PORTAL_DUPLICATE_OF.get(mid) if isinstance(mid, str) else None
    if f == "is_fal_krea_entry":
        mid = r.get("model_id")
        return bool(isinstance(mid, str) and mid.startswith(live._FAL_KREA_PREFIX))
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
