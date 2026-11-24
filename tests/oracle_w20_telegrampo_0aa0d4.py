#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_pool.py
(gate w20_telegrampo_0aa0d4).

The live module is imported against a stubbed `httpx` (a Limits that keeps an
unset max_keepalive_connections as None -- the branch the live code takes -- plus
the transport/stream base classes), so `fd_bounded_limits` is replayed over the
exact descriptor budget in the fixture and `PoolGeneration`'s lease accounting
is driven through real retire/release/close steps on a fake inner transport with
a fake clock. Compared values: the capped-limits decision and the generation's
final lease state.
"""
import asyncio
import importlib.util
import json
import os
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"


class Limits:
    def __init__(self, max_connections=None, max_keepalive_connections=None,
                 keepalive_expiry=None):
        self.max_connections = max_connections
        self.max_keepalive_connections = max_keepalive_connections
        self.keepalive_expiry = keepalive_expiry


class HTTPError(Exception):
    pass


class AsyncBaseTransport:
    pass


class AsyncByteStream:
    pass


class AsyncHTTPTransport:
    def __init__(self, **kwargs):
        self.kwargs = kwargs
        self.closed = False

    async def aclose(self):
        self.closed = True


sys.modules["httpx"] = types.SimpleNamespace(
    Limits=Limits, HTTPError=HTTPError, AsyncBaseTransport=AsyncBaseTransport,
    AsyncByteStream=AsyncByteStream, AsyncHTTPTransport=AsyncHTTPTransport,
    Request=object, Response=SimpleNamespace)

NAME = "w20_telegrampo_0aa0d4_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "plugins", "platforms", "telegram", "telegram_pool.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


class FakeStream:
    async def aclose(self):
        return None


class FakeInner:
    def __init__(self):
        self.fail = False
        self.closes = 0

    async def handle_async_request(self, request):
        if self.fail:
            raise RuntimeError("connect failed")
        return SimpleNamespace(stream=FakeStream())

    async def aclose(self):
        self.closes += 1


async def drive(ops):
    gen = live.PoolGeneration()
    inner = FakeInner()
    gen._inner = inner
    spawned = []
    original_spawn = live._spawn

    def spawn(coro):
        spawned.append(asyncio.ensure_future(coro))

    live._spawn = spawn
    responses = []
    try:
        for op in ops:
            if op == "begin":
                responses.append(await gen.handle_async_request(object()))
            elif op == "begin_fail":
                inner.fail = True
                try:
                    await gen.handle_async_request(object())
                except Exception:  # noqa: BLE001 - the transport error is the expected path
                    pass
                inner.fail = False
            elif op == "end":
                response = responses.pop()
                await response.stream.aclose()
            elif op == "end_twice":
                response = responses.pop()
                await response.stream.aclose()
                await response.stream.aclose()
            elif op == "retire":
                await gen.retire()
            elif op == "grace":
                if gen._grace is not None:
                    gen._grace._run()
                    await asyncio.sleep(0)
            elif op == "aclose":
                await gen.aclose()
        await asyncio.sleep(0)
        for task in spawned:
            if not task.done():
                await task
    finally:
        live._spawn = original_spawn
    return {
        "active": gen._active,
        "retired": gen._retired,
        "closed": gen._closed,
        "inner_closes": inner.closes,
        "grace_armed": gen._grace is not None,
    }


def expected(r):
    f = r.get("func")
    if f == "fd_bounded_limits":
        soft = r.get("soft")
        live._nofile_soft_limit = lambda: soft
        limits = Limits(max_connections=r.get("max_connections"),
                        max_keepalive_connections=r.get("max_keepalive_connections"),
                        keepalive_expiry=r.get("keepalive_expiry"))
        out = live.fd_bounded_limits(limits, r.get("paths", 1))
        return {
            "capped": out is not limits,
            "max_connections": out.max_connections,
            "max_keepalive_connections": out.max_keepalive_connections,
            "keepalive_expiry": out.keepalive_expiry,
        }
    if f == "lease":
        return asyncio.run(drive(r.get("ops") or []))
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
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001 - report the live failure verbatim
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_telegrampo_0aa0d4: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
