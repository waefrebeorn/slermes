#!/usr/bin/env python3
"""Generate tests/fixtures/context_compressor_flow7.jsonl (>=6 cases/function).

Each case is {"func": <live_name>, "in": {...}}; in.io holds the universal-seam
scripts. Both the C harness and the LIVE oracle consume the same file.

FLOW7 ports 14 UNCLAIMED module functions / methods (remainder beyond flow6):
  _begin_compress_attempt, _load_consecutive_overload_aborts,
  _load_anti_thrash_recovery_deadline, _persist_consecutive_overload_aborts,
  _increment_consecutive_overload_aborts, _record_summary_success,
  _record_summary_input_coverage, _log_compression_start,
  _reset_session_compaction_state, _native_anthropic_budget,
  _record_structural_no_op, _has_merged_inflight_replay,
  _derive_trigger, _fallback_summary_for_window.
"""
import json
from collections import Counter
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "context_compressor_flow7.jsonl"
cases = []


def case(func, **inp):
    cases.append({"func": func, "in": inp})


def exc(kind, msg="boom"):
    return {"__raise__": kind, "message": msg}


def script(ret):
    return {"__script__": True, "ret": ret}


MARKER = "--- END OF CONTEXT SUMMARY — respond to the message below, not the summary above ---"
HEADER = ("[STILL IN PROGRESS — this is the active request, restated after the "
          "compaction boundary because it was not finished yet. Continue it; do not "
          "start over.]")

# ── _begin_compress_attempt ──────────────────────────────────────────────
def bas(over=None):
    s = {"_last_summary_dropped_count": 9, "_last_summary_fallback_used": True,
         "_last_feasibility_skip": True, "_last_summary_error": "E",
         "_last_aux_model_failure_error": "A", "_last_aux_model_failure_model": "M",
         "_last_compress_aborted": True, "_last_compress_refused_would_grow": True,
         "_last_summary_overload_degraded": True, "_last_compression_made_progress": True,
         "_last_summary_auth_failure": True, "_structural_no_op_backoff_until": 77.0,
         "keep": "me"}
    if over:
        s.update(over)
    return s


case("_begin_compress_attempt", self=bas(), current_tokens=1000, force=False,
     io={"_begin_compression_telemetry": script({"chunk_count": 99, "x": 1}),
         "_clear_compression_failure_cooldown": None})
case("_begin_compress_attempt", self=bas(), current_tokens=1000, force=True,
     io={"_begin_compression_telemetry": script({"chunk_count": 3}),
         "_clear_compression_failure_cooldown": None})
case("_begin_compress_attempt", self=bas(), current_tokens=None, force=False,
     io={"_begin_compression_telemetry": script({}),
         "_clear_compression_failure_cooldown": None})
case("_begin_compress_attempt", self=bas(), current_tokens=0, force=True,
     io={"_begin_compression_telemetry": script({"a": [1, 2]}),
         "_clear_compression_failure_cooldown": None})
case("_begin_compress_attempt", self=bas(), current_tokens=5, force=False,
     io={"_begin_compression_telemetry": exc("RuntimeError", "r"),
         "_clear_compression_failure_cooldown": None})
case("_begin_compress_attempt", self=bas(), current_tokens=5, force=True,
     io={"_begin_compression_telemetry": script({}),
         "_clear_compression_failure_cooldown": exc("BaseException", "b")})
case("_begin_compress_attempt", self=bas(), current_tokens=5, force=False)  # fail-closed

# ── _load_consecutive_overload_aborts (seam _load_durable) ───────────────
case("_load_consecutive_overload_aborts", self={}, io={"_load_durable": script(0)})
case("_load_consecutive_overload_aborts", self={}, io={"_load_durable": script(5)})
case("_load_consecutive_overload_aborts", self={}, io={"_load_durable": script(-2)})
case("_load_consecutive_overload_aborts", self={}, io={"_load_durable": None})
case("_load_consecutive_overload_aborts", self={}, io={"_load_durable": exc("ValueError", "v")})
case("_load_consecutive_overload_aborts", self={})  # fail-closed

