#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/_store/coverage.py
(gate w34_coverage_56490e).

The live module is imported as a package module and only its pure value
functions are called: no StoreHolder, no connection, no SQL.  Every seam the
module talks to (the state.db connection, the holder, the writer's message
serializer and stored-row digest) is outside these six functions, so the
comparison is exactly what those values decide:

  * ``_text_of``               -- the text a message carries;
  * ``_holds_as_block``        -- whole-line containment of the stored text;
  * ``_images_replaced``       -- an image part swapped for a text placeholder;
  * ``_carried_tail_slots``    -- carried dicts whose original sits in the tail;
  * ``_uncounted_merged_rows`` -- durable rows behind the tail beyond one/dict;
  * ``_tail_originals``        -- the newest covered rows the tail stands for.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w34_coverage_56490e"
MIN_CASES = 10

# insert(0): the quarry must win over this repo's own tests/ packages.
if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
# stub helper only; appended so it can never shadow a quarry package
sys.path.append(str(Path(__file__).resolve().parent))
try:  # host-blocked native extensions
    from hermes_seam_stubs import install as _install_seams

    _install_seams()
except Exception:  # pragma: no cover - stubs are only needed on a hostile host
    pass

from persistence.sqlite.conversation._store import coverage as live  # noqa: E402

WANTED = {
    "text_of", "holds_as_block", "images_replaced",
    "carried_tail_slots", "uncounted_merged_rows", "tail_originals",
}


def _s(req, key):
    v = req.get(key)
    return v if isinstance(v, str) else ""


def _ints(seq):
    return [int(x) for x in seq] if isinstance(seq, list) else []


def text_of(req):
    return live._text_of(req.get("content"))


def holds_as_block(req):
    return live._holds_as_block(_s(req, "inner"), _s(req, "outer"))


def images_replaced(req):
    return live._images_replaced(req.get("stored"), req.get("held"))


def carried_tail_slots(req):
    raw = req.get("carried")
    carried = {}
    for key, value in (raw.items() if isinstance(raw, dict) else ()):
        carried[int(key)] = int(value)
    return live._carried_tail_slots(carried, int(req.get("compacted_count") or 0),
                                   int(req.get("tail_count") or 0))


def uncounted_merged_rows(req):
    tail = req.get("tail")
    return live._uncounted_merged_rows(tail if isinstance(tail, list) else [])


def tail_originals(req):
    return live._tail_originals(_ints(req.get("covered_active")),
                                int(req.get("tail_count") or 0),
                                set(_ints(req.get("merged_away"))))


EXPECTED = {
    "text_of": text_of,
    "holds_as_block": holds_as_block,
    "images_replaced": images_replaced,
    "carried_tail_slots": carried_tail_slots,
    "uncounted_merged_rows": uncounted_merged_rows,
    "tail_originals": tail_originals,
}


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
            py = EXPECTED[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n  c =%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    if cases < MIN_CASES:
        print("ORACLE %s: only %d cases (< %d) -- gate not exercised"
              % (GATE, cases, MIN_CASES), file=sys.stderr)
        sys.exit(1)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
