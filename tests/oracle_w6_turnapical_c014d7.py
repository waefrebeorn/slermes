#!/usr/bin/env python3
"""Differential oracle for agent/turn_api_call.py (gate w6_turnapical_c014d7).

Only _should_stream is ported; its provider/profile lookups and the agent's
observable attributes are injected from the fixture through stubs. The rest of
the module (perform_api_call / interrupt / rate guard) is pure I/O and stays in
Python.
"""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace
from unittest.mock import Mock

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


_stub("agent")
_stub("agent.error_classifier", FailoverReason=SimpleNamespace())
_stub("agent.agent_runtime_helpers_placeholders",
      hidden_interrupt_placeholder_row=lambda: {})
_stub("agent.message_metadata", append_message=lambda *a, **k: None)
_stub("agent.repetition_guard",
      REPETITION_LOOP_INTERRUPTED="", is_runaway_repetition=lambda s: False)
_stub("agent.turn_failure_copy",
      site_copy=lambda *a, **k: "", stamp_failure=lambda d, *a, **k: d)
_providers = _stub("providers", get_provider_profile=lambda p: None)
_stub("hermes_cli")
_rpb = _stub("hermes_cli.runtime_provider_backends",
             _is_external_process_provider=lambda p: False)

LIVE_NAME = "w6_turnapical_c014d7_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/agent/turn_api_call.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "should_stream":
        continue

    ss = r.get("supports_streaming", -1)
    ss_val = None if ss == -1 else bool(ss)
    _providers.get_provider_profile = (
        lambda p, _v=ss_val: SimpleNamespace(supports_streaming=_v))
    _rpb._is_external_process_provider = (
        lambda p, _b=bool(r.get("external_process_provider", False)): _b)
    agent = SimpleNamespace(
        _disable_streaming=bool(r.get("disable_streaming", False)),
        base_url=r.get("base_url") or "",
        provider=r.get("provider") or "",
        _has_stream_consumers=(
            lambda _b=bool(r.get("has_stream_consumers", False)): _b),
        client=(Mock() if r.get("client_is_mock") else object()),
    )
    py = bool(live._should_stream(agent))

    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH should_stream args=%r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w6_turnapical_c014d7: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