# ── _load_anti_thrash_recovery_deadline (seam _load_durable) ─────────────
case("_load_anti_thrash_recovery_deadline", self={}, io={"_load_durable": script(0.0)})
case("_load_anti_thrash_recovery_deadline", self={}, io={"_load_durable": script(123.5)})
case("_load_anti_thrash_recovery_deadline", self={}, io={"_load_durable": script(-1.0)})
case("_load_anti_thrash_recovery_deadline", self={}, io={"_load_durable": None})
case("_load_anti_thrash_recovery_deadline", self={}, io={"_load_durable": exc("BaseException", "b")})
case("_load_anti_thrash_recovery_deadline", self={})  # fail-closed

# ── _persist_consecutive_overload_aborts (seam _durable_write) ───────────
def pself(v=None):
    s = {"quiet_mode": True, "keep": "x"}
    if v is not None:
        s["_consecutive_overload_aborts"] = v
    return s


case("_persist_consecutive_overload_aborts", self=pself(0), io={"_durable_write": None})
case("_persist_consecutive_overload_aborts", self=pself(7), io={"_durable_write": None})
case("_persist_consecutive_overload_aborts", self=pself(-3), io={"_durable_write": None})
case("_persist_consecutive_overload_aborts", self=pself(), io={"_durable_write": None})
case("_persist_consecutive_overload_aborts", self=pself(2), io={"_durable_write": exc("SystemExit", "s")})
case("_persist_consecutive_overload_aborts", self=pself(2))  # fail-closed

# ── _increment_consecutive_overload_aborts (seam _durable_read) ──────────
case("_increment_consecutive_overload_aborts", self={"_consecutive_overload_aborts": 1},
     io={"_durable_read": script([True, 5])})
case("_increment_consecutive_overload_aborts", self={"_consecutive_overload_aborts": 4},
     io={"_durable_read": script([False, 0])})
case("_increment_consecutive_overload_aborts", self={"_consecutive_overload_aborts": 2},
     io={"_durable_read": script([True, 0])})
case("_increment_consecutive_overload_aborts", self={"_consecutive_overload_aborts": 0},
     io={"_durable_read": script([False, 3])})
case("_increment_consecutive_overload_aborts", self={"_consecutive_overload_aborts": 9},
     io={"_durable_read": script([True, -2])})
case("_increment_consecutive_overload_aborts", self={"_consecutive_overload_aborts": 1},
     io={"_durable_read": exc("KeyboardInterrupt", "k")})
case("_increment_consecutive_overload_aborts", self={"_consecutive_overload_aborts": 1})  # fail-closed

# ── _record_summary_success (3 seams) ────────────────────────────────────
def rss():
    return {"_previous_summary": "old", "_summary_model_fallen_back": True,
            "_last_summary_error": "E", "_last_summary_auth_failure": True,
            "keep": "x"}


case("_record_summary_success", self=rss(), summary="new",
     io={"_clear_compression_failure_cooldown": None,
         "_clear_terminal_summary_failures": None,
         "_reset_consecutive_overload_aborts": None})
case("_record_summary_success", self=rss(), summary="",
     io={"_clear_compression_failure_cooldown": None,
         "_clear_terminal_summary_failures": None,
         "_reset_consecutive_overload_aborts": None})
case("_record_summary_success", self=rss(), summary="S",
     io={"_clear_compression_failure_cooldown": exc("RuntimeError", "r"),
         "_clear_terminal_summary_failures": None,
         "_reset_consecutive_overload_aborts": None})
case("_record_summary_success", self=rss(), summary="S",
     io={"_clear_compression_failure_cooldown": None,
         "_clear_terminal_summary_failures": exc("BaseException", "b"),
         "_reset_consecutive_overload_aborts": None})
case("_record_summary_success", self=rss(), summary="S",
     io={"_clear_compression_failure_cooldown": None,
         "_clear_terminal_summary_failures": None,
         "_reset_consecutive_overload_aborts": exc("SystemExit", "s")})
case("_record_summary_success", self=rss(), summary="S")  # fail-closed

# ── _record_summary_input_coverage (no seams; mutates telemetry in self) ─
COV = {"input_chars": 100, "sampled_chars": 80, "omitted_chars": 20,
       "record_count": 5, "sampled_record_count": 4, "elided_record_count": 1}
