#!/usr/bin/env python3
"""Differential oracle for hermes_cli/data_cleanup_holders.py (gate w26_datacleanu_bcef92).

The module coordinates a home wipe: it asks a gateway to drain, and refuses
while anything still owns the home.  Nothing here opens a socket, a pid probe or
a sqlite ledger -- those are seams, and the request carries their answers.  What
is compared is the decision each answer feeds:

  * ``pause_acked``        -- did the identified gateway itself ack the drain?
  * ``_OWNER_STOP_HINTS``  -- which advice a supervisor owner earns;
  * the drain deadline     -- ``min(120, max(5, drain_timeout + 5))``;
  * a backend ledger row   -- purpose / pid validation / still-running;
  * a cron execution row   -- handoff / pid gone / start-time identity.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_datacleanu_bcef92"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli import data_cleanup_holders as live  # noqa: E402

WANTED = {"pause_acked", "owner_stop_hint", "drain_timeout",
          "backend_owner_state", "cron_owner_state", "constants"}

HINT_DEFAULT = "stop it through its supervisor before retrying"


def pause_acked(r):
    return live.pause_acked(r.get("response"), r.get("identity"))


def owner_stop_hint(r):
    return live._OWNER_STOP_HINTS.get(r.get("owner") or "", HINT_DEFAULT)


def drain_timeout(r):
    response = r.get("response") or {}
    return min(120.0, max(5.0, float(response.get("drain_timeout", 30.0)) + 5.0))


def backend_owner_state(r):
    """One row of ``_refuse_backend_writers`` with ``entry_still_running`` fixed."""
    row = r.get("row")
    row = row if isinstance(row, dict) else {}
    if row.get("purpose") not in {"serve", "dashboard"}:
        return "skip"
    pid = row.get("pid")
    if type(pid) is not int or pid <= 0:
        return "invalid_pid"
    if r.get("running"):
        return "running"
    return "free"


def cron_owner_state(r):
    """One row of ``_refuse_cron_writers`` with the pid/start-time probes fixed."""
    row = r.get("row")
    row = row if isinstance(row, dict) else {}
    if row.get("handoff_pending"):
        return "handoff"
    if not r.get("pid_exists"):
        return "skip"
    current = r.get("current_start")
    recorded = row.get("process_started_at")
    if current is None or recorded is None or current == recorded:
        return "owns"
    return "free"


def constants():
    return {"owner_stop_hint_default": HINT_DEFAULT,
            "owner_stop_hints": dict(live._OWNER_STOP_HINTS)}


def expected(r):
    f = r.get("func")
    if f == "pause_acked":
        return pause_acked(r)
    if f == "owner_stop_hint":
        return owner_stop_hint(r)
    if f == "drain_timeout":
        return drain_timeout(r)
    if f == "backend_owner_state":
        return backend_owner_state(r)
    if f == "cron_owner_state":
        return cron_owner_state(r)
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
