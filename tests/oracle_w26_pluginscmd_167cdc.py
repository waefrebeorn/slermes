#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugins_cmd_update_all.py
(gate w26_pluginscmd_167cdc).

The live ``plan_update_all`` runs against rows rebuilt from the fixture (the
CheckResult attribute set it reads) and install records as plain dicts; the
feed probe and ``update_one`` never run.  ``_short`` and the module literals are
called directly.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_pluginscmd_167cdc"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
os.environ.setdefault("USERPROFILE", str(QUARRY.parent))
os.environ.setdefault("HOME", str(QUARRY.parent))

import hermes_cli.plugins_cmd_update_all as live  # noqa: E402
from hermes_cli.plugins_auto_update import HELD_WITH_REF  # noqa: E402

WANTED = {"plan_update_all", "short", "constants"}

ROW_DEFAULTS = {
    "name": "",
    "klass": "",
    "update_available": False,
    "reason": "",
    "needs_fixing": None,
    "current": None,
    "latest": None,
    "min_hermes": None,
}


class Row:
    """The CheckResult surface plan_update_all reads, nothing more."""

    def __init__(self, raw):
        data = dict(ROW_DEFAULTS)
        data.update(raw if isinstance(raw, dict) else {})
        self.__dict__.update(data)


def expected(req):
    f = req.get("func")
    if f == "plan_update_all":
        raw = req.get("results")
        rows = [Row(r) for r in (raw if isinstance(raw, list) else [])]
        installs = req.get("installs")
        plan = live.plan_update_all(rows, installs if isinstance(installs, dict) else None)
        return {
            "movable": [r.name for r in plan.movable],
            "skipped": [[r.name, why] for r, why in plan.skipped],
            "current": plan.current,
        }
    if f == "short":
        return live._short(req.get("reason") if isinstance(req.get("reason"), str) else "")
    if f == "constants":
        return {"update_kind": dict(live._UPDATE_KIND),
                "held_with_ref": HELD_WITH_REF}
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
