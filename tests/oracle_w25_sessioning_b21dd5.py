#!/usr/bin/env python3
"""Differential oracle for gateway/session_ingress_live_turn.py (gate w25_sessioning_b21dd5).

The runner is the seam: the turn registry, the clarify and slash-confirm stores and
the agent's own turn answer with the fixture's fixed values, so the live module's
own decisions are what gets compared.  The module-level consumed ledger is
re-seeded per case from ``consumed_before`` / ``consumed``.
"""
import asyncio
import json
import sys
from collections import OrderedDict
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_sessioning_b21dd5"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from gateway import session_ingress_live_turn as live  # noqa: E402
from tools import clarify_gateway, slash_confirm  # noqa: E402

WANTED = {"may_answer", "replay_key", "answers", "consumed", "constants"}
ABSENT = object()


class Event:
    """The delivery as the module reads it: message id, source, command, internal."""

    def __init__(self, req):
        self.message_id = req.get("message_id")
        self.internal = req.get("internal", False)
        self._command = req.get("command")
        source = SimpleNamespace(
            platform=SimpleNamespace(value=req.get("platform")),
            chat_id=req.get("chat_id"),
            is_bot=req.get("is_bot", False),
        )
        if "profile" in req:
            source.profile = req["profile"]
        self.source = source
        self._consumer_reply = ABSENT

    def get_command(self):
        return self._command


def _awaiting(value):
    async def call(*args, **kwargs):
        return value
    return call


def may_answer(req):
    state = req.get("state")

    def peek(key):
        if state is None:
            return None
        return SimpleNamespace(
            persistent=SimpleNamespace(update_prompt_pending=state.get("update_prompt_pending")))

    runner = SimpleNamespace(
        _session_key_for_source=lambda source: "k",
        _is_session_running=lambda key: req.get("running", False),
        _peek_session_state=peek,
    )
    saved = (clarify_gateway.has_pending, slash_confirm.get_pending)
    clarify_gateway.has_pending = lambda key: req.get("clarify_pending", False)
    slash_confirm.get_pending = lambda key: req.get("slash_pending")
    try:
        return live.may_answer_live_turn(runner, SimpleNamespace(source=None))
    finally:
        clarify_gateway.has_pending, slash_confirm.get_pending = saved


def replay_key(req):
    key = live._replay_key(Event(req))
    return list(key) if key is not None else None


def answers(req):
    runner = SimpleNamespace(
        _session_key_for_source=lambda source: "k",
        _is_session_running=lambda key: req.get("running", False),
        _hm_pending_reply_intercepts=_awaiting(req.get("reply")),
        _route_plaintext_approval_while_busy=_awaiting(req.get("approval", False)),
        _hm_busy_preempt=_awaiting(req.get("preempt", False)),
    )
    event = Event(req)
    live._consumed = OrderedDict((tuple(k), None) for k in (req.get("consumed_before") or []))
    got = asyncio.run(live.answers_live_turn(runner, event))
    reply = None if event._consumer_reply is ABSENT else event._consumer_reply
    return {"consumed": got, "consumer_reply": reply,
            "keys": [list(k) for k in live._consumed]}


def consumed(req):
    event = Event(req)
    live._consumed = OrderedDict((tuple(k), None) for k in (req.get("consumed") or []))
    return live.was_consumed(event)


def constants():
    return {"consumed_max": live._CONSUMED_MAX}


def expected(r):
    f = r.get("func")
    if f == "may_answer":
        return may_answer(r)
    if f == "replay_key":
        return replay_key(r)
    if f == "answers":
        return answers(r)
    if f == "consumed":
        return consumed(r)
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
