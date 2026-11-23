#!/usr/bin/env python3
"""Differential oracle for w7_sessionopen_a7cccc (gateway/session_open.py)."""
import asyncio
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/gateway/session_open.py"
spec = importlib.util.spec_from_file_location("w7_sessionopen_a7cccc_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w7_sessionopen_a7cccc_live"] = live
spec.loader.exec_module(live)


def _reason(receipt_ok, target, row_exists, count):
    class Bindings:
        def receipt(self, sid):
            if not receipt_ok:
                raise KeyError("no receipt")
            return types.SimpleNamespace(entry={"session_id": target})

    resolved = target if receipt_ok else "sid"
    sessions = {resolved: types.SimpleNamespace(message_count=count)} if row_exists else {}
    store = types.SimpleNamespace(bindings=Bindings(), sessions=sessions)
    return live._reason(store, "sid")


def _gate_fire(policy_present, ignore_user_config):
    async def run():
        delivery = types.SimpleNamespace(unsettled=set(), opened=None)
        gate = live._Gate(delivery)
        policy = (types.SimpleNamespace(ignore_user_config=ignore_user_config)
                  if policy_present else None)
        gate.fire(policy, "route")
        return delivery.opened is not None
    return asyncio.run(run())


def _settled_extra(callback_timeout):
    async def run():
        timeouts = []

        async def fake_wait(fs, timeout=None):
            timeouts.append(timeout)
            return set(), list(fs)

        real_wait = live.asyncio.wait
        real_live = live._live
        opening = types.SimpleNamespace(task=object(), callback_timeout=callback_timeout)
        live.asyncio.wait = fake_wait
        live._live = lambda key: opening
        try:
            await live.settled(types.SimpleNamespace(profile_id="p", session_id="s"))
        finally:
            live.asyncio.wait = real_wait
            live._live = real_live
        return timeouts[1] if len(timeouts) > 1 else 0.0
    return asyncio.run(run())


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "reason":
            py = _reason(bool(r.get("receipt_ok", False)), r.get("receipt_target", "real"),
                         bool(r.get("row_exists", False)), int(r.get("message_count", 0)))
        elif func == "gate_fire":
            py = _gate_fire(bool(r.get("policy_present", False)),
                            bool(r.get("ignore_user_config", False)))
        elif func == "settled_extra":
            py = _settled_extra(float(r.get("callback_timeout", 0)))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} {r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} {r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w7_sessionopen_a7cccc: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
