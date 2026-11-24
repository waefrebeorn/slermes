#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/admin/_backup.py (gate w32_backup_1d0d4e_p1).

The backup admin is imported as a package module; the parts exercised here are
the value-only half of the emergency-snapshot path:

  * ``BackupAdmin.preflight_backup``'s UTC stamp and the three names it derives
    (plus the restore temp names) - pathlib arithmetic only;
  * the two-newest retention ``sorted(home.glob(prefix + "*.bak"), reverse=True)[2:]``;
  * ``_reclaim_abandoned_staging`` driven LIVE over a real temporary directory
    whose files carry the fixture mtimes;
  * ``_CopyRate`` / the stepped-copy page budget and the refusal text from
    ``_stepped_rollback_journal_copy`` (live class + live constants).

Every filesystem/sqlite/clock effect stays here in the oracle; the C side is
fed the same values and must decide identically.
"""
import json
import os
import sys
import tempfile
from datetime import UTC, datetime
from pathlib import Path, PurePosixPath

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w32_backup_1d0d4e_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite.admin import _backup as live  # noqa: E402

PREFIX = "state.db.pre-update-emergency-"
WANTED = {"stamp", "paths", "prune", "reclaim", "step_plan", "hold"}


def stamp(req):
    return datetime.fromtimestamp(req["epoch"], UTC).strftime("%Y-%m-%dT%H-%M-%S-%fZ")


def paths(req):
    home = PurePosixPath(req["home"])
    when = req["stamp"]
    pid = req["pid"]
    db_name = req.get("db_name") or "state.db"
    staged = home / f"{PREFIX}{when}-{pid}.partial"
    return {
        "destination": str(home / f"{PREFIX}{when}-{pid}.bak"),
        "staged": str(staged),
        "journal": str(PurePosixPath(f"{staged}-journal")),
        "tmp_restore": str(home / f".{db_name}.snap_restore"),
        "sidecars": [str(home / f"{db_name}{suffix}")
                     for suffix in ("-wal", "-shm", "-journal")],
    }


def prune(req):
    with tempfile.TemporaryDirectory() as work:
        home = Path(work)
        for name in req["names"]:
            (home / name).write_text("x", encoding="utf-8")
        ordered = sorted(home.glob(PREFIX + "*.bak"), reverse=True)
        return {"ordered": [p.name for p in ordered],
                "prune": [p.name for p in ordered[2:]]}


def reclaim(req):
    from unittest import mock

    with tempfile.TemporaryDirectory() as work:
        home = Path(work)
        names = req["names"]
        for name, mtime in zip(names, req["mtimes"]):
            target = home / name
            target.write_text("x", encoding="utf-8")
            os.utime(target, (mtime, mtime))
        # The live function reads time.time(); pin it to the fixture clock so
        # the two-hour horizon is the same instant the C side is given.
        with mock.patch("time.time", return_value=float(req["now"])):
            live._reclaim_abandoned_staging(home, PREFIX)
        survivors = sorted(p.name for p in home.iterdir())
        return {"removed": sorted(set(names) - set(survivors)), "survivors": survivors}


def _rate(req):
    rate = live._CopyRate()
    rate.pages = req["rate_pages"]
    rate.seconds = req["rate_seconds"]
    return rate


def step_plan(req):
    rate = _rate(req)
    per_second = rate.pages_per_second()
    return {"pages_per_second": per_second,
            "next_pages": max(live._ROLLBACK_FIRST_STEP_PAGES,
                              int(per_second * live._ROLLBACK_STEP_SECONDS))}


def hold(req):
    rate = _rate(req)
    rate.total = req["total"]
    expected = rate.total / max(rate.pages_per_second(), 1.0)
    refuse = expected > live._ROLLBACK_LOCK_HOLD_SECONDS
    message = None
    if refuse:
        message = ("state.db uses a rollback journal and is written continuously; a consistent snapshot "
                   f"would block those writes for about {expected:.1f}s, so it was not taken. "
                   "Retry when no agent turn is running.")
    return {"expected_hold": expected, "refuse": refuse, "message": message}


def expected(r):
    func = r.get("func")
    if func == "stamp":
        return stamp(r)
    if func == "paths":
        return paths(r)
    if func == "prune":
        return prune(r)
    if func == "reclaim":
        return reclaim(r)
    if func == "step_plan":
        return step_plan(r)
    if func == "hold":
        return hold(r)
    raise AssertionError("unknown func %r" % (func,))


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
