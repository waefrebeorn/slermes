#!/usr/bin/env python3
"""Differential oracle for agent/notification_presentation.py
(gate w25_notificati_ae9773).

The live module is imported from the Python quarry and its two predicates are
called directly.  The ContextVar that carries the muted surface is a seam, so
the oracle sets it from the request and resets it after the call; the agent's
callback attributes and the config loader never come into play:

  * ``event_presentation_muted`` over (muted surface, event, session id);
  * ``diagnostic_process_event`` over an event dict;
  * the ``_FREEFORM_EVENTS`` / ``_PRESENTATION_CALLBACKS`` literal tables.

A live call that raises (an unhashable event, a non-dict event) is reported as
``{"__error__": "<type>"}`` so the C side can be compared on the error seam too.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_notificati_ae9773"

if not os.environ.get("USERPROFILE") and not os.environ.get("HOMEPATH"):
    os.environ["USERPROFILE"] = str(REPO.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from agent import notification_presentation as live  # noqa: E402

WANTED = {"event_muted", "diagnostic_process_event", "constants"}


def event_muted(req):
    token = live._muted_surface.set(req.get("muted_surface"))
    try:
        return live.event_presentation_muted(req.get("event"), req.get("session_id"))
    finally:
        live._muted_surface.reset(token)


def diagnostic_process_event(req):
    return live.diagnostic_process_event(req.get("event"))


def constants(_req):
    return {
        "freeform_events": sorted(live._FREEFORM_EVENTS),
        "presentation_callbacks": list(live._PRESENTATION_CALLBACKS),
        "muted_variable": live._muted_surface.name,
        "muted_default": live._muted_surface.get(),
    }


def expected(r):
    f = r.get("func")
    if f == "event_muted":
        return event_muted(r)
    if f == "diagnostic_process_event":
        return diagnostic_process_event(r)
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
