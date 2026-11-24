#!/usr/bin/env python3
"""Differential oracle for persistence/core/conventions.py
(gate w22_convention_a3be4b).

The module is imported live from the quarry (it is pure: logging, math,
datetime and enum). The logging sink is its only side effect and writes a
WARNING for a corrupt cell; the compared value is the decoded instant.

  ensure_utc  -- live `datetime.fromisoformat` + live `ensure_utc`; a naive
                 instant and an unparsable one are reported apart.
  stored      -- live `stored_instant`, with `{"__dt__": iso}` standing in for a
                 caller's datetime object (the fixture side of that seam).
  visibility  -- live RowVisibility members.
  limits      -- the live trust-window constants and UNTRUSTED_INSTANT.
"""
import importlib.util
import json
import sys
from datetime import datetime
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "persistence" / "core" / "conventions.py"
GATE = "w22_convention_a3be4b"

WANTED = {"ensure_utc", "stored", "visibility", "limits"}

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("conventions_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["conventions_live"] = live
_spec.loader.exec_module(live)


def ensure_utc(text):
    try:
        value = datetime.fromisoformat(text)
    except (TypeError, ValueError):
        return {"ok": False, "value": None, "error": "parse"}
    try:
        normalized = live.ensure_utc(value)
    except ValueError:
        return {"ok": False, "value": None, "error": "naive"}
    return {"ok": True, "value": normalized.isoformat(), "error": None}


def stored(value):
    if isinstance(value, dict):
        marker = value.get("__dt__")
        if isinstance(marker, str):
            try:
                value = datetime.fromisoformat(marker)
            except ValueError:
                value = marker  # an unparsable marker stays a string: untrusted
        else:
            value = value  # a plain dict: float(dict) raises -> untrusted
    instant = live.stored_instant(value)
    return None if instant is None else instant.timestamp()


def expected(r):
    f = r.get("func")
    if f == "ensure_utc":
        return ensure_utc(r.get("iso"))
    if f == "stored":
        return stored(r.get("value"))
    if f == "visibility":
        return [{"name": member.name, "value": member.value} for member in live.RowVisibility]
    if f == "limits":
        return {"floor": live._STORED_EPOCH_MIN, "ceiling": live._STORED_EPOCH_MAX,
                "untrusted_epoch": live.UNTRUSTED_INSTANT.timestamp(),
                "untrusted_iso": live.UNTRUSTED_INSTANT.isoformat()}
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
