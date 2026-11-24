#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_intake.py
(gate w22_telegramin_6b9a1e).

`effective_update_message` is pure over an update object, so the live function
runs unmodified. Attribute values are materialized into Python objects: every
fixture message may carry a "label" and equal non-empty labels are rendered as
the SAME object, which is how `message is update.business_message` is expressed
in a decoded JSON tree.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "platforms" / "telegram" / "telegram_intake.py"
GATE = "w22_telegramin_6b9a1e"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("telegram_intake_live", SRC)
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

WANTED = {"effective"}


class Update:
    pass


def materialize(value, memo):
    """Rebuild a fixture value, aliasing equal labels to one Python object."""
    if isinstance(value, dict):
        label = value.get("label")
        if isinstance(label, str) and label:
            if label in memo:
                return memo[label]
        made = {}
        if isinstance(label, str) and label:
            memo[label] = made
        for key, item in value.items():
            made[key] = materialize(item, memo)
        return made
    if isinstance(value, list):
        return [materialize(item, memo) for item in value]
    return value


def build_update(raw):
    if not isinstance(raw, dict):
        return raw
    memo = {}
    update = Update()
    for key, value in raw.items():
        setattr(update, key, materialize(value, memo))
    return update


def expected(r):
    if r.get("func") != "effective":
        raise AssertionError("unknown func %r" % (r.get("func"),))
    return live.effective_update_message(build_update(r.get("update")))


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
