#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_dice.py
(gate w22_telegramdi_2d8019).

The adapter is a PTB object (network/update seam), so the oracle runs the live
coroutine against a *recording* adapter. Everything the module decides is live:

  handle -- await `handle_dice_message(adapter, update, context)` and report the
            adapter transcript plus the event that was built:
              calls  one of
                       []                                             (dice None,
                                                                       or neither
                                                                       processed
                                                                       nor observed)
                       ["log_blocked_user"]                           (unauthorized)
                       ["build_message_event", "observe"]              (observed)
                       ["build_message_event", "apply_attribution",
                        "handle_message"]                             (handled)
              event  {"text", "kind", "update_id", "attributed"}
            A missing `dice.emoji` / `dice.value` or `update.update_id` raises
            AttributeError exactly where the source dereferences it.
  text   -- the rendered roll line on its own.

Fixture contract: `msg` is the message object (any non-object reads as None and
so has no dice), `dice` is that message's dice attribute, and authorized /
proceed / observe are the adapter hook results (Python truthiness).
"""
import asyncio
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "platforms" / "telegram" / "telegram_dice.py"
GATE = "w22_telegramdi_2d8019"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("telegram_dice_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["telegram_dice_live"] = live
_spec.loader.exec_module(live)

WANTED = {"handle", "text"}


class FakeEvent:
    def __init__(self, kind, update_id):
        self.kind = kind
        self.update_id = update_id
        self.text = None
        self.attributed = False


class FakeAdapter:
    """Records the hooks the dice handler calls; nothing leaves the process."""

    def __init__(self, msg, authorized, proceed, observe):
        self.msg = msg
        self.authorized = authorized
        self.proceed = proceed
        self.observe = observe
        self.calls = []
        self.events = []

    # --- update / message hooks -------------------------------------------
    def _effective_update_message(self, _update):
        return self.msg

    def _is_user_authorized_from_message(self, _msg):
        return self.authorized

    def _log_blocked_user(self, _msg):
        self.calls.append("log_blocked_user")

    def _should_process_message(self, _msg):
        return self.proceed

    def _should_observe_unmentioned_group_message(self, _msg):
        return self.observe

    # --- event hooks -------------------------------------------------------
    def _build_message_event(self, _msg, message_type, update_id=None):
        self.calls.append("build_message_event")
        event = FakeEvent(message_type, update_id)
        self.events.append(event)
        return event

    def _observe_unmentioned_group_message(self, _msg, _message_type, update_id=None, event=None):
        self.calls.append("observe")

    def _apply_telegram_group_observe_attribution(self, event):
        self.calls.append("apply_attribution")
        event.attributed = True
        return event

    async def handle_message(self, _event):
        self.calls.append("handle_message")


def message_from(spec):
    """The object `_effective_update_message` returns: None unless it is a message."""
    if isinstance(spec, dict):
        return types.SimpleNamespace()
    return None


def dice_attr(spec):
    if isinstance(spec, dict):
        return types.SimpleNamespace(**spec)
    return spec


def handle_case(r):
    msg = message_from(r.get("msg"))
    if msg is not None:
        dice = r.get("dice")
        if dice is not None:
            msg.dice = dice_attr(dice)

    update = types.SimpleNamespace()
    if "update_id" in r:
        update.update_id = r.get("update_id")

    adapter = FakeAdapter(msg, r.get("authorized"), r.get("proceed"), r.get("observe"))

    async def run():
        await live.handle_dice_message(adapter, update, None)

    try:
        asyncio.run(run())
    except Exception as exc:  # noqa: BLE001
        return {"raised": exc.__class__.__name__}

    event = adapter.events[0] if adapter.events else None
    payload = None
    if event is not None:
        payload = {"text": event.text,
                   "kind": event.kind.name,
                   "update_id": event.update_id,
                   "attributed": bool(getattr(event, "attributed", False))}
    return {"calls": adapter.calls, "event": payload}


def text_case(emoji, value):
    class Msg:
        dice = types.SimpleNamespace(emoji=emoji, value=value)

    adapter = FakeAdapter(Msg(), True, True, False)
    update = types.SimpleNamespace(update_id=0)

    async def run():
        await live.handle_dice_message(adapter, update, None)

    asyncio.run(run())
    return adapter.events[0].text


def expected(r):
    f = r.get("func")
    if f == "handle":
        return handle_case(r)
    if f == "text":
        return text_case(r.get("emoji"), r.get("value"))
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
