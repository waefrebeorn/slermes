#!/usr/bin/env python3
"""Differential oracle for hermes_cli/config_defaults_auxiliary.py
(gate w22_configdefa_fa2045).

The module is pure: `_aux(timeout, *, reasoning_effort=True, **extra)` builds a
dict and nothing else. The live factory is imported from the quarry and called
with the fixture's timeout/flag/extra using the same binding shape the call
sites use (`**extra`), so a duplicate keyword or a non-mapping extra surfaces
as the live TypeError.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "config_defaults_auxiliary.py"
GATE = "w22_configdefa_fa2045"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("config_defaults_auxiliary_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["config_defaults_auxiliary_live"] = live
_spec.loader.exec_module(live)

WANTED = {"aux"}


def aux(r):
    extra = r.get("extra")
    flag = r.get("reasoning_effort", True)
    try:
        if extra is None:
            value = live._aux(r.get("timeout"), reasoning_effort=flag)
        else:
            value = live._aux(r.get("timeout"), reasoning_effort=flag, **extra)
    except TypeError:
        return {"ok": False, "error": "TypeError"}
    return {"ok": True, "value": value}


def expected(r):
    f = r.get("func")
    if f == "aux":
        return aux(r)
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