case("_record_summary_input_coverage", self={"_active_compression_telemetry": {"a": 1}}, coverage=COV)
case("_record_summary_input_coverage", self={"_active_compression_telemetry": {}}, coverage=COV)
case("_record_summary_input_coverage", self={}, coverage=COV)  # no-op
case("_record_summary_input_coverage", self={"_active_compression_telemetry": None}, coverage=COV)
case("_record_summary_input_coverage", self={"_active_compression_telemetry": [1, 2]}, coverage=COV)
case("_record_summary_input_coverage", self={"_active_compression_telemetry": {"summary_input_chars": 0}},
     coverage=dict(COV, input_chars=0, omitted_chars=0))  # overwrite

# ── _log_compression_start (seam _log_info x3) ───────────────────────────
def lself(tt=1000, cl=128000, tp=0.7):
    return {"threshold_tokens": tt, "context_length": cl, "threshold_percent": tp}


case("_log_compression_start", self=lself(), display_tokens=120000, compress_start=4,
     compress_end=10, n_turns=6, tail_msgs=3, io={"_log_info": None})
case("_log_compression_start", self=lself(tt=5000, cl=32000, tp=0.5), display_tokens=1000,
     compress_start=0, compress_end=3, n_turns=3, tail_msgs=2, io={"_log_info": None})
case("_log_compression_start", self=lself(tt=99, cl=100, tp=0.25), display_tokens=0,
     compress_start=1, compress_end=1, n_turns=0, tail_msgs=0, io={"_log_info": None})
case("_log_compression_start", self=lself(), display_tokens=7, compress_start=2,
     compress_end=8, n_turns=5, tail_msgs=1, io={"_log_info": [None, exc("RuntimeError", "r")]})
case("_log_compression_start", self=lself(), display_tokens=7, compress_start=2,
     compress_end=8, n_turns=5, tail_msgs=1,
     io={"_log_info": [None, None, exc("KeyboardInterrupt", "k")]})
case("_log_compression_start", self=lself(), display_tokens=7, compress_start=2,
     compress_end=8, n_turns=5, tail_msgs=1)  # fail-closed

# ── _reset_session_compaction_state (3 seams + prepared_compaction) ──────
def rss2(pc=None):
    s = {"prepared_compaction": pc, "keep": "x", "_previous_summary": "old",
         "_consecutive_overload_aborts": 9, "_last_summary_overload_degraded": True,
         "_context_probed": True, "_last_compress_aborted": True,
         "_last_compression_telemetry": {"z": 1}}
    return s


case("_reset_session_compaction_state", self=rss2(None),
     io={"_prepared_compaction_discard": None, "_reset_real_usage_pairing": None,
         "_reset_proactive_prune_rearm": None})
case("_reset_session_compaction_state", self=rss2({}),
     io={"_prepared_compaction_discard": None, "_reset_real_usage_pairing": None,
         "_reset_proactive_prune_rearm": None})
case("_reset_session_compaction_state", self=rss2({"tag": "prep"}),
     io={"_prepared_compaction_discard": None, "_reset_real_usage_pairing": None,
         "_reset_proactive_prune_rearm": None})
case("_reset_session_compaction_state", self=rss2({}),
     io={"_prepared_compaction_discard": exc("BaseException", "b"),
         "_reset_real_usage_pairing": None, "_reset_proactive_prune_rearm": None})
case("_reset_session_compaction_state", self=rss2({}),
     io={"_prepared_compaction_discard": None,
         "_reset_real_usage_pairing": exc("RuntimeError", "r"),
         "_reset_proactive_prune_rearm": None})
case("_reset_session_compaction_state", self=rss2({}),
     io={"_prepared_compaction_discard": None, "_reset_real_usage_pairing": None,
         "_reset_proactive_prune_rearm": exc("SystemExit", "s")})
case("_reset_session_compaction_state", self=rss2({}))  # fail-closed

# ── _native_anthropic_budget (seam _native_anthropic_tail_prices) ────────
_nself = {"model": "m", "provider": "anthropic", "context_length": 128000}
case("_native_anthropic_budget", self=_nself, io={"_native_anthropic_tail_prices": script(4096)})
case("_native_anthropic_budget", self=_nself, io={"_native_anthropic_tail_prices": script(0)})
case("_native_anthropic_budget", self=_nself, io={"_native_anthropic_tail_prices": script([1, 2])})
case("_native_anthropic_budget", self=_nself, io={"_native_anthropic_tail_prices": None})
case("_native_anthropic_budget", self=_nself, io={"_native_anthropic_tail_prices": exc("RuntimeError", "r")})
case("_native_anthropic_budget", self=_nself)  # fail-closed

