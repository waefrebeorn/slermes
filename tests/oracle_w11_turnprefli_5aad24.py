#!/usr/bin/env python3
"""Differential oracle for gate w11_turnprefli_5aad24 (agent/turn_preflight_gate.py).

The live run_preflight_gate body executes for every case against a deterministic
injected world (ollama error, compressor, warrants helper, run_preflight_compression).
The C port mirrors the same guard chain; this compares every rebound verdict field.
"""
import importlib.util
import json
import os
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

R = {}


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


agent_pkg = _mod("agent")
agent_pkg.__path__ = []


class PreflightGateVerdict:
    def __init__(self, **kw):
        self.__dict__.update(kw)


def _append_message(messages, row):
    messages.append(row)


def _warrants(a, b, c):
    return bool(R.get("warrants", False))


def _compression(agent, v, *, compressor, request_pressure_tokens,
                 provider_overflow_preflight, defer_preflight, moa_prepared_request,
                 system_message, user_message, max_compression_attempts, effective_task_id):
    v.action = R.get("compression_action", "fallthrough")
    v._last_preflight_pressure = R.get("post_pressure")
    v.compression_attempts += 1 if provider_overflow_preflight else 0
    v.provider_overflow_preflight = bool(provider_overflow_preflight)
    v.deferred = bool(defer_preflight(request_pressure_tokens))
    return v


_mod("agent.message_metadata", append_message=_append_message)
_mod("agent.turn_context", _compression_warrants_another_preflight_pass=_warrants)
_mod("agent.turn_preflight",
     PreflightGateVerdict=PreflightGateVerdict,
     run_preflight_compression=_compression)
_mod("agent.conversation_loop",
     _ollama_context_limit_error=lambda a, t: R.get("runtime_error"))

SRC = QUARRY + "/agent/turn_preflight_gate.py"
spec = importlib.util.spec_from_file_location("w11_turnprefli_5aad24_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def verdict_fields(v, messages):
    return {
        "action": v.action,
        "api_call_count": v.api_call_count,
        "compression_attempts": v.compression_attempts,
        "final_response": v.final_response,
        "failed": v.failed,
        "_turn_exit_reason": v._turn_exit_reason,
        "_compression_timeout_exhausted": v._compression_timeout_exhausted,
        "_preflight_compression_blocked": v._preflight_compression_blocked,
        "_last_preflight_pressure": v._last_preflight_pressure,
        "_provider_overflow_recovery_pending": v._provider_overflow_recovery_pending,
        "provider_overflow_preflight": getattr(v, "provider_overflow_preflight", False),
        "deferred": getattr(v, "deferred", False),
        "messages_count": len(messages),
    }


def compute(r):
    global R
    R = r
    messages = [{"role": "user", "content": "seed"}] * int(r.get("messages_count", 0))
    compressor = types.SimpleNamespace(
        threshold_tokens=r.get("threshold_tokens", 0),
        should_defer_preflight_to_real_usage=lambda t: bool(R.get("defer", False)))
    agent = types.SimpleNamespace(
        context_compressor=compressor,
        _request_pressure_anchored=bool(r.get("anchored", False)),
        _emit_diagnostic_status=lambda s: None,
        iteration_budget=types.SimpleNamespace(refund=lambda: None))
    v = live.run_preflight_gate(
        agent,
        request_pressure_tokens=r.get("request_pressure_tokens", 0),
        _moa_prepared_request=None,
        pending_moa_prepared_request=None,
        messages=messages,
        system_message="sys", user_message="usr",
        active_system_prompt="active", conversation_history=[],
        api_call_count=r.get("api_call_count_in", 0),
        compression_attempts=r.get("compression_attempts_in", 0),
        max_compression_attempts=3, effective_task_id=None,
        final_response=r.get("final_response_in"), failed=bool(r.get("failed_in", False)),
        _turn_exit_reason=r.get("turn_exit_reason_in"),
        _compression_timeout_exhausted=bool(r.get("timeout_exhausted_in", False)),
        _preflight_compression_blocked=bool(r.get("blocked_in", False)),
        _provider_overflow_recovery_pending=bool(r.get("provider_overflow_recovery_pending", False)),
        _last_preflight_pressure=r.get("last_preflight_pressure"))
    return verdict_fields(v, messages)


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or "out" not in r:
            continue
        cases += 1
        try:
            py = compute(r)
        except Exception as exc:
            mism += 1
            print("LIVE ERROR case=%s %r" % (r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH case=%s\n  py=%r\n  c =%r" % (r.get("case"), py, got), file=sys.stderr)
    print("ORACLE w11_turnprefli_5aad24: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
