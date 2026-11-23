#!/usr/bin/env python3
"""Differential oracle for gateway/cron_store_notices.py (gate w13_cronstore_1b947c).

Drives the LIVE install_cron_store_notices transition-listener state machine
with a scripted loop and a stubbed send, and compares the ordered action trace
to the C port.  Delivery side effects (profile scope, i18n, home-channel send)
are a recorded seam; all announce/defer/drop decisions are the live module's.
"""
import asyncio
import importlib.util
import json
import sys
import types
from pathlib import Path

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

Q = "C:/Users/eman5/hermes-agent"
G = "w13_cronstore_1b947c"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _seed(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn)
            pm.__path__ = [Q + "/" + pn.replace(".", "/")]
            sys.modules[pn] = pm
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parent = ".".join(parts[:-1])
    if parent:
        setattr(sys.modules[parent], parts[-1], mod)
    return mod


_seed("agent")
_seed("agent.i18n", t=lambda k, **kw: k)
_seed("cron")
sh = _seed("cron.store_health", degraded_record=lambda p: None,
           degraded_records=lambda: [], set_transition_listener=lambda cb: None,
           store_key=lambda p: str(p))
_seed("gateway")
_seed("gateway.run", _async_profile_runtime_scope=None)
_seed("gateway.warning_notifications", present_notification=None)
_seed("hermes_constants", get_routing_process_hermes_home=lambda: "/home")

_spec = importlib.util.spec_from_file_location(G + "_live", Q + "/gateway/cron_store_notices.py")
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    actions = []
    degraded_map = {str(k).replace("\\", "/"): bool(v) for k, v in (rec.get("degraded") or {}).items()}
    boot = [str(s).replace("\\", "/") for s in (rec.get("boot") or [])]

    async def fake_send(runner, event, record):
        actions.append("send:%s:%s" % (event, str(record.store).replace("\\", "/")))

    live.send_cron_store_notice = fake_send

    def degraded_record(path):
        s = str(path).replace("\\", "/")
        return object() if degraded_map.get(s) else None

    sh.degraded_record = degraded_record
    sh.degraded_records = lambda: [types.SimpleNamespace(store=s, notice_fields=lambda: {}) for s in boot]

    listener = {}
    sh.set_transition_listener = lambda cb: listener.setdefault("cb", cb)

    loop_real = asyncio.new_event_loop()
    timers = {}

    class _Handle:
        def __init__(self, store): self.store = store; self.cancelled = False
        def cancel(self):
            self.cancelled = True
            actions.append("cancel:" + str(self.store).replace("\\", "/"))

    class _Done:
        def add_done_callback(self, cb): pass

    class FakeLoop:
        def create_task(self, coro):
            loop_real.run_until_complete(coro)
            return _Done()
        def call_later(self, delay, cb, record):
            store = str(record.store).replace("\\", "/")
            actions.append("schedule:" + store)
            h = _Handle(store)
            timers[store] = (h, cb, record)
            return h
        def call_soon_threadsafe(self, cb, ev, record):
            cb(ev, record)

    live.install_cron_store_notices(types.SimpleNamespace(), FakeLoop())
    transition = listener["cb"]

    def rec_for(store):
        return types.SimpleNamespace(store=store, notice_fields=lambda: {})

    for ev in (rec.get("events") or []):
        kind = ev.get("kind")
        store = str(ev.get("store", "")).replace("\\", "/")
        r = rec_for(store)
        if kind == "fire":
            t = timers.pop(store, None)
            if t is not None:
                t[1](r)  # send_recovered(record)
        else:
            transition(kind, r)
    loop_real.close()
    return actions


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") != "notice_facts":
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%s C=%r PY=%r" % (G, rec.get("case"), got, py), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (G, cases, mism))
    return 1 if (mism or cases == 0) else 0


if __name__ == "__main__":
    sys.exit(main())
