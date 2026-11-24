#!/usr/bin/env python3
"""Differential oracle for
plugins/platforms/telegram/telegram_send_path_recovery.py (w20_telegramse_1853c7).

The live module runs its real edge functions against a scripted adapter and
gateway runner; the event loop is replaced by a shim that records task
creates and makes ``sleep`` instant. Compared values: the ordered action the
edge performs (super / clear / status write / sweep create / replay) and the
adapter flags it leaves behind.
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
NAME = "w20_telegramse_1853c7_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "plugins", "platforms", "telegram",
                       "telegram_send_path_recovery.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

TRACE = []
TASKS = []
DELIVERED = []
RERUN_AFTER = []
CALLS = [0]


class Pending:
    """A sweep task that is already running."""

    def done(self):
        return False

    def get_name(self):
        return "pending"


class Adapter(live.TelegramSendPathRecoveryMixin):
    def __init__(self, spec):
        self.spec = spec if isinstance(spec, dict) else {}
        self._send_path_degraded = bool(self.spec.get("degraded", False))
        self._running = bool(self.spec.get("running", False))
        self.has_fatal_error = bool(self.spec.get("fatal", False))
        self.name = self.spec.get("name", "tg")
        self.platform = self.spec.get("platform", "telegram")
        self._owner_profile = self.spec.get("owner_profile")
        self._send_path_recovery_task = Pending() if self.spec.get("task_running") else None
        self._send_path_recovery_rerun = bool(self.spec.get("rerun", False))
        self._background_tasks = set()
        self.gateway_runner = None
        if self.spec.get("has_redeliver", True):
            self.gateway_runner = Runner(self, self.spec)

    @property
    def send_path_degraded(self):
        return self._send_path_degraded

    def _write_runtime_status_safe(self, status, **kw):
        TRACE.append("status:%s" % status)


class Runner:
    def __init__(self, adapter, spec):
        self.adapter = adapter
        if spec.get("has_redeliver", True):
            self._redeliver_failed_obligations_for_platform = self._redeliver
        if spec.get("has_replay", False):
            self._schedule_restart_notice_replay = lambda: TRACE.append("replay")

    async def _redeliver(self, platform, profile=None):
        i = CALLS[0]
        CALLS[0] = i + 1
        count = DELIVERED[i] if i < len(DELIVERED) else 0
        TRACE.append("redeliver:%d" % count)
        self.adapter._send_path_recovery_rerun = (
            bool(RERUN_AFTER[i]) if i < len(RERUN_AFTER) else False)
        return count


class Loop:
    def create_task(self, coro, name=None):
        TRACE.append("create:%s" % name)
        task = asyncio.ensure_future(coro)
        if name:
            task.set_name(name)
        TASKS.append(task)
        return task


async def _fake_sleep(_seconds):
    TRACE.append("sleep")
    await asyncio.sleep(0)


live.asyncio = types.SimpleNamespace(sleep=_fake_sleep,
                                     get_running_loop=lambda: Loop())


def reset():
    del TRACE[:]
    del TASKS[:]
    CALLS[0] = 0


def adapter_out(adapter):
    task = adapter._send_path_recovery_task
    name = None if isinstance(task, Pending) or task is None else task.get_name()
    return {"degraded": bool(adapter._send_path_degraded),
            "rerun": bool(adapter._send_path_recovery_rerun),
            "task": name}


async def _run(r):
    reset()
    spec = r.get("adapter") or {}
    adapter = Adapter(spec)
    f = r.get("func")
    if f == "mark_recovered":
        live.mark_send_path_recovered(adapter)
        out = {"actions": list(TRACE)}
        out.update(adapter_out(adapter))
        for task in list(TASKS):
            task.cancel()
        await asyncio.sleep(0)
        return out
    if f == "schedule_sweep":
        live._schedule_recovery_sweep(adapter)
        out = {"actions": list(TRACE)}
        out.update(adapter_out(adapter))
        for task in list(TASKS):
            task.cancel()
        await asyncio.sleep(0)
        return out
    if f == "sweep_loop":
        DELIVERED[:] = list(r.get("delivered") or [])
        RERUN_AFTER[:] = list(r.get("rerun_after") or [])
        live._schedule_recovery_sweep(adapter)
        del TRACE[:]
        for task in list(TASKS):
            await task
        return {"actions": list(TRACE), "calls": CALLS[0]}
    if f == "finalize":
        del TRACE[:]

        class Base:
            async def _finalize_delivery_obligation(self, *a, **k):
                TRACE.append("super")

        class Mix(live.TelegramSendPathRecoveryMixin, Base):
            pass

        result = types.SimpleNamespace(**(r.get("result") or {}))
        delivery_adapter = adapter if spec.get("is_mixin", True) else object()
        await Mix()._finalize_delivery_obligation(1, result, None, delivery_adapter)
        actions = ["super"]
        if TASKS or adapter._send_path_recovery_rerun:
            actions.append("sweep")
        for task in list(TASKS):
            task.cancel()
        await asyncio.sleep(0)
        return {"actions": actions}
    raise AssertionError("unknown func %r" % (f,))


def expected(r):
    return asyncio.run(_run(r))


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
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_telegramse_1853c7: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
