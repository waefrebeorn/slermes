#!/usr/bin/env python3
"""Differential oracle for agent/turn_preflight.py (gate w7_turnpreflig_0b1495).

The live run_preflight_compression body is executed for every case against a
deterministic injected world (agent/compressor/lazy helpers are stubbed in
sys.modules and driven by the fixture flags). The C port mirrors the same
decision tree; this oracle compares the rebound loop locals and action/result
from the live PreflightGateVerdict to the C harness output.
"""
import importlib.util
import json
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

R = {}  # current fixture, read by the injected stubs


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


agent_pkg = _mod("agent")
agent_pkg.__path__ = []
_mod("agent.context_compressor_summary", accepts_keyword_argument=lambda f, n: False)
_mod("agent.context_engine", automatic_compaction_status_message=lambda *a, **k: "")
_mod("agent.conversation_compression",
     PRE_API_COMPRESSION_STATUS_TEMPLATE="Compressing {tokens} tokens",
     _reset_read_dedup_caches=lambda *a, **k: None,
     compression_blocked_transiently=lambda a: bool(R.get("blocked_transient", False)),
     compression_skipped_due_to_lock=lambda a: bool(R.get("lock_skipped", False)),
     context_compression_timed_out=lambda a: bool(R.get("timed_out", False)),
     conversation_history_after_compression=lambda a, m, h: h)
_mod("agent.conversation_compression_feasibility",
     ensure_compression_feasibility_checked=lambda *a, **k: None)
_mod("agent.turn_context",
     _review_fork_first_request_pending=lambda a: bool(R.get("review_fork_pending", False)))
_mod("agent.turn_context_compaction",
     _apply_grown_window=lambda *a, **k: None,
     _blocked_compress_reason=lambda c, t, attempts_spent=None: "cooldown",
     _clear_overflow_warn=lambda a: None,
     _refund_api_call=lambda a, c: c - 1,
     _reanchor=lambda a, m, u: 0,
     _reset_retry_state_after_compaction=lambda a: None)
_mod("agent.conversation_loop",
     _COMPRESSION_TIMEOUT_FINAL_RESPONSE="TIMEOUT_FINAL",
     _HANDOFF_SKIP_FINAL_RESPONSE="HANDOFF_SKIP_FINAL",
     _compression_deferred_result=lambda *a, reason="transient_block": {"kind": "deferred", "reason": reason},
     _maybe_grow_local_window=lambda a, c, t: R.get("grown_window", 0) or None,
     _provider_overflow_exhausted_result=lambda *a: {"kind": "exhausted"},
     _should_skip_model_call_for_reference_handoff=lambda m, u: bool(R.get("handoff", False)))

SRC = QUARRY + "/agent/turn_preflight.py"
spec = importlib.util.spec_from_file_location("w7_turnpreflig_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def verdict_fields(v):
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
        "result": v.result,
    }


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") != "run_preflight":
        continue
    cases += 1
    R = r
    nmsg = int(r.get("msg_count", 2))
    messages = [{"role": "user", "content": str(i)} for i in range(nmsg)]
    compressor = types.SimpleNamespace(
        get_active_compression_failure_cooldown=lambda: (1 if R.get("cooldown", False) else None),
        should_compress=lambda t: bool(R.get("should_compress", False)),
        threshold_tokens=100, context_length=4096)
    agent_obj = types.SimpleNamespace(
        compression_enabled=bool(R.get("compression_enabled", True)),
        model="test-model",
        context_compressor=types.SimpleNamespace(context_length=4096),
        _emit_status=lambda s: None,
        _persist_session=lambda *a: None,
        _warn_context_overflow_blocked=lambda *a: None,
        _warn_uncompressed_context_overflow=lambda *a: None,
        _compress_context=lambda m, s, **k: (m if R.get("compress_noop", False) else list(m), "active"))
    v = live.PreflightGateVerdict(
        action="", pending_moa_prepared_request=None, messages=messages,
        active_system_prompt="active", conversation_history=[],
        api_call_count=int(r.get("api_call_count", 1)),
        compression_attempts=int(r.get("compression_attempts", 0)),
        final_response=r.get("final_response"), failed=False,
        _turn_exit_reason=None, _compression_timeout_exhausted=False,
        _preflight_compression_blocked=bool(r.get("preflight_compression_blocked", False)),
        _provider_overflow_recovery_pending=False, _last_preflight_pressure=None, result=None)
    try:
        out = live.run_preflight_compression(
            agent_obj, v, compressor=compressor,
            request_pressure_tokens=5000,
            provider_overflow_preflight=bool(r.get("provider_overflow_preflight", False)),
            defer_preflight=lambda t: bool(R.get("defer", False)),
            moa_prepared_request=None, system_message="", user_message="",
            max_compression_attempts=int(r.get("max_compression_attempts", 3)),
            effective_task_id=None)
        py = verdict_fields(out)
    except Exception as exc:
        mism += 1
        print("LIVE ERROR case=%s %r" % (r.get("case"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH case=%s\n  py=%r\n  c =%r" % (r.get("case"), py, got), file=sys.stderr)

print("ORACLE w7_turnpreflig_0b1495: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
