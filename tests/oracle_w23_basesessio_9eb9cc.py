#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/base_session_command.py (gate
w23_basesessio_9eb9cc).

``dispatch_active_session_command`` is the "/stop" / "/new" / "/reset" path that
bypasses the active-session guard.  It talks to a platform adapter, so the
adapter is the seam: this oracle runs the live coroutine against a recording
adapter built from the fixture, with real ``asyncio.Event`` guards standing in
for the adapter's guards (the module keeps them in a ``WeakKeyDictionary`` and a
``WeakSet``, so they must be real, identity-hashed objects).

The fixture names its guards; the oracle maps names to events in both
directions, so the answer is written in names: the sequence of adapter calls the
live coroutine made, which guard the session is left under, whether the held
text went back into the store, and how many guards the in-flight set still
holds.

``guard_under_commands`` and ``command_in_flight`` run live over the same guard
ledger.
"""
import asyncio
import importlib.util
import json
import sys
import types
import weakref
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "gateway" / "platforms" / "base_session_command.py"
GATE = "w23_basesessio_9eb9cc"
COMMAND = "<command>"
KEY = "s1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("base_session_command_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["base_session_command_live"] = live
_spec.loader.exec_module(live)

WANTED = {"guard_under", "in_flight", "dispatch"}


class Guard(asyncio.Event):
    """A command guard: identity is all the ledger keys on."""

    def __init__(self, name):
        super().__init__()
        self.guard_name = name


class Ledger:
    """The fixture's guard world: name <-> event, plus the module ledgers."""

    def __init__(self, world):
        self.world = world
        self.by_name = {}
        live._replaced_guard = weakref.WeakKeyDictionary()
        live._in_flight = weakref.WeakSet()
        for pair in world.get("replaced") or []:
            if not isinstance(pair, (list, tuple)) or len(pair) < 2:
                continue
            child, parent = pair[0], pair[1]
            live._replaced_guard[self.guard(child)] = self.guard(parent)
        for name in world.get("in_flight") or []:
            live._in_flight.add(self.guard(name))

    def guard(self, name):
        if not isinstance(name, str):
            return None
        if name not in self.by_name:
            self.by_name[name] = Guard(name)
        return self.by_name[name]

    def name_of(self, guard):
        if guard is None:
            return None
        for name, made in self.by_name.items():
            if made is guard:
                return name
        # the only event the oracle did not make is the command's own guard
        return COMMAND


class Held:
    def __init__(self, calls):
        self.calls = calls

    def cancel_timer(self):
        self.calls.append("cancel_timer")


def guard_under(raw):
    world = raw if isinstance(raw, dict) else {}
    ledger = Ledger(world)
    return ledger.name_of(live.guard_under_commands(ledger.guard(world.get("guard"))))


def in_flight(raw):
    world = raw if isinstance(raw, dict) else {}
    ledger = Ledger(world)
    return bool(live.command_in_flight(ledger.guard(world.get("guard"))))


class Adapter:
    """Records what the command asked for; owns the per-session state."""

    name = "fake"

    def __init__(self, world, ledger):
        self._world = world
        self._ledger = ledger
        self.calls = []
        self.held = None
        self._active_sessions = {}
        self._session_tasks = {}
        self._store = {}
        self._authorization_check = object() if world.get("auth_check") else None
        current = world.get("current_guard")
        if isinstance(current, str):
            self._active_sessions[KEY] = ledger.guard(current)
        if world.get("in_session_tasks"):
            self._session_tasks[KEY] = object()
        if world.get("held"):
            self.held = Held(self.calls)
            self._store[KEY] = self.held

    def _text_debounce_store(self):
        return self._store

    async def _dispatch_inline_reply(self, event, log_cmd=None):
        self.calls.append("dispatch_inline_reply")
        newer = self._world.get("newer_owner")
        if isinstance(newer, str):
            self._active_sessions[KEY] = self._ledger.guard(newer)

    def _discard_text_debounce(self, session_key):
        self.calls.append("discard_text_debounce")

    async def cancel_session_processing(self, session_key, release_guard=False,
                                        discard_pending=False):
        self.calls.append("cancel_session_processing")

    async def _drain_pending_after_session_command(self, session_key, guard):
        self.calls.append("drain_pending")

    async def _flush_text_debounce_now(self, session_key):
        self.calls.append("flush_text_debounce_now")

    def _session_task_is_stale(self, session_key):
        self.calls.append("session_task_is_stale")
        return bool(self._world.get("task_stale"))


def dispatch(raw):
    world = raw if isinstance(raw, dict) else {}
    ledger = Ledger(world)
    adapter = Adapter(world, ledger)
    event = types.SimpleNamespace(_interrupt_accepted=world.get("interrupt_accepted", False))
    asyncio.run(live.dispatch_active_session_command(adapter, event, KEY, "stop"))
    return {
        "calls": list(adapter.calls),
        "active": ledger.name_of(adapter._active_sessions.get(KEY)),
        "held_restored": adapter.held is not None and adapter._store.get(KEY) is adapter.held,
        "in_flight_after": len(live._in_flight),
    }


def expected(r):
    f = r.get("func")
    if f == "guard_under":
        return guard_under(r.get("world"))
    if f == "in_flight":
        return in_flight(r.get("world"))
    if f == "dispatch":
        return dispatch(r.get("world"))
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
