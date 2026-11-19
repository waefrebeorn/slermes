#!/usr/bin/env python3
"""Generate tests/context_flow_cases.jsonl — scripted-I/O cases for the
context_compressor FLOW oracle. Each line is {"func","in"} consumed by
t_port_context_flow.c (C seams) and oracle_context_flow.py (live stubs).

Run: python tests/gen_context_flow_cases.py > tests/context_flow_cases.jsonl
"""
import json
import sys

CASES = []

def add(func, **inp):
    CASES.append({"func": func, "in": inp})

def R(ret=None, state=None, raise_=None):
    d = {}
    if ret is not None:
        d["ret"] = ret
    if state is not None:
        d["state"] = state
    if raise_ is not None:
        d["raise"] = raise_
    return d

def io(**kw):
    return kw

# ── pin ───────────────────────────────────────────────────────────────────
add("pin", pin={"route": None}, ops=[{"op": "take"}, {"op": "take_det"}, {"op": "kwargs"}])
add("pin", pin={"route": None}, ops=[
    {"op": "set", "route": {"provider": "p", "model": "m", "base_url": "", "api_key": None, "timeout": 30}},
    {"op": "take"}, {"op": "take"}, {"op": "kwargs"}])
add("pin", pin={"route": None}, ops=[
    {"op": "set", "route": {"label": "x", "deterministic": True}},
    {"op": "take_det"}, {"op": "take_det"}, {"op": "take"}])
add("pin", pin={"route": None}, ops=[
    {"op": "set", "route": {"provider": "p", "model": "m"}},
    {"op": "take_det"}, {"op": "kwargs"}])
add("pin", pin={"route": None}, ops=[{"op": "set", "route": "notdict"}, {"op": "take"}])
add("pin", pin={"route": None}, ops=[
    {"op": "set", "route": {"provider": "p", "model": "", "base_url": None, "api_key": "k", "api_mode": "", "timeout": 0}},
    {"op": "kwargs"}])

# ── classify ──────────────────────────────────────────────────────────────
add("classify", exc={"type": "Other", "message": "request failed", "status_code": 408, "has_status": True}, verdict={"reason": "timeout"})
add("classify", exc={"type": "TimeoutError", "message": "stream stalled after 120s"}, verdict={"reason": "timeout", "transport": True})
add("classify", exc={"type": "JSONDecodeError", "message": "expecting value: line 1"}, verdict={"reason": "unknown"})
add("classify", exc={"type": "Other", "message": "model does not exist", "status_code": 404, "has_status": True}, verdict={})
add("classify", exc={"type": "RuntimeError", "message": "Context compression LLM returned empty content (provider=x)"}, verdict={})
add("classify", exc={"type": "RuntimeError", "message": "summary was truncated (finish_reason=length): cap"}, verdict={})
add("classify", exc={"type": "Other", "message": "provider overloaded"}, verdict={"reason": "overloaded"})
add("classify", exc={"type": "Other", "message": "connection reset"}, verdict={"reason": "ssl_cert_verification", "transport": True})
add("classify", exc={"type": "Other", "message": "blocked", "code": "gemini_prompt_blocked"}, verdict={})
add("classify", exc={"type": "Other", "message": "something odd"}, verdict={"reason": "unknown"})

# ── access_or_quota ───────────────────────────────────────────────────────
add("access_or_quota", exc={"type": "Other", "message": "quota", "status_code": 401, "has_status": True}, verdict={"reason": "rate_limit"})
add("access_or_quota", exc={"type": "Other", "message": "x"}, verdict={"reason": "auth"})
add("access_or_quota", exc={"type": "Other", "message": "x"}, verdict={"reason": "auth_permanent"})
add("access_or_quota", exc={"type": "Other", "message": "No API key was found for provider"}, verdict={"reason": "unknown"})
add("access_or_quota", exc={"type": "Other", "message": "forbidden", "status_code": 403, "has_status": True}, verdict={})
add("access_or_quota", exc={"type": "Other", "message": "insufficient_quota"}, verdict={})
add("access_or_quota", exc={"type": "UnscopedSecretError", "message": "no scope", "unscoped_secret": True}, verdict={})
add("access_or_quota", exc={"type": "Other", "message": "timeout talking to provider"}, verdict={"reason": "timeout"})

