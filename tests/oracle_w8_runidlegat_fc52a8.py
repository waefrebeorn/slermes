#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/run_idle_gates.py
(gate w8_runidlegat_fc52a8).

The goal-cached store is replaced by an injected stand-in carrying the same
facts the C seam takes; all four sync gates and the async executor hop are
compared against the real live functions.
"""
import asyncio
import importlib.util
import json
import sys
import types
from pathlib import Path
from types import SimpleNamespace

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

FACTS = {}
def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m

def _work(_db=None):
    if FACTS.get("raises"):
        raise RuntimeError("probe failed")
    return bool(FACTS.get("has_work", False))

hermes_cli = _mod("hermes_cli"); hermes_cli.__path__ = []
_mod("hermes_cli.heartbeat", store_has_active_heartbeat=_work)
_mod("hermes_cli.loops", store_has_active_loop=_work)

spec = importlib.util.spec_from_file_location("run_idle_gates_live", Q + "/gateway/run_idle_gates.py")
live = importlib.util.module_from_spec(spec)
sys.modules["run_idle_gates_live"] = live
spec.loader.exec_module(live)

SENTINEL = SimpleNamespace(coordination=SimpleNamespace(has_pending_handoffs=lambda: _work(None)))

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    FACTS.clear()
    FACTS.update(r.get("facts") or {})
    live._profile_session_db_probe = lambda home: (None if not FACTS.get("db") else SENTINEL)
    try:
        if func == "gate":
            py = live._gate(Path("."), _work)
        elif func == "heartbeat":
            py = live.profile_has_active_heartbeat(Path("."))
        elif func == "loop":
            py = live.profile_has_active_loop(Path("."))
        elif func == "handoff":
            py = live.profile_has_pending_handoff(Path("."))
        elif func == "off_loop":
            if r.get("has_offload"):
                async def offload(probe):
                    return bool(r.get("offload_result", False))
                runner = SimpleNamespace(_run_in_executor_with_context=offload)
            else:
                runner = SimpleNamespace()
            py = asyncio.run(live.off_loop_gate(runner, lambda: True))
        else:
            continue
    except Exception as exc:
        print("LIVE ERROR %s %r: %r" % (func, r, exc), file=sys.stderr)
        mism += 1
        cases += 1
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w8_runidlegat_fc52a8: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
