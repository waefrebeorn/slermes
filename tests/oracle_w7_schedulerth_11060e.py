#!/usr/bin/env python3
"""Differential oracle for cron/scheduler_thread.py (gate w7_schedulerth_11060e)."""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn)
            pm.__path__ = [Q + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_stub("cron")

LIVE_NAME = "w7_schedulerth_11060e_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/cron/scheduler_thread.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)


def make_ticker(stop_set, alive, crashed, restarts):
    obj = live.SupervisedTickerThread.__new__(live.SupervisedTickerThread)
    obj._stop_event = SimpleNamespace(is_set=lambda: stop_set)

    class FakeThread:
        def __init__(self, a):
            self.a = a
        def is_alive(self):
            return self.a
        def start(self):
            pass

    obj._thread = FakeThread(alive)
    obj._crashed = crashed
    obj._name = "cron-scheduler"
    obj.restarts = restarts

    def _spawn():
        obj._crashed = False
        return FakeThread(True)

    obj._spawn = _spawn
    return obj


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "restart_if_dead":
        continue
    obj = make_ticker(bool(r.get("stop_set")), bool(r.get("thread_alive")),
                      bool(r.get("crashed")), int(r.get("restarts", 0)))
    restarted = obj.restart_if_dead()
    py = [restarted, obj.restarts]

    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH restart_if_dead args=%r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w7_schedulerth_11060e: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
