#!/usr/bin/env python3
"""Differential oracle for gateway/restart_ack.py (w7_restartack_3c2a4b gate).

send_restart_ack is async and delegates error classification to
BasePlatformAdapter. We stub gateway.config.Platform and gateway.platforms.base,
drive the LIVE function with a fake adapter/runner/event, and inject the await
outcome and classification through the fixture booleans.
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


def stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn)
            pm.__path__ = [os.path.join(Q, *parts[:i])]
            sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


LOCAL = object()  # identity sentinel for Platform.LOCAL
stub("gateway")
stub("gateway.config", Platform=types.SimpleNamespace(LOCAL=LOCAL))
stub("gateway.platforms")


class BasePlatformAdapter:
    @staticmethod
    def _is_retryable_error(error):
        return getattr(error, "retryable", False)

    @staticmethod
    def _is_timeout_error(error):
        return getattr(error, "timeout", False)


stub("gateway.platforms.base", BasePlatformAdapter=BasePlatformAdapter,
     _mark_notify_metadata=lambda m: m)

LIVE_NAME = "w7_restartack_3c2a4b_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/gateway/restart_ack.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)


class Runner:
    def _reply_metadata(self, event):
        return {}

    def _reply_anchor_for_event(self, event):
        return "anchor"


class Event:
    source = types.SimpleNamespace(chat_id="c1")


def compute(r):
    if r.get("func") != "send_restart_ack":
        return None
    if r.get("adapter_none", False):
        adapter = None
    else:
        platform = LOCAL if r.get("platform_local", False) else object()

        class Adapter:
            def __init__(self, plat):
                self.platform = plat
                self.raises = bool(r.get("send_raises", False))
                self.success = bool(r.get("result_success", False))
                self.retryable = bool(r.get("result_retryable", False))
                self.error = types.SimpleNamespace(
                    retryable=bool(r.get("error_retryable", False)),
                    timeout=bool(r.get("error_timeout", False)))

            async def send(self, *a, **kw):
                if self.raises:
                    raise RuntimeError("boom")
                return types.SimpleNamespace(
                    success=self.success, retryable=self.retryable, error=self.error)

        adapter = Adapter(platform)
    return asyncio.run(live.send_restart_ack(Runner(), adapter, Event(), "ack"))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    py = compute(r)
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH restartack %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w7_restartack_3c2a4b: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