# ── short_error ───────────────────────────────────────────────────────────
add("short_error", str_e="boom", class_name="RuntimeError", limit=220)
add("short_error", str_e="   ", class_name="TimeoutError", limit=220)
add("short_error", str_e="x" * 300, class_name="RuntimeError", limit=220)
add("short_error", str_e="y" * 215 + "   " + "z" * 100, class_name="RuntimeError", limit=220)
add("short_error", str_e="a" * 220, class_name="RuntimeError", limit=220)
add("short_error", str_e="hello world", class_name="ValueError", limit=8)

# ── next_timeout ──────────────────────────────────────────────────────────
add("next_timeout", state={}, calls=["_consecutive_timeout_failures"] * 5)
add("next_timeout", state={}, calls=["_consecutive_truncation_failures"] * 3)
add("next_timeout", state={}, calls=["_consecutive_timeout_failures", "_consecutive_truncation_failures", "_consecutive_timeout_failures"])
add("next_timeout", state={"_consecutive_timeout_failures": 2}, calls=["_consecutive_timeout_failures"])

# ── block_reason ──────────────────────────────────────────────────────────
add("block_reason", state={"_summary_failure_cooldown_until": 100}, now=40)
add("block_reason", state={"_structural_no_op_backoff_until": 50}, now=10)
add("block_reason", state={"_summary_failure_cooldown_until": 100, "_structural_no_op_backoff_until": 200}, now=0)
add("block_reason", state={"_summary_failure_cooldown_until": 100, "_structural_no_op_backoff_until": 50}, now=10, ignore_cooldown=True)
add("block_reason", state={"_ineffective_compression_count": 2}, now=0)
add("block_reason", state={}, now=0)
add("block_reason", state={"_summary_failure_cooldown_until": 0.5}, now=0)
add("block_reason", state={"_summary_failure_cooldown_until": 101.5}, now=0)

# ── placement ─────────────────────────────────────────────────────────────
def msg(role, content="text", **kw):
    d = {"role": role, "content": content}
    d.update(kw)
    return d

add("placement", compressed=[msg("system", "sys")], tail_messages=[msg("user", "hello")], compress_start=1)
add("placement", compressed=[], tail_messages=[msg("user", "hello")], compress_start=0)
add("placement", compressed=[msg("system", "sys"), msg("user", "u")], tail_messages=[msg("assistant", "a")], compress_start=2)
add("placement", compressed=[msg("system", "sys"), msg("assistant", "a")], tail_messages=[msg("user", "u")], compress_start=2)
add("placement", compressed=[msg("system", "sys"), msg("tool", "t", tool_call_id="x")], tail_messages=[msg("tool", "t2", tool_call_id="y"), msg("user", "u")], compress_start=2)
add("placement", compressed=[msg("system", "sys"), msg("user", "")], tail_messages=[msg("assistant", "a")], compress_start=2)
add("placement", compressed=[msg("system", "sys"), msg("assistant", "a", tool_calls=[{"id": "1"}])], tail_messages=[msg("user", "u")], compress_start=2)

# ── walk_tail ─────────────────────────────────────────────────────────────
add("walk_tail", messages=[msg("user"), msg("assistant"), msg("user"), msg("assistant")], head_end=0, ceiling=10, min_tail=1, cut_at_break=False, prices=[5, 5, 5, 5])
add("walk_tail", messages=[msg("user"), msg("assistant"), msg("user"), msg("assistant")], head_end=0, ceiling=10, min_tail=1, cut_at_break=True, prices=[5, 5, 5, 5])
add("walk_tail", messages=[msg("user"), msg("assistant"), msg("user"), msg("assistant")], head_end=0, ceiling=10, min_tail=3, cut_at_break=False, prices=[5, 5, 5, 5])
add("walk_tail", messages=[msg("user"), msg("assistant"), msg("user"), msg("assistant")], head_end=2, ceiling=100, min_tail=1, cut_at_break=False, prices=[5, 5, 5, 5])
add("walk_tail", messages=[], head_end=0, ceiling=10, min_tail=1, cut_at_break=False, prices=[])
add("walk_tail", messages=[msg("user"), msg("assistant"), msg("user")], head_end=0, ceiling=7, min_tail=2, cut_at_break=True, prices=[3, 4, 5])

# ── should_compress_info ──────────────────────────────────────────────────
add("should_compress_info", state={"threshold_tokens": 1000, "last_prompt_tokens": 500}, now=0, wall=0)
add("should_compress_info", state={"threshold_tokens": 1000, "last_prompt_tokens": 2000, "quiet_mode": True}, now=0, wall=0)
add("should_compress_info", state={"threshold_tokens": 1000, "last_prompt_tokens": 2000, "quiet_mode": True, "_summary_failure_cooldown_until": 100}, now=0, wall=0,
    io=io(get_active_compression_failure_cooldown=R(), _load_ineffective_compression_count=R()))
