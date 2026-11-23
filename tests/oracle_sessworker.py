#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/session_worker_spawn.py.

Drives the live acquire_process() under two deterministic scenarios:
  1. not cancelled: spawn returns immediately -> the process is returned.
  2. cancelled before create() stored the process: spawn blocks on an Event,
     the awaiting task is cancelled, then the Event is released so create()
     observes the cancelled flag and closes the process.
A fake ManagedWorker records close() calls; sys.modules is patched so the
function's local `from gateway.session_managed_worker import ManagedWorker`
binds to the fake.
"""
import asyncio, importlib.util, json, sys, threading, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.path.insert(0, "C:/Users/eman5/hermes-agent")

SRC = "C:/Users/eman5/hermes-agent/gateway/session_worker_spawn.py"
spec = importlib.util.spec_from_file_location("session_worker_spawn_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["session_worker_spawn_live"] = live
spec.loader.exec_module(live)

CLOSED = []
class FakeWorker:
    def __init__(self, p):
        self.p = p
    def close(self):
        CLOSED.append(self.p)
fake = types.ModuleType("gateway.session_managed_worker")
fake.ManagedWorker = FakeWorker
sys.modules["gateway.session_managed_worker"] = fake

def spawn_ok(v):
    return v

def compute(process, cancel):
    CLOSED.clear()
    if not cancel:
        async def go():
            return await live.acquire_process(spawn_ok, process)
        ret = asyncio.run(go())
        return {"cancelled": False, "returned": ret, "closed": list(CLOSED)}

    entered = threading.Event()
    gate = threading.Event()
    def spawn_block(v):
        entered.set()
        gate.wait(timeout=2.0)
        return v
    async def go():
        task = asyncio.create_task(live.acquire_process(spawn_block, process))
        await asyncio.to_thread(entered.wait, 2.0)
        await asyncio.sleep(0)
        task.cancel()
        cancelled = False
        try:
            await task
        except asyncio.CancelledError:
            cancelled = True
        gate.set()
        await asyncio.sleep(0)
        await asyncio.sleep(0)
        return cancelled
    cancelled = asyncio.run(go())
    return {"cancelled": cancelled, "returned": None, "closed": list(CLOSED)}

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "acquire":
        continue
    cases += 1
    py = compute(r.get("process"), bool(r.get("cancel")))
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH acquire py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE sessworker: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
