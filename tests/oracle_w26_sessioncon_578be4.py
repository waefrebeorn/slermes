#!/usr/bin/env python3
"""Differential oracle for gateway/session_control_actions.py (gate w26_sessioncon_578be4).

The authority rows, the goal manager, ``execute_command`` and the event stream
are the seam.  This oracle drives the module's own decision points on the
fixture values:

  * ``_validate`` (raising AuthorityRefusal is reported as its wire reason);
  * the action tables (``_ACTION_COMMANDS`` / ``_SUBGOAL_ACTIONS`` / ``_ENVELOPE``);
  * ``_publish_snapshot`` against a stand-in live session whose event stream only
    records that a publish happened -- the ordering guard is the real code.
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
GATE = "w26_sessioncon_578be4"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
os.environ.setdefault("USERPROFILE", str(QUARRY.parent))
os.environ.setdefault("HOME", str(QUARRY.parent))

from gateway import session_control_actions as live  # noqa: E402
from persistence.core.errors import AuthorityRefusal  # noqa: E402

WANTED = {"validate", "dispatch", "envelope", "publish_decision", "constants"}


class _Stream:
    """Records published events; the event bus itself is the seam."""

    def __init__(self):
        self.events = []

    def publish(self, session_id, payload, event_type=None):
        self.events.append((session_id, payload, event_type))


def validate(req):
    try:
        live._validate(req.get("action"), req.get("args"))
    except AuthorityRefusal as exc:
        return exc.reason
    return "ok"


def dispatch(req):
    action = req.get("action")
    if action in live._SUBGOAL_ACTIONS:
        return ["subgoal"]
    if action in live._ACTION_COMMANDS:
        name, arg = live._ACTION_COMMANDS[action]
        return [name, arg]
    return None


def envelope(req):
    result = req.get("result")
    result = result if isinstance(result, dict) else {}
    return {key: result.get(key) for key in live._ENVELOPE}


def publish_decision(req):
    ticket = req.get("ticket")
    ticket = int(ticket) if isinstance(ticket, (int, float)) else 0
    seq = req.get("control_published_seq")
    seq = int(seq) if isinstance(seq, (int, float)) else 0
    stream = _Stream()
    session = SimpleNamespace(
        control_published_seq=seq,
        control_revision=req.get("control_revision"),
        event_stream=stream,
    )
    snapshot = {"revision": req.get("snapshot_revision")}
    live._publish_snapshot(
        None, session, "s-1", snapshot, ticket,
        only_if_present=bool(req.get("only_if_present")),
        only_if_changed=bool(req.get("only_if_changed")),
    )
    return {
        "published": bool(stream.events),
        "control_published_seq": session.control_published_seq,
        "control_revision": session.control_revision,
    }


def constants():
    return {
        "action_commands": {k: list(v) for k, v in live._ACTION_COMMANDS.items()},
        "subgoal_actions": sorted(live._SUBGOAL_ACTIONS),
        "envelope": list(live._ENVELOPE),
    }


def expected(r):
    f = r.get("func")
    if f == "validate":
        return validate(r)
    if f == "dispatch":
        return dispatch(r)
    if f == "envelope":
        return envelope(r)
    if f == "publish_decision":
        return publish_decision(r)
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