# ── _record_structural_no_op (seams _time_monotonic, _log_warning) ───────
def snself(sec=300.0, quiet=True):
    return {"_STRUCTURAL_NO_OP_BACKOFF_SECONDS": sec, "quiet_mode": quiet, "keep": "x"}


case("_record_structural_no_op", self=snself(), reason="no-eligible", n_messages=9,
     io={"_time_monotonic": script(1000.0), "_log_warning": None})
case("_record_structural_no_op", self=snself(), reason="tiny", n_messages=0,
     io={"_time_monotonic": script(0.0), "_log_warning": None})
case("_record_structural_no_op", self=snself(60.0), reason="x", n_messages=3,
     io={"_time_monotonic": script(12.5), "_log_warning": None})
case("_record_structural_no_op", self=snself(), reason="q", n_messages=None,
     io={"_time_monotonic": script(7.0), "_log_warning": None})
case("_record_structural_no_op", self=snself(), reason="x", n_messages=1,
     io={"_time_monotonic": exc("RuntimeError", "r"), "_log_warning": None})
case("_record_structural_no_op", self=snself(), reason="x", n_messages=1,
     io={"_time_monotonic": script(1.0), "_log_warning": exc("BaseException", "b")})
case("_record_structural_no_op", self=snself(quiet=False), reason="loud", n_messages=2,
     io={"_time_monotonic": script(50.0), "_log_warning": None})
case("_record_structural_no_op", self=snself(quiet=False), reason="loud", n_messages=2,
     io={"_time_monotonic": script(50.0), "_log_warning": exc("RuntimeError", "r")})
case("_record_structural_no_op", self=snself(), reason="x", n_messages=1)  # fail-closed

# ── _has_merged_inflight_replay (seams _is_context_summary_message, _content_text_for_contains) ──
def msg(content):
    return {"role": "assistant", "content": content}


case("_has_merged_inflight_replay", message=msg("preamble " + MARKER + "  " + HEADER + "  fix the bug"),
     io={"_is_context_summary_message": True,
         "_content_text_for_contains": "preamble " + MARKER + "  " + HEADER + "  fix the bug"})
case("_has_merged_inflight_replay", message=msg(MARKER + HEADER),
     io={"_is_context_summary_message": True,
         "_content_text_for_contains": MARKER + HEADER})
case("_has_merged_inflight_replay", message=msg("no marker " + HEADER),
     io={"_is_context_summary_message": True,
         "_content_text_for_contains": "no marker " + HEADER})
case("_has_merged_inflight_replay", message=msg("quoted " + HEADER + " in summary " + MARKER + " tail"),
     io={"_is_context_summary_message": True,
         "_content_text_for_contains": "quoted " + HEADER + " in summary " + MARKER + " tail"})
case("_has_merged_inflight_replay", message=msg(MARKER + " old " + MARKER + "\n" + HEADER + " active "),
     io={"_is_context_summary_message": True,
         "_content_text_for_contains": MARKER + " old " + MARKER + "\n" + HEADER + " active "})
case("_has_merged_inflight_replay", message={"role": "user"},
     io={"_is_context_summary_message": False})
case("_has_merged_inflight_replay", message=msg("x"),
     io={"_is_context_summary_message": exc("SystemExit", "s")})
case("_has_merged_inflight_replay", message=msg("x"),
     io={"_is_context_summary_message": True,
         "_content_text_for_contains": exc("RuntimeError", "r")})
case("_has_merged_inflight_replay", message=msg("x"))  # fail-closed

# ── _derive_trigger (4 seams) ────────────────────────────────────────────
def dself(mt=None, cfg=0.7, mx=200000):
    return {"model_thresholds": mt or {}, "_config_threshold_percent": cfg, "max_tokens": mx}


case("_derive_trigger", self=dself(), model="m", context_length=100000, provider="p",
     io={"resolve_model_threshold": script(0.7), "_effective_threshold_percent": script(0.5),
         "_compute_threshold_tokens": script(50000), "_effective_threshold_cap": script(45000)})
case("_derive_trigger", self=dself(), model="m", context_length=100000, provider="p",
     io={"resolve_model_threshold": script(0.7), "_effective_threshold_percent": script(0.5),
         "_compute_threshold_tokens": script(50000), "_effective_threshold_cap": None})