add("should_compress_info", state={"threshold_tokens": 1000, "prompt_tokens": 2000, "quiet_mode": True, "_ineffective_compression_count": 2, "_anti_thrash_recovery_deadline": 50}, prompt_tokens=2000, now=0, wall=0,
    io=io(get_active_compression_failure_cooldown=R(), _load_ineffective_compression_count=R()))

# ── blocked_locally ───────────────────────────────────────────────────────
add("blocked_locally", state={"_summary_failure_cooldown_until": 100, "quiet_mode": True}, now=0, wall=0)
add("blocked_locally", state={"_summary_failure_cooldown_until": 100, "quiet_mode": True}, now=0, wall=0, ignore_cooldown=True)
add("blocked_locally", state={"_structural_no_op_backoff_until": 50, "quiet_mode": True}, now=10, wall=0)
add("blocked_locally", state={"_ineffective_compression_count": 2, "quiet_mode": True, "_anti_thrash_recovery_deadline": 0}, now=0, wall=1000,
    io=io(_durable_write=R()))
add("blocked_locally", state={"_ineffective_compression_count": 2, "quiet_mode": True, "_anti_thrash_recovery_deadline": 1000}, now=0, wall=1300,
    io=io(_durable_write=R(), _persist_ineffective_compression_count=R()))
add("blocked_locally", state={"_ineffective_compression_count": 2, "quiet_mode": True, "_anti_thrash_recovery_deadline": 5000}, now=0, wall=1000,
    io=io(_durable_write=R()))
add("blocked_locally", state={"_ineffective_compression_count": 0, "quiet_mode": True, "_anti_thrash_recovery_deadline": 500}, now=0, wall=0,
    io=io(_durable_write=R()))
add("blocked_locally", state={"_ineffective_compression_count": 0, "quiet_mode": True, "_anti_thrash_recovery_deadline": 0}, now=0, wall=0)

# ── blocked ───────────────────────────────────────────────────────────────
add("blocked", state={"quiet_mode": True, "_ineffective_compression_count": 0}, now=0, wall=0)
add("blocked", state={"quiet_mode": True, "_summary_failure_cooldown_until": 100, "_ineffective_compression_count": 0}, now=0, wall=0,
    io=io(get_active_compression_failure_cooldown=R(state={"_summary_failure_cooldown_until": 0}), _load_ineffective_compression_count=R()))
add("blocked", state={"quiet_mode": True, "_summary_failure_cooldown_until": 100}, now=0, wall=0,
    io=io(get_active_compression_failure_cooldown=R(), _load_ineffective_compression_count=R()))
add("blocked", state={"quiet_mode": True, "_summary_failure_cooldown_until": 100}, now=0, wall=0,
    io=io(get_active_compression_failure_cooldown=R(raise_={"type": "RuntimeError", "message": "db down"}),
          **{"logger.debug": R()}, _load_ineffective_compression_count=R()))

# ── fallback_to_main ──────────────────────────────────────────────────────
add("fallback_to_main", state={"summary_model": "aux", "model": "main", "_active_compression_telemetry": {"failure_class": None}},
    exc={"type": "RuntimeError", "message": "boom"}, reason="timed out", failed_model="aux-resolved",
    io=io(**{"logger.warning": R(), "_clear_compression_failure_cooldown": R(state={"_summary_failure_cooldown_until": 0, "_last_summary_error": None})}))
add("fallback_to_main", state={"summary_model": "aux", "model": "main"},
    exc={"type": "RuntimeError", "message": "boom"}, reason="failed", failed_model=None,
    io=io(**{"logger.warning": R(), "_clear_compression_failure_cooldown": R()}))
add("fallback_to_main", state={"summary_model": "", "model": "main", "_last_aux_resolved_model": "auto-pick"},
    exc={"type": "RuntimeError", "message": "boom"}, reason="failed", failed_model=None,
    io=io(**{"logger.warning": R(), "_clear_compression_failure_cooldown": R()}))
add("fallback_to_main", state={"summary_model": "", "model": "main", "_last_aux_resolved_model": ""},
    exc={"type": "RuntimeError", "message": "boom"}, reason="failed", failed_model=None,
    io=io(**{"logger.warning": R(), "_clear_compression_failure_cooldown": R()}))
