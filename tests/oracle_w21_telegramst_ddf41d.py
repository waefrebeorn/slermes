#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_status.py
(gate w21_telegramst_ddf41d).

The live coroutine runs unmodified: the module is imported from its real source
with only `gateway.platforms.base` stubbed (SendResult is annotation-only under
`from __future__ import annotations`, so the class is never constructed), and a
scripted adapter answers `edit_message`/`send` from the fixture. Both sides
produce the SendResult, the resulting id map in dict order, whether the lock was
created and the ordered adapter calls.
"""
import asyncio
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GATE = "w21_telegramst_ddf41d"
SRC = os.path.join(QUARRY, "plugins", "platforms", "telegram", "telegram_status.py")


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class SendResult:  # annotation-only upstream; never built by the coroutine
    def __init__(self, success=True, message_id=None):
        self.success = success
        self.message_id = message_id


_mk("gateway")
_mk("gateway.platforms")
_mk("gateway.platforms.base", SendResult=SendResult)

NAME = GATE + "_live"
_sp = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(_sp)
sys.modules[NAME] = live
_sp.loader.exec_module(live)


class Result:
    def __init__(self, spec):
        spec = spec if isinstance(spec, dict) else {}
        self.success = bool(spec.get("success"))
        self.message_id = spec.get("message_id")


class Adapter:
    """Scripted adapter: the answers and the mid-await mutation come from the case."""

    def __init__(self, case):
        self.max_ids = case.get("max", 2000)
        self._STATUS_MESSAGE_IDS_MAX = self.max_ids
        self.triples = [(str(t[0]), str(t[1]), t[2]) for t in (case.get("message_ids") or [])]
        self._status_message_ids = {(c, k): v for c, k, v in self.triples}
        self._status_locks = {}
        self.edit_spec = case.get("edit")
        self.send_spec = case.get("send")
        self.during = case.get("during_edit")
        self.ops = []
        self.key = None

    async def edit_message(self, chat_id, message_id, content, finalize=True, metadata=None):
        self.ops.append("edit_message")
        if self.during == "replace":
            self._status_message_ids[(str(chat_id), self.key)] = "other"
        elif self.during == "pop":
            self._status_message_ids.pop((str(chat_id), self.key), None)
        return Result(self.edit_spec)

    async def send(self, chat_id, content, metadata=None):
        self.ops.append("send")
        return Result(self.send_spec)


async def run_case(case):
    ad = Adapter(case)
    chat, key = case.get("chat_id"), case.get("status_key")
    ad.key = str(key)
    pair = (str(chat), str(key))
    if case.get("lock_exists"):
        ad._status_locks[pair] = asyncio.Lock()
    created = pair not in ad._status_locks
    res = await live.send_or_update_status(ad, chat, key, case.get("content", ""), metadata=None)
    return {"result": {"success": res.success, "message_id": res.message_id},
            "ids": [[c, k, v] for (c, k), v in ad._status_message_ids.items()],
            "lock_created": created,
            "ops": list(ad.ops)}


def expected(r):
    f = r.get("func")
    if f == "update":
        return asyncio.run(run_case(r))
    if f == "str":
        return str(r.get("value"))
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"update", "str"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        py = expected(r)
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