case("_derive_trigger", self=dself(), model="m", context_length=100000, provider="p",
     io={"resolve_model_threshold": script(0.9), "_effective_threshold_percent": script(0.8),
         "_compute_threshold_tokens": script(10000), "_effective_threshold_cap": script(20000)})
case("_derive_trigger", self=dself(mt={"m": 0.6}, cfg=0.5, mx=None), model="m",
     context_length=0, provider="codex",
     io={"resolve_model_threshold": script(0.6), "_effective_threshold_percent": script(0.6),
         "_compute_threshold_tokens": script(0), "_effective_threshold_cap": script(0)})
case("_derive_trigger", self=dself(), model="m", context_length=10, provider="p",
     io={"resolve_model_threshold": exc("RuntimeError", "r")})
case("_derive_trigger", self=dself(), model="m", context_length=10, provider="p",
     io={"resolve_model_threshold": script(0.7),
         "_effective_threshold_percent": exc("BaseException", "b")})
case("_derive_trigger", self=dself(), model="m", context_length=10, provider="p",
     io={"resolve_model_threshold": script(0.7), "_effective_threshold_percent": script(0.5),
         "_compute_threshold_tokens": exc("KeyboardInterrupt", "k")})
case("_derive_trigger", self=dself(), model="m", context_length=10, provider="p",
     io={"resolve_model_threshold": script(0.7), "_effective_threshold_percent": script(0.5),
         "_compute_threshold_tokens": script(50),
         "_effective_threshold_cap": exc("ValueError", "v")})
case("_derive_trigger", self=dself(), model="m", context_length=10, provider="p")  # fail-closed

# ── _fallback_summary_for_window (seams log + build static + strip prefix) ─
def fself(quiet=True, overload=False, err=None):
    return {"quiet_mode": quiet, "_last_summary_overload_degraded": overload,
            "_last_summary_error": err, "keep": "x"}


case("_fallback_summary_for_window", self=fself(quiet=True), telemetry={},
     turns_to_summarize=[{"role": "user", "content": "hi"}], n_dropped=3, feasibility_skip=False,
     io={"_build_static_fallback_summary": "[static]", "_strip_summary_prefix": "[stripped]"})
case("_fallback_summary_for_window", self=fself(quiet=False), telemetry={},
     turns_to_summarize=[], n_dropped=0, feasibility_skip=True,
     io={"_log_info": None, "_build_static_fallback_summary": "S", "_strip_summary_prefix": "S2"})
case("_fallback_summary_for_window", self=fself(quiet=False), telemetry={"failure_class": "old"},
     turns_to_summarize=[], n_dropped=1, feasibility_skip=False,
     io={"_log_warning": None, "_build_static_fallback_summary": "S", "_strip_summary_prefix": "S2"})
case("_fallback_summary_for_window", self=fself(quiet=True, overload=True), telemetry={},
     turns_to_summarize=[], n_dropped=2, feasibility_skip=False,
     io={"_build_static_fallback_summary": "S", "_strip_summary_prefix": "S2"})
case("_fallback_summary_for_window", self=fself(quiet=False, err="ERR"), telemetry={},
     turns_to_summarize=[], n_dropped=2, feasibility_skip=False,
     io={"_log_warning": None, "_build_static_fallback_summary": "S", "_strip_summary_prefix": "S2"})
case("_fallback_summary_for_window", self=fself(quiet=True), telemetry={},
     turns_to_summarize=[], n_dropped=1, feasibility_skip=True,
     io={"_build_static_fallback_summary": exc("RuntimeError", "r")})
case("_fallback_summary_for_window", self=fself(quiet=True), telemetry={},
     turns_to_summarize=[], n_dropped=1, feasibility_skip=False,
     io={"_build_static_fallback_summary": "S", "_strip_summary_prefix": exc("BaseException", "b")})
case("_fallback_summary_for_window", self=fself(quiet=True), telemetry={},
     turns_to_summarize=[], n_dropped=1, feasibility_skip=False)  # fail-closed

OUT.parent.mkdir(parents=True, exist_ok=True)
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for c in cases:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")

cnt = Counter(c["func"] for c in cases)
assert len(cnt) == 14, cnt
assert all(cnt[f] >= 6 for f in cnt), cnt
print("wrote %d cases across %d functions -> %s" % (len(cases), len(cnt), OUT))
print("per-function counts:", dict(sorted(cnt.items())))