add("fallback_to_main", state={"summary_model": "aux", "model": "main", "_active_compression_telemetry": {"failure_class": "summary_overload_degraded"}},
    exc={"type": "RuntimeError", "message": "boom"}, reason="failed", failed_model="aux",
    io=io(**{"logger.warning": R(), "_clear_compression_failure_cooldown": R()}))

# ── on_summary_failure ────────────────────────────────────────────────────
add("on_summary_failure", state={"summary_model": "", "model": "main"},
    exc={"type": "RuntimeError", "message": "No LLM provider configured for compression"}, verdict={},
    io=io(_raise_if_stale_attempt=R(), _record_compression_failure_cooldown=R(state={"_summary_failure_cooldown_until": 600, "_last_summary_error": "no auxiliary LLM provider configured"}), **{"logger.warning": R()}))
add("on_summary_failure", state={"summary_model": "aux", "model": "main", "_summary_model_fallen_back": False},
    exc={"type": "Other", "message": "invalid api key", "status_code": 401, "has_status": True}, verdict={"reason": "auth"},
    io=io(_raise_if_stale_attempt=R(), classify_api_error=R(), _fallback_to_main_for_compression=R(state={"summary_model": "", "_summary_model_fallen_back": True}), _generate_summary=R(ret="RETRY_SUMMARY")))
add("on_summary_failure", state={"summary_model": "", "model": "main", "_consecutive_timeout_failures": 0},
    exc={"type": "TimeoutError", "message": "stream stalled"}, verdict={"reason": "timeout", "transport": True},
    io=io(_raise_if_stale_attempt=R(), classify_api_error=R(), _record_compression_failure_cooldown=R(state={"_summary_failure_cooldown_until": 60}), **{"logger.warning": R()}))
add("on_summary_failure", state={"summary_model": "", "model": "main"},
    exc={"type": "RuntimeError", "message": "truncated (finish_reason=length)"}, verdict={},
    io=io(_raise_if_stale_attempt=R(), classify_api_error=R(), _record_compression_failure_cooldown=R(state={"_summary_failure_cooldown_until": 60}), **{"logger.warning": R()}))
add("on_summary_failure", state={"summary_model": "", "model": "main"},
    exc={"type": "RuntimeError", "message": "LLM returned empty content"}, verdict={},
    io=io(_raise_if_stale_attempt=R(), classify_api_error=R(), _record_compression_failure_cooldown=R(state={"_summary_failure_cooldown_until": 30}), **{"logger.warning": R()}))
add("on_summary_failure", state={"summary_model": "", "model": "main"},
    exc={"type": "Other", "message": "connection reset"}, verdict={"reason": "ssl_cert_verification", "transport": True},
    io=io(_raise_if_stale_attempt=R(), classify_api_error=R(), _record_compression_failure_cooldown=R(state={"_summary_failure_cooldown_until": 30}), **{"logger.warning": R()}))
add("on_summary_failure", state={"summary_model": "", "model": "main", "_consecutive_overload_aborts": 2},
    exc={"type": "Other", "message": "provider overloaded"}, verdict={"reason": "overloaded"},
    io=io(_raise_if_stale_attempt=R(), classify_api_error=R(), _record_compression_failure_cooldown=R(state={"_summary_failure_cooldown_until": 60}),
          _increment_consecutive_overload_aborts=R(state={"_consecutive_overload_aborts": 3}),
          _clear_terminal_summary_failures=R(state={"_last_summary_auth_failure": False, "_last_summary_network_failure": False, "_last_summary_truncated_failure": False, "_last_summary_empty_content_failure": False, "_last_summary_overload_failure": False}),
          **{"logger.warning": R()}))
add("on_summary_failure", state={"summary_model": "", "model": "main", "_consecutive_overload_aborts": 0},
    exc={"type": "Other", "message": "provider overloaded"}, verdict={"reason": "overloaded"},
    io=io(_raise_if_stale_attempt=R(), classify_api_error=R(), _record_compression_failure_cooldown=R(state={"_summary_failure_cooldown_until": 60}),
          _increment_consecutive_overload_aborts=R(state={"_consecutive_overload_aborts": 1}), **{"logger.warning": R()}))
add("on_summary_failure", state={"summary_model": "", "model": "main"},
    exc={"type": "JSONDecodeError", "message": "expecting value"}, verdict={"reason": "unknown"},
    io=io(_raise_if_stale_attempt=R(), classify_api_error=R(), **{"logger.error": R()}, _record_compression_failure_cooldown=R(state={"_summary_failure_cooldown_until": 30}), **{"logger.warning": R()}))
