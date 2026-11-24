#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_typing_gate.py
(gate w21_telegramty_a78c8c).

Executes the LIVE module: `typing_may_fire` / `_typing_in_cooldown` are the real
functions, `claim_ai_action` and `normalize_telegram_chat_id` are the real
collaborators imported from their own live modules. Two seams are injected:

* the loop clock -- `asyncio.get_running_loop().time()` is replaced by a fake
  loop returning the fixture's `now`, so both sides compare the same instant;
* the adapter -- a scripted object whose `__dict__` holds the three per-chat
  dicts and whose two cooldown/slot readers answer from the fixture state.

Expected "state" is read back off that live adapter after the call, which is
exactly the state the C port hands back.
"""
import asyncio
import json
import sys
import types
from collections import deque
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = Path("C:/Users/eman5/hermes-agent")
SRC = QUARRY / "plugins" / "platforms" / "telegram" / "telegram_typing_gate.py"
GATE = "w21_telegramty_a78c8c"


def _pkg(name, path):
    mod = types.ModuleType(name)
    mod.__path__ = [str(path)]
    sys.modules[name] = mod
    return mod


# Register the parent packages by path so the quarry's plugin __init__ (which
# pulls the whole platform stack) is never executed: only the two sibling
# modules the gate actually uses get imported.
for _name, _path in (
    ("plugins", QUARRY / "plugins"),
    ("plugins.platforms", QUARRY / "plugins" / "platforms"),
    ("plugins.platforms.telegram", QUARRY / "plugins" / "platforms" / "telegram"),
    ("gateway", QUARRY / "gateway"),
    ("gateway.platforms", QUARRY / "gateway" / "platforms"),
):
    if _name not in sys.modules:
        _pkg(_name, _path)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import importlib.util

_spec = importlib.util.spec_from_file_location("telegram_typing_gate_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["telegram_typing_gate_live"] = live
_spec.loader.exec_module(live)

NOW = {"v": 0.0}


class _Loop:
    def time(self):
        return NOW["v"]


asyncio.get_running_loop = lambda: _Loop()


class Adapter:
    def __init__(self, state):
        self._flood = state.get("flood_remaining")
        self._slot = state.get("slot_remaining", 0.0)
        if isinstance(state.get("cooldown_until"), dict):
            self.__dict__["_telegram_typing_cooldown_until"] = dict(state["cooldown_until"])
        if isinstance(state.get("last_sent"), dict):
            self.__dict__["_telegram_typing_last_sent"] = dict(state["last_sent"])
        if isinstance(state.get("ai_calls"), dict):
            self.__dict__["_telegram_ai_action_calls"] = {
                str(k): deque(v) for k, v in state["ai_calls"].items() if isinstance(v, list)}

    def _send_flood_cooldown_remaining(self, chat_id):
        return self._flood

    def _chat_outbound_slot_remaining(self, chat_id):
        return self._slot


def snapshot(ad):
    return {
        "cooldown_until": dict(ad.__dict__.get("_telegram_typing_cooldown_until", {})),
        "last_sent": dict(ad.__dict__.get("_telegram_typing_last_sent", {})),
        "ai_calls": {k: list(q) for k, q in ad.__dict__.get("_telegram_ai_action_calls", {}).items()},
    }


def expected(r):
    func = r.get("func")
    NOW["v"] = float(r.get("now", 0.0) or 0.0)
    ad = Adapter(r.get("state") or {})
    chat_id = r.get("chat_id")
    if func == "in_cooldown":
        result = live._typing_in_cooldown(ad, chat_id)
    elif func == "may_fire":
        result = live.typing_may_fire(ad, chat_id)
    else:
        raise AssertionError("unknown func %r" % (func,))
    return {"result": bool(result), "state": snapshot(ad)}


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
        if not isinstance(r, dict) or r.get("func") not in ("in_cooldown", "may_fire"):
            continue
        cases += 1
        try:
            py = expected(r)
        except BaseException as exc:  # noqa: BLE001 - reported, never fatal
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
