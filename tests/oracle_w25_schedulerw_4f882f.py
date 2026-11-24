#!/usr/bin/env python3
"""Differential oracle for cron/scheduler_worktree_maintenance.py
(gate w25_schedulerw_4f882f).

The live module is imported from the Python quarry and its pure decision
function is called with its three seams fixed by the request:

  * ``time.monotonic`` -> the request's ``state.now``;
  * ``_last_worktree_prune_at`` -> the request's ``state.last_prune``;
  * ``os.path`` (realpath/normcase) -> identity, so the repo strings are already
    the canonical keys the filesystem would resolve them to.

``claim_due_worktree_repos`` then decides which repos are due; the semaphore
itself and the prune subprocess stay out of the comparison.  ``claim_sweep_slot``'s
wait argument and ``SWEEP_WAIT_SECONDS`` are value-only and compared directly.
"""
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_schedulerw_4f882f"

if not os.environ.get("USERPROFILE") and not os.environ.get("HOMEPATH"):
    os.environ["USERPROFILE"] = str(REPO.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from cron import scheduler_worktree_maintenance as live  # noqa: E402

WANTED = {"claim_due", "sweep_slot_timeout", "constants"}

REAL_OS = live.os
REAL_TIME = live.time
REAL_MAP = live._last_worktree_prune_at


def claim_due(req):
    state = req.get("state")
    state = state if isinstance(state, dict) else {}
    stamp = state.get("last_prune")
    live._last_worktree_prune_at = dict(stamp) if isinstance(stamp, dict) else {}
    live.os = SimpleNamespace(path=SimpleNamespace(realpath=lambda p: p,
                                                   normcase=lambda p: p))
    live.time = SimpleNamespace(monotonic=lambda: state.get("now"))
    try:
        return live.claim_due_worktree_repos(req.get("repos"), req.get("interval_s"))
    finally:
        live.os = REAL_OS
        live.time = REAL_TIME
        live._last_worktree_prune_at = REAL_MAP


def sweep_slot_timeout(req):
    # claim_sweep_slot's only value decision is the timeout it passes to acquire.
    provided = req.get("wait_seconds")
    return live.SWEEP_WAIT_SECONDS if provided is None else provided


def constants(_req):
    return {"sweep_wait_seconds": float(live.SWEEP_WAIT_SECONDS)}


def expected(r):
    f = r.get("func")
    if f == "claim_due":
        return claim_due(r)
    if f == "sweep_slot_timeout":
        return sweep_slot_timeout(r)
    if f == "constants":
        return constants(r)
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
            py = {"__error__": type(exc).__name__}
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
