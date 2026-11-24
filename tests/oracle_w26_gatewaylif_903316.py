#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_lifecycle_guards.py (gate w26_gatewaylif_903316).

The gateway probe is the seam: ``live_work`` answers from the control socket, and
``describe_active_work_unit`` reads the cron job registry from a profile's
``jobs.json``.  Both are supplied as fixed inputs here:

  * ``answers`` stands in for ``live_work`` per candidate home (refusal_lines);
  * no fixture home carries a ``jobs.json``, so a cron unit renders by id alone;
  * ``_fmt_elapsed`` / ``describe_active_work_unit`` / ``_work_lines`` /
    ``refusal_lines`` themselves are the live functions, called on those values.
"""
import json
import os
import sys
from pathlib import Path
from unittest import mock

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_gatewaylif_903316"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
os.environ.setdefault("USERPROFILE", str(QUARRY.parent))
os.environ.setdefault("HOME", str(QUARRY.parent))

from hermes_cli import gateway_lifecycle_guards as live  # noqa: E402
from hermes_cli.update_cmd_drain_report import (  # noqa: E402
    _fmt_elapsed,
    describe_active_work_unit,
)

WANTED = {"elapsed", "describe", "work_lines", "refusal_lines", "constants"}


def elapsed(req):
    return _fmt_elapsed(req.get("seconds"))


def describe(req):
    unit = req.get("unit")
    return describe_active_work_unit(unit if isinstance(unit, dict) else {}, None)


def work_lines(req):
    work = req.get("work")
    work = work if isinstance(work, dict) else {}
    home = Path(str(req.get("home") or "/nonexistent-w26-home"))
    hint = req.get("flag_hint")
    hint = hint if isinstance(hint, str) else "hermes gateway {verb} --yes"
    return live._work_lines(str(req.get("verb") or ""), home, work, hint)


def refusal_lines(req):
    homes = [Path(str(h)) for h in (req.get("homes") or [])]
    answers = list(req.get("answers") or [])
    seen = {"i": 0}

    def fake_live_work(home, profile=None):
        i = seen["i"]
        seen["i"] = i + 1
        return answers[i] if i < len(answers) else None

    hint = req.get("flag_hint")
    kwargs = {} if not isinstance(hint, str) else {"flag_hint": hint}
    with mock.patch.object(live, "live_work", fake_live_work):
        return live.refusal_lines(str(req.get("verb") or ""), homes, **kwargs)


def constants():
    return {"interrupt_flag": live.INTERRUPT_FLAG}


def expected(r):
    f = r.get("func")
    if f == "elapsed":
        return elapsed(r)
    if f == "describe":
        return describe(r)
    if f == "work_lines":
        return work_lines(r)
    if f == "refusal_lines":
        return refusal_lines(r)
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