add("on_summary_failure", state={"summary_model": "", "model": "main"},
    exc={"type": "Other", "message": "x"}, verdict={},
    io=io(_raise_if_stale_attempt=R(raise_={"type": "AuxiliaryExplicitCancellation", "message": ""})))

# ── generate_summary ──────────────────────────────────────────────────────
GEN_HAPPY_IO = io(
    _compression_cancelled=R(ret=False),
    _compute_summary_budget=R(ret=2000),
    _collect_ghosted_skill_names=R(ret=["skill-a"]),
    _extract_pruned_skill_names=R(ret=["skill-b"]),
    _serialize_records_for_summary=R(ret=["rec1"]),
    _sample_summary_records=R(ret={"content": " sampled ", "coverage": {"n": 1}}),
    _record_summary_input_coverage=R(),
    typed_summary_messages=R(ret=[{"role": "user", "content": "typed"}]),
    _build_summary_prompt=R(ret=[{"role": "system", "content": "sys"}, {"role": "user", "content": "usr"}]),
    _call_summary_llm=R(ret="  raw summary  "),
    strip_think_blocks=R(ret="raw summary"),
    _redact_compaction_text=R(ret="redacted summary"),
    _reinject_pruned_skill_markers=R(ret="redacted summary [SKILL_PRUNED:skill-a]"),
    _ground_historical_task_snapshot=R(ret="grounded"),
    _augment_summary_lean=R(ret="augmented"),
    _validate_summary_user_provenance=R(),
    _raise_if_stale_attempt=R(),
    _record_summary_success=R(),
    _with_summary_prefix=R(ret="PREFIX\naugmented"),
)
add("generate_summary", state={"_summary_failure_cooldown_until": 0, "_previous_summary": "", "tail_mode": "lean", "_summary_has_user_turn": True, "quiet_mode": True},
    turns=[{"role": "user", "content": "hi"}], now=0, io=GEN_HAPPY_IO)
add("generate_summary", state={"_summary_failure_cooldown_until": 100, "quiet_mode": True}, turns=[], now=0,
    io=io(_compression_cancelled=R(ret=False), **{"logger.debug": R()}))
add("generate_summary", state={"_summary_failure_cooldown_until": 100, "_previous_summary": "", "tail_mode": "lean", "_summary_has_user_turn": True, "quiet_mode": True},
    turns=[{"role": "user", "content": "hi"}], now=0, bypass_cooldown=True, io=GEN_HAPPY_IO)
add("generate_summary", state={"quiet_mode": True}, turns=[], now=0, io=io(_compression_cancelled=R(ret=True)))
GEN_FAIL_IO = dict(GEN_HAPPY_IO)
GEN_FAIL_IO["_call_summary_llm"] = R(raise_={"type": "RuntimeError", "message": "empty content"})
GEN_FAIL_IO["_on_summary_failure"] = R(ret="FALLBACK")
add("generate_summary", state={"_summary_failure_cooldown_until": 0, "_previous_summary": "", "tail_mode": "lean", "_summary_has_user_turn": True, "quiet_mode": True},
    turns=[{"role": "user", "content": "hi"}], now=0, io=GEN_FAIL_IO)
GEN_FULL_IO = dict(GEN_HAPPY_IO)
del GEN_FULL_IO["_serialize_records_for_summary"]
del GEN_FULL_IO["_sample_summary_records"]
del GEN_FULL_IO["_record_summary_input_coverage"]
del GEN_FULL_IO["typed_summary_messages"]
GEN_FULL_IO["_serialize_for_summary"] = R(ret="serialized")
GEN_FULL_IO["_bound_summary_input"] = R(ret="bounded")
add("generate_summary", state={"_summary_failure_cooldown_until": 0, "_previous_summary": "", "tail_mode": "full", "_summary_has_user_turn": True, "quiet_mode": True},
    turns=[{"role": "user", "content": "hi"}], now=0, io=GEN_FULL_IO)
GEN_PREV_IO = dict(GEN_HAPPY_IO)
GEN_PREV_IO["_redact_compaction_text"] = [R(ret="old redacted"), R(ret="redacted")]
add("generate_summary", state={"_summary_failure_cooldown_until": 0, "_previous_summary": "old secret", "tail_mode": "lean", "_summary_has_user_turn": False, "quiet_mode": True},
    turns=[{"role": "user", "content": "hi"}], now=0, io=GEN_PREV_IO)
