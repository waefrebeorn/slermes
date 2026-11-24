#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_server_idle_exit.py
(gate w26_webserveri_efe232).

The clock, the session table and the busy ledgers are the seams; each is a fixed
input while the live code runs:

  * ``IdleClientTracker(now=<fake>)`` -- the real counter, driven by the
    fixture's open/close events and read back through ``live_count`` /
    ``idle_for``, so the lock-guarded arithmetic is the live one;
  * ``_session_work_in_flight`` -- the real probe over a dict whose thread slots
    hold objects with ``is_alive()``;
  * ``should_exit_idle`` -- the real predicate over a tracker seeded with the
    fixture's live count and last-client stamp;
  * ``busy_ledger`` -- substituted so the real ``turn_in_flight`` decides.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_webserveri_efe232"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli import web_server_idle_exit as live  # noqa: E402

WANTED = {"tracker", "session_work_in_flight", "should_exit_idle",
          "turn_in_flight_view", "constants"}

_PROBE = {"true": True, "false": False, "none": None}


class _Thread:
    def __init__(self, alive):
        self._alive = bool(alive)

    def is_alive(self):
        return self._alive


def tracker(req):
    clock = [req.get("start", 0.0)]
    trk = live.IdleClientTracker(now=lambda: clock[0])
    for op in req.get("ops") or []:
        if not isinstance(op, dict):
            continue
        if "now" in op:
            clock[0] = op["now"]
        name = op.get("op")
        if name == "open":
            trk.on_open()
        elif name == "close":
            trk.on_close()
    if "now" in req:
        clock[0] = req["now"]
    return {"live": trk.live_count(), "idle_for": trk.idle_for()}


def session_work_in_flight(req):
    raw = req.get("session")
    session = dict(raw) if isinstance(raw, dict) else {}
    for key in ("_run_thread", "_agent_build_thread"):
        value = session.get(key)
        if isinstance(value, dict):
            session[key] = _Thread(value.get("alive"))
    return live._session_work_in_flight(session)


def should_exit_idle(req):
    last = req.get("last_client_at", 0.0)
    now = req.get("now", last)
    trk = live.IdleClientTracker(now=lambda: now)
    trk._live = req.get("live", 0)
    trk._last_client_at = last
    probe = _PROBE[req.get("probe", "false")]
    return live.should_exit_idle(trk, req.get("grace_s", live.DEFAULT_IDLE_GRACE_S),
                                 lambda: probe)


def turn_in_flight_view(req):
    ledger = req.get("ledger")
    saved = live.busy_ledger
    live.busy_ledger = lambda: ledger
    try:
        return live.turn_in_flight()
    finally:
        live.busy_ledger = saved


def constants():
    return {"default_idle_grace_s": float(live.DEFAULT_IDLE_GRACE_S),
            "tunnel_ws_ping_interval_s": float(live.TUNNEL_WS_PING_INTERVAL_S),
            "tunnel_ws_ping_timeout_s": float(live.TUNNEL_WS_PING_TIMEOUT_S)}


DISPATCH = {
    "tracker": tracker,
    "session_work_in_flight": session_work_in_flight,
    "should_exit_idle": should_exit_idle,
    "turn_in_flight_view": turn_in_flight_view,
    "constants": lambda req: constants(),
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
            py = DISPATCH[r["func"]](r)
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
