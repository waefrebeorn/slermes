#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_topic_transfer.py
(gate w25_telegramto_67ae35).

The live coroutine is driven for real with ``asyncio.run``: the adapter, the
awaited ``_topic_transfer_fn`` hook, the dm-topic lookup and
``resolve_channel_prompt`` are the seams the fixture answers.  A real
``SessionSource`` (so ``replace_source``/``dataclasses.replace`` behaves) and a
thin event stand-in (so ``get_command`` reports the fixture's parsed command)
are used, and the result is read off the event *after* the call -- which is
exactly what the runner would see.
"""
import asyncio
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_telegramto_67ae35"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from gateway.config import Platform  # noqa: E402
from plugins.platforms.telegram import telegram_topic_transfer as live  # noqa: E402
from gateway.session import SessionSource  # noqa: E402
from gateway.session_identity import replace_source  # noqa: E402

WANTED = {"transfer", "prompt_inputs", "recovery"}
CASE = {}


class Event:
    """The slice of MessageEvent this module touches."""

    def __init__(self, source, internal=False, metadata=None, command=None):
        self.text = command or "hello"
        self.source = source
        self.internal = internal
        self.metadata = metadata or {}
        self.reply_anchor_override = None
        self.auto_skill = None
        self.channel_prompt = None
        self.allow_gateway_control = False
        self._command = command

    def get_command(self):
        return self._command


class Adapter:
    def __init__(self, transfer, authorized, topic):
        self._topic_transfer_fn = transfer
        self._authorized = authorized
        self._topic = topic
        self.topic_calls = []
        self.config = types.SimpleNamespace(extra={})

    def _is_sender_authorized(self, user_id, chat_type, chat_id):
        return self._authorized

    def _get_dm_topic_info(self, chat_id, thread_id):
        self.topic_calls.append((chat_id, thread_id))
        return self._topic


def make_source(spec):
    if not isinstance(spec, dict):
        return None
    return SessionSource(
        platform=Platform.TELEGRAM,
        chat_id=str(spec.get("chat_id") if spec.get("chat_id") is not None else "1"),
        chat_type=spec.get("chat_type") or "dm",
        user_id=spec.get("user_id"),
        thread_id=spec.get("thread_id"),
        message_id=spec.get("message_id"),
    )


def make_transfer(decided):
    async def hook(source):
        if not isinstance(decided, dict):
            return None
        if decided.get("raise"):
            raise RuntimeError("oracle: topic creation failed")
        return (decided.get("thread_id"), bool(decided.get("created")))
    return hook


def _install():
    live.coerce_plaintext_gateway_command = lambda event: None
    live.resolve_channel_prompt = (
        lambda extra, thread_id, chat_id: CASE.get("channel_prompt"))


def transfer(req):
    CASE.clear()
    CASE.update(req)
    _install()
    source = make_source(req.get("source"))
    hook = make_transfer(req.get("decided")) if req.get("has_transfer") else None
    adapter = Adapter(hook, req.get("authorized"), req.get("topic"))
    event = Event(source,
                  internal=bool(req.get("internal")),
                  metadata=req.get("metadata"),
                  command=req.get("command"))
    asyncio.run(live.apply_root_dm_topic_transfer(adapter, event))
    moved = event.source is not source
    if not moved:
        return {"moved": False, "thread_id": None, "message_id": None,
                "reply_anchor_override": None, "prompt_applied": False}
    return {
        "moved": True,
        "thread_id": event.source.thread_id,
        "message_id": event.source.message_id,
        "reply_anchor_override": event.reply_anchor_override,
        "prompt_applied": bool(adapter.topic_calls),
    }


def prompt_inputs(req):
    CASE.clear()
    CASE.update(req)
    _install()
    source = make_source({"chat_id": req.get("chat_id"), "thread_id": req.get("thread_id")})
    adapter = Adapter(None, None, req.get("topic"))
    event = Event(source)
    live.apply_topic_prompt_inputs(adapter, event)
    return {
        "chat_topic": event.source.chat_topic,
        "auto_skill": event.auto_skill,
        "channel_prompt": event.channel_prompt,
    }


def recovery(req):
    CASE.clear()
    CASE.update(req)
    _install()
    source = make_source({"chat_id": req.get("chat_id"), "thread_id": req.get("before_thread")})
    adapter = Adapter(None, None, req.get("topic"))
    event = Event(source)

    def recover(ev):
        ev.source = replace_source(ev.source, thread_id=req.get("after_thread"))

    live.apply_topic_recovery_with_inputs(adapter, event, recover)
    applied = bool(adapter.topic_calls)
    return {
        "applied": applied,
        "chat_topic": event.source.chat_topic if applied else None,
    }


def expected(req):
    func = req.get("func")
    if func == "transfer":
        return transfer(req)
    if func == "prompt_inputs":
        return prompt_inputs(req)
    if func == "recovery":
        return recovery(req)
    raise AssertionError("unknown func %r" % (func,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except ValueError:
            continue
        if not isinstance(req, dict) or req.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(req)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (req.get("func"), req.get("case"), exc), file=sys.stderr)
            continue
        got = req.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (req.get("func"), req.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
