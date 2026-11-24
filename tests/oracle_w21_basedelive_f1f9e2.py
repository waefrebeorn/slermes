#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/base_delivery_ledger.py
(gate w21_basedelive_f1f9e2).

The live coroutines are executed unchanged. The ledger store is a recorder
(`gateway.delivery_ledger.record_obligation` / `mark_attempting` /
`mark_delivered` / `mark_failed`), the config gate comes from the fixture, and
`compute_obligation_id`, `is_reconnect_only` and `classify_dead_error` stay the
live implementations. The fixture also carries the two classifier verdicts, and
those are re-derived here from the error text so a stale verdict fails loudly.
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

import gateway.dead_targets as dead_targets  # noqa: E402
import gateway.delivery_ledger as delivery_ledger  # noqa: E402

SRC = os.path.join(Q, "gateway", "platforms", "base_delivery_ledger.py")
_spec = importlib.util.spec_from_file_location("w21_bdl_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["w21_bdl_live"] = live
_spec.loader.exec_module(live)

CALLS = {"recorded": [], "attempting": [], "delivered": [], "failed": [],
         "redeliver": [], "schedule": []}


def _record_obligation(**kw):
    CALLS["recorded"].append(kw)


def _mark_attempting(obligation_id):
    CALLS["attempting"].append(obligation_id)


def _mark_delivered(obligation_id):
    CALLS["delivered"].append(obligation_id)


def _mark_failed(obligation_id, error):
    CALLS["failed"].append((obligation_id, error))


delivery_ledger.record_obligation = _record_obligation
delivery_ledger.mark_attempting = _mark_attempting
delivery_ledger.mark_delivered = _mark_delivered
delivery_ledger.mark_failed = _mark_failed


def _reset():
    for v in CALLS.values():
        v.clear()


def _ledger_enabled():
    return bool(WORLD.get("ledger_enabled"))


delivery_ledger.ledger_enabled = _ledger_enabled
WORLD = {}


class Source:
    def __init__(self):
        self.platform = types.SimpleNamespace(value="telegram")
        self.chat_id = "chat-1"
        self.thread_id = None


class Event:
    def __init__(self, world):
        self.text = world.get("text")
        self.message_id = world.get("message_id")
        if "ledger_message_id" in world:
            self.ledger_message_id = world.get("ledger_message_id")
        self.source = Source()


class Runner:
    def __init__(self, world):
        if world.get("redeliver_callable"):
            self._redeliver_failed_obligations_for_platform = self._redeliver
        if world.get("schedule_callable"):
            self._schedule_flood_redelivery = self._schedule

    def _redeliver(self, platform, profile=None):
        CALLS["redeliver"].append((platform, profile))

    def _schedule(self, platform, profile=None):
        CALLS["schedule"].append((platform, profile))


class Adapter:
    def __init__(self, world, delivery_adapter):
        self.world = world
        self.delivery_adapter = delivery_adapter
        self.typed_command_prefix = world.get("typed_command_prefix")
        self.gateway_runner = Runner(world)

    def _final_delivery_adapter(self, source):
        if self.world.get("live_is_delivery_adapter"):
            return self.delivery_adapter
        return types.SimpleNamespace(_owner_profile=None)


def expected(r):
    f = r.get("func")
    if f == "env_probe":
        sle = os.environ.get("SLERMES_HOME") or ""
        home = os.environ.get("HOME") or ""
        return {"sler": bool(sle), "home": bool(home), "same": bool(sle) and sle == home}
    w = r.get("world") if isinstance(r.get("world"), dict) else {}
    global WORLD
    WORLD = w
    _reset()
    delivery_adapter = types.SimpleNamespace(_owner_profile="work")
    adapter = Adapter(w, delivery_adapter)
    event = Event(w)
    if f == "record_final":
        out = asyncio.run(live.record_final_obligation(
            adapter, event, w.get("session_key") or "", w.get("content") or "",
            delivery_adapter, bool(w.get("is_ephemeral"))))
        if out is not None:
            if not CALLS["recorded"] or not CALLS["attempting"]:
                raise AssertionError("obligation id returned without a store write")
        return out
    if f == "finalize":
        result = types.SimpleNamespace(success=bool(w.get("success")),
                                       error=w.get("error"))
        error = str(getattr(result, "error", "") or "")
        # The fixture's seam verdicts must be what the live classifiers say.
        if bool(w.get("reconnect_only")) != delivery_ledger.is_reconnect_only(error):
            raise AssertionError("fixture reconnect_only disagrees with is_reconnect_only(%r)" % error)
        if (w.get("dead_kind") or None) != dead_targets.classify_dead_error(error):
            raise AssertionError("fixture dead_kind disagrees with classify_dead_error(%r)" % error)
        asyncio.run(live.finalize_final_obligation(adapter, "obl-1", result, event, delivery_adapter))
        if CALLS["delivered"]:
            return {"mark": "delivered", "action": "none"}
        if not CALLS["failed"]:
            raise AssertionError("a failed result must mark_failed")
        action = "redeliver" if CALLS["redeliver"] else ("schedule" if CALLS["schedule"] else "none")
        return {"mark": "failed", "action": action}
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"record_final", "finalize", "env_probe"}
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
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE w21_basedelive_f1f9e2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
