#!/usr/bin/env python3
"""Differential oracle for tui_gateway/session_notifications.py (gate
w29_sessionnot_c045cd_p1: routing identity, dedupe keys, poller budgets).

The live module is imported as a package module so its relative import works.
Nothing in the poller's process runs: the pieces the pure half talks to are
injected as seams, exactly the ones the C port takes as parameters or drops:

  * ``_session_lookup_key`` -- the real helper from tui_gateway.session_registry
    (agent.session_id or session_key or fallback), so the identity key is the
    production one;
  * ``time`` -- a fake whose ``sleep`` records the delay instead of waiting
    (``_notif_refusal_backoff`` is called for real; only the wait is a seam);
  * registry/lock/lineage-DB/queue/emitter -- never touched by this half.

The session record's ``agent`` and the loop ``state`` arrive as records; they
are rebuilt as attribute objects here because the live code uses getattr.
"""
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_sessionnot_c045cd_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tui_gateway import session_notifications as live  # noqa: E402
from tui_gateway.session_registry import _session_lookup_key  # noqa: E402

# seam 1: the production identity-key helper (a sibling module, no server state).
live._session_lookup_key = _session_lookup_key


class _FakeTime:
    """seam 2: the refusal backoff's wait, recorded instead of slept."""

    slept = []

    def sleep(self, seconds):
        _FakeTime.slept.append(seconds)


live.time = _FakeTime()

WANTED = {"current_keys", "session_matches", "requires_owner", "dedup_key",
          "refusal_backoff", "loop_route", "process_owners", "constants"}


def _session(rec):
    """Rebuild the session record: ``agent`` is an attribute object upstream."""
    rec = dict(rec) if isinstance(rec, dict) else {}
    agent = rec.get("agent")
    if isinstance(agent, dict):
        rec["agent"] = types.SimpleNamespace(**agent)
    return rec


def _state(rec):
    rec = rec if isinstance(rec, dict) else {}
    state = types.SimpleNamespace()
    state.route = rec.get("route")
    return state


def refusal_backoff(req):
    session = dict(req.get("session") or {})
    _FakeTime.slept = []
    live._notif_refusal_backoff(session)
    return {"delay": _FakeTime.slept[-1], "next": session["_notif_refusal_delay"]}


def constants():
    out = {
        "kanban_notify_kinds": list(live._KANBAN_NOTIFY_KINDS),
        "kanban_poll_seconds": float(live._KANBAN_POLL_SECONDS),
        "loop_poll_seconds": float(live._LOOP_POLL_SECONDS),
        "refusal_backoff_s": float(live._NOTIF_REFUSAL_BACKOFF_S),
        "refusal_backoff_max_s": float(live._NOTIF_REFUSAL_BACKOFF_MAX_S),
        "dedup_extra_fields": {k: list(v) for k, v in live._DEDUP_EXTRA_FIELDS.items()},
    }
    return out


def expected(r):
    f = r.get("func")
    if f == "current_keys":
        keys = live._notif_current_keys(r.get("sid"), _session(r.get("session")))
        return sorted(keys)
    if f == "session_matches":
        keys = set(r.get("keys") or [])
        return bool(live._notif_session_matches(_session(r.get("session")), keys))
    if f == "requires_owner":
        return bool(live._notification_event_requires_owner(r.get("evt") or {}))
    if f == "dedup_key":
        return list(live._notification_event_dedup_key(r.get("evt") or {}))
    if f == "refusal_backoff":
        return refusal_backoff(r)
    if f == "loop_route":
        return bool(live._loop_route_is_gateway_chat(_state(r.get("state"))))
    if f == "process_owners":
        return sorted(live._session_process_owners(_session(r.get("session"))))
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
