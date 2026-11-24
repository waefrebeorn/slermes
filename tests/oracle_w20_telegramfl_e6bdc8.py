#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_flood_cooldown.py
(gate w20_telegramfl_e6bdc8).

The live module is imported against the real `telegram_ids` helper and a
stubbed `plugins.platforms.telegram.adapter._flood_cap_result` / SendResult, so
the only seams are the send-result constructor and the clocks: `asyncio` and
`time` are replaced on the live module namespace by fixed-clock stand-ins, and
`_update_receipt_dir` points at a temp dir holding the persisted window file the
fixture describes. Compared values: _bot_id, the store file name, the seeding
_windows performs, the arm _record_send_flood_cooldown does, and
_send_flood_cooldown_remaining's answer plus the surviving map.
"""
import importlib.util
import json
import os
import shutil
import sys
import tempfile
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = list(attrs.pop("__path__", []))
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class SendResult:
    def __init__(self, success=True, message_id=None, error=None, raw_response=None,
                 retryable=False, retry_after=None, continuation_message_ids=(),
                 error_kind=None):
        self.success = success
        self.message_id = message_id
        self.error = error
        self.retry_after = retry_after


def _flood_cap_result(wait):
    return SendResult(success=False, error="flood_control:%s" % (wait,), retry_after=float(wait))


_mk("gateway", __path__=[os.path.join(Q, "gateway")])
_mk("gateway.platforms", __path__=[os.path.join(Q, "gateway", "platforms")])
_mk("gateway.platforms.base", SendResult=SendResult)
_mk("plugins", __path__=[os.path.join(Q, "plugins")])
_mk("plugins.platforms", __path__=[os.path.join(Q, "plugins", "platforms")])
_mk("plugins.platforms.telegram", __path__=[os.path.join(Q, "plugins", "platforms", "telegram")])
_mk("plugins.platforms.telegram.adapter", _flood_cap_result=_flood_cap_result)

NAME = "w20_telegramfl_e6bdc8_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "plugins", "platforms", "telegram", "telegram_flood_cooldown.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


class FakeTask:
    def done(self):
        return False


class FakeHandle:
    def __init__(self, cb):
        self._cb = cb
        self.cancelled = False


class FakeLoop:
    def __init__(self, now):
        self.now = now
        self.tasks = []

    def time(self):
        return self.now

    def create_task(self, coro, name=None):
        coro.close()
        task = FakeTask()
        self.tasks.append(task)
        return task

    def call_later(self, delay, cb):
        return FakeHandle(cb)


class Adapter:
    pass


def make_adapter(token, has_dir, tmp, preset, loaded):
    ad = Adapter()
    ad.config = types.SimpleNamespace(token=token) if token is not None else types.SimpleNamespace(token=None)
    ad._update_receipt_dir = Path(tmp) if has_dir else None
    ad._telegram_send_cooldown_until = dict(preset or {})
    ad._telegram_send_cooldown_loaded = bool(loaded)
    return ad


def expected(r, tmp):
    f = r.get("func")
    if f == "bot_id":
        ad = types.SimpleNamespace(config=types.SimpleNamespace(token=r.get("token")))
        return live._bot_id(ad)
    if f == "store_name":
        ad = make_adapter(r.get("token"), bool(r.get("has_receipt_dir")), tmp, {}, False)
        path = live._store_path(ad)
        return path.name if path is not None else None
    if f == "seed":
        loop = FakeLoop(float(r.get("loop_now", 0)))
        live.asyncio = types.SimpleNamespace(get_running_loop=lambda: loop)
        live.time = types.SimpleNamespace(time=lambda: float(r.get("wall_now", 0)))
        ad = make_adapter(r.get("token", "123:b"), True, tmp, r.get("preset"), r.get("loaded"))
        text = r.get("saved_text")
        if text is not None:
            path = Path(tmp) / ("telegram_flood_cooldown_%s.json" % live._bot_id(ad))
            path.write_text(text, encoding="utf-8")
        return dict(live._windows(ad))
    if f == "record":
        loop = FakeLoop(float(r.get("loop_now", 0)))
        live.asyncio = types.SimpleNamespace(get_running_loop=lambda: loop)
        ad = make_adapter(r.get("token", "123:b"), False, tmp, r.get("preset"), True)
        chat = r.get("chat")
        result = live.TelegramFloodCooldownMixin._record_send_flood_cooldown(
            ad, chat, float(r.get("wait")))
        key = str(live.normalize_telegram_chat_id(chat))
        until = ad.__dict__["_telegram_send_cooldown_until"]
        return {"key": key, "deadline": until.get(key), "until": dict(until),
                "result": {"success": result.success, "error": result.error,
                           "retry_after": result.retry_after}}
    if f == "remaining":
        loop = FakeLoop(float(r.get("loop_now", 0)))
        live.asyncio = types.SimpleNamespace(get_running_loop=lambda: loop)
        ad = make_adapter(r.get("token", "123:b"), False, tmp, r.get("until"), True)
        remaining = live.TelegramFloodCooldownMixin._send_flood_cooldown_remaining(
            ad, r.get("chat"))
        return {"remaining": remaining,
                "until": dict(ad.__dict__["_telegram_send_cooldown_until"])}
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    tmp = tempfile.mkdtemp(prefix="tf6_")
    try:
        exp = expected(r, tmp)
    except Exception as exc:  # noqa: BLE001 - report the live failure verbatim
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        shutil.rmtree(tmp, ignore_errors=True)
        continue
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_telegramfl_e6bdc8: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
