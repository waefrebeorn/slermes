#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_picker_labels.py
(gate w24_telegrampi_acbcc8).

The live module is imported as-is: every function the gate ports is already
pure (os.path.commonprefix + regex + str slicing), so there is no seam to stub
-- the only adaptation is ``suffix_for``, a callable in Python and a {id:
suffix} map on the wire, because a pure function can only see values.

Two shapes are pinned here so the C side has a rule to match:

  * a non-string id/name/prefix/suffix, which upstream raises TypeError on,
    is ``"ERROR:type"`` (the error seam);
  * ``_around_difference`` is always reached with at least one look-alike
    (its caller applies ``or [""]``), so an empty list is ``[""]``.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_telegrampi_acbcc8"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import importlib.util  # noqa: E402

# The package __init__ registers the whole Telegram adapter (gateway, session,
# credentials); this module is pure text arithmetic, so it is loaded straight
# from its file and nothing else of the plugin is imported.
_spec = importlib.util.spec_from_file_location(
    "live_telegram_picker_labels",
    QUARRY / "plugins" / "platforms" / "telegram" / "telegram_picker_labels.py",
)
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

WANTED = {"prefix", "label", "around", "labels", "constants"}


def prefix(req):
    ids = req.get("model_ids")
    ids = ids if isinstance(ids, list) else []
    if any(not isinstance(m, str) for m in ids):
        return "ERROR:type"
    return live.shared_label_prefix(ids)


def label(req):
    model_id = req.get("model_id")
    if not isinstance(model_id, str):
        return "ERROR:type"
    pfx = req.get("prefix")
    sfx = req.get("suffix")
    if pfx is not None and not isinstance(pfx, str):
        return "ERROR:type"
    if sfx is not None and not isinstance(sfx, str):
        return "ERROR:type"
    return live.model_button_label(model_id, pfx or "", sfx or "")


def around(req):
    name = req.get("name")
    if not isinstance(name, str):
        return "ERROR:type"
    room = req.get("room")
    if isinstance(room, bool):
        room = int(room)
    if not isinstance(room, (int, float)):
        return "ERROR:type"
    others = req.get("others")
    others = others if isinstance(others, list) else []
    others = [o if isinstance(o, str) else "" for o in others] or [""]
    return live._around_difference(name, others, int(room))


def labels(req):
    ids = req.get("model_ids")
    if not isinstance(ids, list):
        return "ERROR:type"
    if any(not isinstance(m, str) for m in ids):
        return "ERROR:type"
    sfx = req.get("suffixes")
    sfx = sfx if isinstance(sfx, dict) else {}

    def suffix_for(model_id):
        value = sfx.get(model_id, "")
        return value if isinstance(value, str) else ""

    return live.model_button_labels(ids, suffix_for)


def constants():
    return {"model_label_budget": live.MODEL_LABEL_BUDGET}


def expected(r):
    f = r.get("func")
    if f == "prefix":
        return prefix(r)
    if f == "label":
        return label(r)
    if f == "around":
        return around(r)
    if f == "labels":
        return labels(r)
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
