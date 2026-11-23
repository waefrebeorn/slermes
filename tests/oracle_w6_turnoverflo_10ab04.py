#!/usr/bin/env python3
"""Differential oracle for agent/turn_overflow.py (gate w6_turnoverflo_10ab04).

_clamp_output_cap is a live function; its loop-state object and agent are
fixture-driven stubs so the call exercises the real clamping expression without
compressing / sleeping / persisting anything.
"""
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


_stub("agent")
_stub("agent.conversation_compression",
      COMPRESSION_RETRY_MESSAGES_STATUS_TEMPLATE="",
      COMPRESSION_RETRY_TOKENS_STATUS_TEMPLATE="",
      COMPRESSION_RETRY_TOO_LARGE_STATUS_TEMPLATE="",
      compression_blocked_transiently=lambda a: False,
      compression_skipped_due_to_lock=lambda a: False,
      context_compression_timed_out=lambda a: False)
_stub("agent.error_classifier", FailoverReason=SimpleNamespace())
_stub("agent.message_sanitization", serialized_messages_bytes=lambda m: 0)
_stub("agent.model_metadata",
      get_context_length_from_provider_error=lambda e, o: None,
      is_output_cap_error=lambda e: False,
      parse_available_output_tokens_from_error=lambda e: None)
_stub("agent.process_transport", is_local_server=lambda b, p: False)
_stub("agent.turn_failure_copy", site_copy=lambda *a, **k: "", stamp_failure=lambda d, *a, **k: d)
_stub("agent.turn_retry_state", TurnRetryState=object)

LIVE_NAME = "w6_turnoverflo_10ab04_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/agent/turn_overflow.py")
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
    if r.get("func") != "clamp":
        continue

    available_out = int(r.get("available_out", "0"))
    old_ctx = int(r.get("old_ctx", "0"))
    estimate = int(r.get("request_input_estimate", "0"))

    agent = SimpleNamespace(
        _ephemeral_max_output_tokens=None,
        _buffer_vprint=lambda *a, **k: None)
    st = SimpleNamespace(
        agent=agent,
        request_tokens=lambda: estimate,
        count_attempt=lambda **k: None,
        compress_scored_by_tokens=lambda *a, **k: (None, False, 0),
        done=lambda action, result=None: SimpleNamespace(action=action))
    live._clamp_output_cap(st, SimpleNamespace(), available_out, old_ctx)
    py = agent._ephemeral_max_output_tokens

    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH clamp args=%r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w6_turnoverflo_10ab04: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