GEN_SCAN_IO = dict(GEN_HAPPY_IO)
GEN_SCAN_IO["_transcript_has_real_user_turn"] = R(ret=True)
add("generate_summary", state={"_summary_failure_cooldown_until": 0, "_previous_summary": "", "tail_mode": "lean", "quiet_mode": True},
    turns=[{"role": "user", "content": "hi"}], now=0, io=GEN_SCAN_IO)

# ── compress ──────────────────────────────────────────────────────────────
MSGS = [msg("system", "sys"), msg("user", "u1"), msg("assistant", "a1"), msg("user", "u2"), msg("assistant", "a2"), msg("user", "u3")]
FINAL = [msg("system", "sys"), msg("user", "SUMMARY"), msg("user", "u3")]
COMPRESSED = [msg("system", "sys"), msg("user", "SUMMARY")]

def compress_io(**over):
    base = io(
        _raise_if_stale_attempt=R(),
        _begin_compress_attempt=R(ret={}),
        _protect_head_size=R(ret=1),
        _spared_pending_tool_round=R(ret=[]),
        _drop_blank_echoes=R(ret=MSGS),
        _plan_compaction_window=R(ret={"messages": MSGS, "pruned_count": 0, "compress_start": 1, "compress_end": 4}),
        _take_prepared_summary=R(ret=None),
        summary_view=R(ret=MSGS),
        _scan_window_handoffs=R(ret={"turns_to_summarize": MSGS[1:4], "summary_indices": [], "tail_start": 4, "previous_summary_before": None}),
        _record_compression_regions=R(),
        _fallback_streak_skip=R(ret=False),
        _feasibility_skip=R(ret=False),
        _summarize_window=R(ret="SUMMARY"),
        _commit_window=R(ret={"messages": MSGS, "pruned_count": 0}),
        _assemble_with_archive_list=R(ret=COMPRESSED),
        _finalize_compressed=R(ret=FINAL),
    )
    base.update(over)
    return base

add("compress", state={"quiet_mode": True, "last_prompt_tokens": 0}, messages=MSGS, current_tokens=5000, io=compress_io())
add("compress", state={"quiet_mode": True}, messages=MSGS[:3], current_tokens=5000,
    io=io(_raise_if_stale_attempt=R(), _begin_compress_attempt=R(ret={}), _protect_head_size=R(ret=1), _structural_no_op_result=R()))
add("compress", state={"quiet_mode": True}, messages=MSGS, current_tokens=5000,
    io=compress_io(_plan_compaction_window=R(ret={"messages": MSGS, "pruned_count": 0, "compress_start": 4, "compress_end": 4}), _structural_no_op_result=R()))
add("compress", state={"quiet_mode": True}, messages=MSGS, current_tokens=5000,
    io=compress_io(_scan_window_handoffs=R(ret={"turns_to_summarize": [], "summary_indices": [1, 2, 3], "tail_start": 4, "previous_summary_before": "old"}), _structural_no_op_result=R()))
add("compress", state={"quiet_mode": True}, messages=MSGS, current_tokens=5000,
    io=compress_io(_summarize_window=R(ret=None), _abort_on_summary_failure=R(ret=True)))
add("compress", state={"quiet_mode": True}, messages=MSGS, current_tokens=5000,
    io=compress_io(_summarize_window=R(ret=None), _abort_on_summary_failure=R(ret=False), _fallback_summary_for_window=R(ret="FALLBACK")))
add("compress", state={"quiet_mode": True}, messages=MSGS, current_tokens=5000,
    io=compress_io(_fallback_streak_skip=R(ret=True), _fallback_summary_for_window=R(ret="FALLBACK")))
add("compress", state={"quiet_mode": True}, messages=MSGS, current_tokens=5000,
    io=compress_io(_take_prepared_summary=R(ret={"compress_end": 5}), _adopt_prepared_summary=R(ret="PREPARED")))
add("compress", state={"quiet_mode": True, "last_prompt_tokens": 9000}, messages=MSGS, current_tokens=None,
    io=compress_io())
add("compress", state={"quiet_mode": True, "last_prompt_tokens": 0}, messages=MSGS, current_tokens=None,
    io=compress_io(estimate_messages_tokens_rough=R(ret=7000)))

for c in CASES:
    print(json.dumps(c, ensure_ascii=False))
print(f"# generated {len(CASES)} cases", file=sys.stderr)