#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_voice_mention.py
(gate w21_telegramvo_2f1ab7).

The live module is executed as-is; only its I/O collaborators are replaced:
``gateway.platforms.helpers.spoken_mention_clips`` (STT) and
``gateway.platforms.base.cache_audio_from_bytes_async`` (audio cache) are
patched before the module binds them, and ``fetch_telegram_file`` is injected
into ``sys.modules`` so the in-function import resolves without a network.
Every fixture line builds the adapter/message pair its ``world`` describes and
compares the live coroutine's result to the C port's ``out``.
"""
import asyncio
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

import gateway.platforms.base as gbase  # noqa: E402
import gateway.platforms.helpers as ghelp  # noqa: E402


class _FetchFailed(Exception):
    pass


async def _fetch_telegram_file(voice, label):
    return types.SimpleNamespace(file_id="f1"), b"OGGDATA"


async def _cache_audio(data, ext=None):
    return "/cache/group-voice.ogg"


async def _spoken_clips(paths, patterns, *, adapter=None, source=None, keep=frozenset()):
    return list(CLIPS)


CLIPS = []
gbase.cache_audio_from_bytes_async = _cache_audio
ghelp.spoken_mention_clips = _spoken_clips

_mk = types.ModuleType("plugins.platforms.telegram.telegram_media_fetch")
_mk.__path__ = []
_mk.fetch_telegram_file = _fetch_telegram_file
for _name in ("plugins", "plugins.platforms", "plugins.platforms.telegram"):
    if _name not in sys.modules:
        _pkg = types.ModuleType(_name)
        _pkg.__path__ = []
        sys.modules[_name] = _pkg
sys.modules["plugins.platforms.telegram.telegram_media_fetch"] = _mk

SRC = os.path.join(Q, "plugins", "platforms", "telegram", "telegram_voice_mention.py")
_spec = importlib.util.spec_from_file_location("w21_tvm_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["w21_tvm_live"] = live
_spec.loader.exec_module(live)


class Config:
    def __init__(self, extra):
        self.extra = extra


class Msg:
    def __init__(self, world):
        self.voice = object() if world.get("has_voice") else None
        if "caption" in world:
            self.caption = world.get("caption")
        self.message_id = 7


class Adapter:
    name = "telegram"

    def __init__(self, world):
        self.world = world
        self._mention_patterns = ["hey hermes"] if world.get("mention_patterns") else []
        gate = {"voice_mention_patterns": world["gate_extra"]} if "gate_extra" in world else {}
        self.config = Config(gate)
        self.cached = []

    def _should_process_message(self, msg, spoken_mention=False):
        return bool(self.world.get("spoken_ok") if spoken_mention
                    else self.world.get("should_process"))

    def _telegram_media_size_allowed(self, voice, label):
        return bool(self.world.get("media_allowed")), ""

    def _build_message_event(self, msg, mtype):
        return types.SimpleNamespace(source="group")

    async def _cache_inbound_av(self, msg, event, voice, label, kind, ext, mime):
        self.cached.append((label, kind, ext, mime))


class Event:
    pass


def canon(v):
    if v is None:
        return None
    return list(v)


def expected(r):
    f = r.get("func")
    world = r.get("world") if isinstance(r.get("world"), dict) else {}
    if f == "env_probe":
        return {"sler": bool(os.environ.get("SLERMES_HOME")),
                "home": bool(os.environ.get("HOME")),
                "same": bool(os.environ.get("SLERMES_HOME")) and
                        os.environ.get("SLERMES_HOME") == os.environ.get("HOME")}
    if f == "admit_media":
        global CLIPS
        CLIPS = list(world.get("clips") or [])
        adapter = Adapter(world)
        msg = Msg(world)
        if not world.get("fetch_ok"):
            async def _boom(voice, label):
                raise _FetchFailed("fetch")
            _mk.fetch_telegram_file = _boom
        else:
            _mk.fetch_telegram_file = _fetch_telegram_file
        try:
            return canon(asyncio.run(live.admit_media(adapter, msg)))
        finally:
            _mk.fetch_telegram_file = _fetch_telegram_file
    if f == "cache_inbound_voice":
        adapter = Adapter(world)
        msg = Msg(world)
        event = Event()
        clips = world.get("clips") or []
        asyncio.run(live.cache_inbound_voice(adapter, msg, event, clips))
        if adapter.cached:
            label, kind, ext, mime = adapter.cached[0]
            return {"status": "download", "ext": ext, "mime": mime, "kind": kind, "label": label}
        return {"status": "attach", "media_urls": list(event.media_urls),
                "media_types": list(event.media_types),
                "reply_expected": event.reply_expected}
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"admit_media", "cache_inbound_voice", "env_probe"}
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
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001 - reported, never silently passed
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE w21_telegramvo_2f1ab7: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
