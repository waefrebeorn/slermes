#!/usr/bin/env python3
"""Generate tests/fixtures/context_compressor_flow8.jsonl (>=6 cases/func).

Each case is {"func": <live_name>, "in": {...}}; in.io holds the universal-seam
scripts. Both the C harness and the LIVE oracle consume the same file.

FLOW8 ports 12 UNCLAIMED module functions / methods (remainder beyond flow7):
  _reset_micro_compact_cursor_state, _reset_real_usage_pairing,
  _reset_consecutive_overload_aborts, _reset_proactive_prune_rearm,
  _temporal_anchoring_rule, _structural_no_op_result,
  _warn_reclamation_no_op, _spared_pending_tool_round,
  _demote_tool_result_at, _serialize_records_for_summary,
  _summary_template_sections, _build_summary_prompt.
"""
import json
from collections import Counter
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "context_compressor_flow8.jsonl"
FUNCS = [
    "_reset_micro_compact_cursor_state", "_reset_real_usage_pairing",
    "_reset_consecutive_overload_aborts", "_reset_proactive_prune_rearm",
    "_temporal_anchoring_rule", "_structural_no_op_result",
    "_warn_reclamation_no_op", "_spared_pending_tool_round",
    "_demote_tool_result_at", "_serialize_records_for_summary",
    "_summary_template_sections", "_build_summary_prompt",
]
cases = []


def case(func, **inp):
    cases.append({"func": func, "in": inp})


def exc(kind, msg="boom"):
    return {"__raise__": kind, "message": msg}


def script(ret):
    return {"__script__": True, "ret": ret}


# ── _reset_micro_compact_cursor_state ────────────────────────────────────
for i, (cur, roll, fail, last) in enumerate([
    (7, "rolling", 2, 4), (0, "", 0, -1), (99, "x", 5, 12),
    (1, "a\nb", 0, 0), (-3, "z", 1, 2), (10, "q", 3, 8), (50, "w", 0, 0),
]):
    case("_reset_micro_compact_cursor_state", self={
        "_micro_compact_cursor": cur, "_micro_compact_rolling_summary": roll,
        "_micro_compact_consecutive_failures": fail,
        "_micro_compact_last_failure_cursor": last, "keep": i})

# ── _reset_real_usage_pairing ────────────────────────────────────────────
for i, (a, b, c, d) in enumerate([
    (100, 200, True, True), (0, 0, False, False), (5, 6, True, False),
    (9, 1, False, True), (1234, 5678, True, True), (1, 2, False, False),
    (77, 88, True, True),
]):
    case("_reset_real_usage_pairing", self={
        "last_real_prompt_tokens": a, "last_compression_rough_tokens": b,
        "awaiting_real_usage_after_compression": c, "_provider_omits_usage": d,
        "keep": i})

# ── _reset_consecutive_overload_aborts (seam _persist_consecutive_overload_aborts) ──
for v in [0, 5, -2, 1, 9, 3, 7]:
    case("_reset_consecutive_overload_aborts",
         self={"_consecutive_overload_aborts": v, "keep": "x"},
         io={"_persist_consecutive_overload_aborts": None})
case("_reset_consecutive_overload_aborts",
     self={"_consecutive_overload_aborts": 2},
     io={"_persist_consecutive_overload_aborts": exc("RuntimeError", "r")})
case("_reset_consecutive_overload_aborts",
     self={"_consecutive_overload_aborts": 2},
     io={"_persist_consecutive_overload_aborts": exc("BaseException", "b")})
case("_reset_consecutive_overload_aborts",
     self={"_consecutive_overload_aborts": 2})  # fail-closed

# ── _reset_proactive_prune_rearm ─────────────────────────────────────────
for i, (r, c, w) in enumerate([
    (100, 5, ["x", 1]), (0, 0, None), (9, 3, ["y", 2]),
    (50, 10, None), (1, 0, ["z", 3]), (7, 2, None), (20, 4, ["a", 9]),
]):
    case("_reset_proactive_prune_rearm", self={
        "_proactive_prune_rearm_tokens": r, "_proactive_prune_cold_credit": c,
        "_last_reclaim_block_warn": w, "keep": i})

# ── _temporal_anchoring_rule (seam _today_for_prompt) ────────────────────
for d in ["2026-11-20", "2025-01-01", "", "1999-12-31", "today", "A B C", "1"]:
    case("_temporal_anchoring_rule", io={"_today_for_prompt": script(d)})
case("_temporal_anchoring_rule", io={"_today_for_prompt": None})
case("_temporal_anchoring_rule", io={"_today_for_prompt": exc("RuntimeError", "r")})
case("_temporal_anchoring_rule", io={"_today_for_prompt": exc("BaseException", "b")})
case("_temporal_anchoring_rule")  # fail-closed

# ── _structural_no_op_result (seam _record_structural_no_op) ─────────────
def snr_self():
    return {"_last_compression_savings_pct": 55.5, "keep": "s"}


for fc, rs, nm in [
    ("none", "empty", 0), ("cooldown", "too small", 3), ("x", "y", 10),
    ("a", "b", 1), ("c", "d", 99), ("e", "f", 2), ("g", "h", 7),
]:
    case("_structural_no_op_result", self=snr_self(),
         telemetry={"failure_class": "old", "x": 1},
         failure_class=fc, reason=rs, n_messages=nm,
         io={"_record_structural_no_op": None})
case("_structural_no_op_result", self=snr_self(), telemetry={},
     failure_class="a", reason="b", n_messages=1,
     io={"_record_structural_no_op": exc("RuntimeError", "r")})
case("_structural_no_op_result", self=snr_self(), telemetry={},
     failure_class="a", reason="b", n_messages=1,
     io={"_record_structural_no_op": exc("KeyboardInterrupt", "k")})
case("_structural_no_op_result", self=snr_self(), telemetry={},
     failure_class="a", reason="b", n_messages=1)  # fail-closed

# ── _warn_reclamation_no_op (seams _billed_basis_over_threshold, _log_warning) ──
def wrn_self(rearm=0, warn=None, thr=1000):
    return {"threshold_tokens": thr, "_proactive_prune_rearm_tokens": rearm,
            "_last_reclaim_block_warn": warn, "keep": "w"}


case("_warn_reclamation_no_op", self=wrn_self(), reason="lock", current_tokens=None,
     io={})
case("_warn_reclamation_no_op", self=wrn_self(), reason="lock", current_tokens=500,
     io={"_billed_basis_over_threshold": script(False)})
case("_warn_reclamation_no_op", self=wrn_self(), reason="lock", current_tokens=1500,
     io={"_billed_basis_over_threshold": script(True), "_log_warning": None})
case("_warn_reclamation_no_op", self=wrn_self(rearm=7, warn=["lock", 7]),
     reason="lock", current_tokens=1500,
     io={"_billed_basis_over_threshold": script(True)})
case("_warn_reclamation_no_op", self=wrn_self(rearm=0, thr=1000000),
     reason="big", current_tokens=1234567, before=999888,
     io={"_billed_basis_over_threshold": script(True), "_log_warning": None})
case("_warn_reclamation_no_op", self=wrn_self(rearm=4), reason="neg",
     current_tokens=-2000, before=-5,
     io={"_billed_basis_over_threshold": script(True), "_log_warning": None})
case("_warn_reclamation_no_op", self=wrn_self(), reason="e", current_tokens=1500,
     io={"_billed_basis_over_threshold": exc("BaseException", "b")})
case("_warn_reclamation_no_op", self=wrn_self(), reason="e", current_tokens=1500,
     io={})  # fail-closed

# ── _spared_pending_tool_round (seams _pending_tool_round, _effective_input_window, _estimate_msg_budget_tokens) ──
MSG_TOOL = [
    {"role": "assistant", "tool_calls": [{"id": "c1"}]},
    {"role": "tool", "tool_call_id": "c1", "content": "x"},
    {"role": "tool", "tool_call_id": "c2", "content": "y"},
    {"role": "tool", "tool_call_id": "c3", "content": "z"},
    {"role": "tool", "tool_call_id": "c4", "content": "w"},
]


def spr_self(w=10000, mt=None):
    return {"context_length": w, "max_tokens": mt}


case("_spared_pending_tool_round", self=spr_self(), messages=MSG_TOOL,
     io={"_pending_tool_round": script([2, 4]),
         "_effective_input_window": script(10000),
         "_estimate_msg_budget_tokens": [script(10), script(20), script(30)]})
case("_spared_pending_tool_round", self=spr_self(), messages=MSG_TOOL,
     io={"_pending_tool_round": script([0, 0])})
case("_spared_pending_tool_round", self=spr_self(), messages=MSG_TOOL,
     io={"_pending_tool_round": script([1, 3]),
         "_effective_input_window": script(10000),
         "_estimate_msg_budget_tokens": [script(5), script(6)]})
case("_spared_pending_tool_round", self=spr_self(w=100), messages=MSG_TOOL,
     io={"_pending_tool_round": script([2, 5]),
         "_effective_input_window": script(100),
         "_estimate_msg_budget_tokens": [script(900), script(900), script(900)]})
case("_spared_pending_tool_round", self=spr_self(), messages=MSG_TOOL,
     io={"_pending_tool_round": exc("RuntimeError", "r")})
case("_spared_pending_tool_round", self=spr_self(), messages=MSG_TOOL,
     io={"_pending_tool_round": script([1, 3]),
         "_effective_input_window": exc("BaseException", "b")})
case("_spared_pending_tool_round", self=spr_self(), messages=MSG_TOOL,
     io={"_pending_tool_round": script([1, 3]),
         "_effective_input_window": script(10000),
         "_estimate_msg_budget_tokens": exc("KeyboardInterrupt", "k")})
case("_spared_pending_tool_round", self=spr_self(), messages=MSG_TOOL,
     io={})  # fail-closed

# ── _demote_tool_result_at (seams _strip_images_from_tool_msg, _is_summary_stub, _json_dict, _summarize_tool_result) ──
def tool_msg(content, cid="c1", role="tool"):
    return {"role": role, "tool_call_id": cid, "content": content}


case("_demote_tool_result_at", result=[tool_msg("x")], idx=0,
     call_id_to_tool={}, min_prune_chars=10, io={})  # role not tool via caller? role is tool; content short
case("_demote_tool_result_at", result=[tool_msg("x", role="user")], idx=0,
     call_id_to_tool={}, min_prune_chars=0, io={})  # role != tool
case("_demote_tool_result_at", result=[tool_msg("")], idx=0,
     call_id_to_tool={}, min_prune_chars=0, io={})  # empty
case("_demote_tool_result_at", result=[tool_msg("[Old tool output cleared to save context space]")], idx=0,
     call_id_to_tool={}, min_prune_chars=0, io={})  # placeholder
case("_demote_tool_result_at", result=[tool_msg("[Duplicate tool output — same content as a more recent call]")], idx=0,
     call_id_to_tool={}, min_prune_chars=0, io={})  # duplicate prefix
case("_demote_tool_result_at", result=[tool_msg("a" * 400)], idx=0,
     call_id_to_tool={"c1": ["read_file", "{}"]}, min_prune_chars=200,
     io={"_is_summary_stub": script(True)})  # stub
case("_demote_tool_result_at", result=[tool_msg("a" * 400)], idx=0,
     call_id_to_tool={"c1": ["read_file", "{}"]}, min_prune_chars=200,
     io={"_is_summary_stub": script(False),
         "_summarize_tool_result": script("READ f — done [tool: read_file]")})
case("_demote_tool_result_at", result=[tool_msg("a" * 400)], idx=0,
     call_id_to_tool={"c1": ["skill_view", '{"name":"deploy"}']}, min_prune_chars=200,
     protected_skills=["deploy"],
     io={"_is_summary_stub": script(False), "_json_dict": script({"name": "deploy"})})
case("_demote_tool_result_at", result=[tool_msg(["IMG"])], idx=0,
     call_id_to_tool={}, min_prune_chars=0,
     io={"_strip_images_from_tool_msg": script({"role": "tool", "content": "STRIPPED"})})
case("_demote_tool_result_at", result=[tool_msg(["IMG"])], idx=0,
     call_id_to_tool={}, min_prune_chars=0,
     io={"_strip_images_from_tool_msg": None})  # strip -> None => False
case("_demote_tool_result_at", result=[tool_msg("a" * 400)], idx=0,
     call_id_to_tool={"c1": ["x", "{}"]}, min_prune_chars=200,
     io={"_is_summary_stub": exc("BaseException", "b")})
case("_demote_tool_result_at", result=[tool_msg("a" * 400)], idx=0,
     call_id_to_tool={"c1": ["x", "{}"]}, min_prune_chars=200,
     io={})  # fail-closed

# ── _serialize_records_for_summary (seams _summary_part_text, _redact_compaction_text, _media_directive_sub, _strip_think_blocks, _elide_middle, _render_tool_call_for_summary) ──
SER_SELF = {"_CONTENT_MAX": 6000, "_CONTENT_HEAD": 4000, "_CONTENT_TAIL": 1500}


def ser_io(red="hello", media=None, part=None, think=None, elide=None, tc=None):
    io = {"_redact_compaction_text": script(red),
          "_media_directive_sub": script(media if media is not None else red)}
    if part is not None:
        io["_summary_part_text"] = part
    if think is not None:
        io["_strip_think_blocks"] = think
    if elide is not None:
        io["_elide_middle"] = elide
    if tc is not None:
        io["_render_tool_call_for_summary"] = tc
    return io


case("_serialize_records_for_summary", self=SER_SELF,
     turns=[{"role": "user", "content": "raw"}],
     io=ser_io(red="hello", media="hello"))
case("_serialize_records_for_summary", self=SER_SELF,
     turns=[{"role": "tool", "tool_call_id": "id9", "content": "raw"}],
     io=ser_io(red="OUT", media="OUT"))
case("_serialize_records_for_summary", self=SER_SELF,
     turns=[{"role": "assistant", "content": "raw",
             "tool_calls": [{"id": "a"}, {"id": "b"}]}],
     io=ser_io(red="ANS", media="ANS", tc=[script("  a(...)"), script("  b(...)")]))
case("_serialize_records_for_summary", self=SER_SELF,
     turns=[{"role": "user", "content": [{"t": 1}, "plain", 5]}],
     io=ser_io(red="parts", media="parts", part=[script("P1"), script("P2")]))
case("_serialize_records_for_summary", self=SER_SELF,
     turns=[{"role": "assistant", "content": "raw"}],
     io=ser_io(red="raw", media="raw", think=script("CLEAN")))
case("_serialize_records_for_summary", self=SER_SELF,
     turns=[{"role": "user", "content": "raw"}],
     io=ser_io(red="LONG", media="LONG", elide=script("HEAD ... TAIL")))
case("_serialize_records_for_summary", self=SER_SELF,
     turns=[{"role": "user", "content": "raw"}],
     io={"_redact_compaction_text": exc("RuntimeError", "r")})
case("_serialize_records_for_summary", self=SER_SELF,
     turns=[{"role": "user", "content": "raw"}],
     io={})  # fail-closed

# ── _summary_template_sections (seam _temporal_anchoring_rule) ───────────
def sec(h="H", g="G", c="C", r="R"):
    return {"historical_task": h, "goal": g, "constraints": c, "resolved_questions": r}


for h, g, c, r in [
    ("task1", "goal1", "con1", "res1"), ("", "", "", ""),
    ("a\nb", "g", "c", "r"), ("H2", "G2", "C2", "R2"),
    ("one", "two", "three", "four"), ("x", "y", "z", "w"),
    ("long " * 20, "g", "c", "r"),
]:
    case("_summary_template_sections", _section=sec(h, g, c, r),
         summary_budget=1500, _session_log_section="",
         io={"_temporal_anchoring_rule": script("")})
case("_summary_template_sections", _section=sec(), summary_budget=3000,
     _session_log_section="<<LEAN_SESSION_LOG_SECTION>>",
     io={"_temporal_anchoring_rule": script("TEMP")})
case("_summary_template_sections", _section=sec(), summary_budget=100,
     _session_log_section="S",
     io={"_temporal_anchoring_rule": exc("BaseException", "b")})
case("_summary_template_sections", _section=sec(), summary_budget=100,
     _session_log_section="S", io={})  # fail-closed

# ── _build_summary_prompt (seams _memory_provider_section, _section_instructions, _summary_template_sections, _bound_summary_input) ──
BSEC = {"language": "LANG", "historical_task": "H", "goal": "G",
        "constraints": "C", "resolved_questions": "R"}


def bio(mem="", tsec="TSEC", has_turn=False, prev=None, bsec=None):
    io = {"_memory_provider_section": script(mem),
          "_section_instructions": script(bsec or BSEC),
          "_summary_template_sections": script(tsec)}
    if prev is not None:
        io["_bound_summary_input"] = script(prev)
    return io


case("_build_summary_prompt", self={"_previous_summary": None, "tail_mode": "lean"},
     content_to_summarize="C1", summary_budget=1500, focus_topic=None,
     memory_context="", has_user_turn=False,
     io=bio())
case("_build_summary_prompt", self={"_previous_summary": None, "tail_mode": "lean"},
     content_to_summarize="C2", summary_budget=2000, focus_topic="deploy",
     memory_context="", has_user_turn=False,
     io=bio())
case("_build_summary_prompt", self={"_previous_summary": "PREV", "tail_mode": "lean"},
     content_to_summarize="C3", summary_budget=3000, focus_topic=None,
     memory_context="", has_user_turn=True,
     io=bio(prev="BOUNDED"))
case("_build_summary_prompt", self={"_previous_summary": None, "tail_mode": "lean"},
     content_to_summarize="C4", summary_budget=1000, focus_topic="alpha beta",
     memory_context="MEM", has_user_turn=True,
     io=bio(mem="MEMSEC"))
case("_build_summary_prompt", self={"_previous_summary": None, "tail_mode": "full"},
     content_to_summarize="C5", summary_budget=500, focus_topic=None,
     memory_context="", has_user_turn=False,
     io=bio())
case("_build_summary_prompt", self={"_previous_summary": "P2", "tail_mode": "lean"},
     content_to_summarize="C6", summary_budget=2500, focus_topic="focus",
     memory_context="M2", has_user_turn=False,
     io=bio(mem="M2SEC", prev="BOUND2", tsec="SEC2"))
case("_build_summary_prompt", self={"_previous_summary": None},
     content_to_summarize="C7", summary_budget=100, focus_topic=None,
     memory_context="", has_user_turn=False,
     io={"_memory_provider_section": exc("RuntimeError", "r")})
case("_build_summary_prompt", self={"_previous_summary": None},
     content_to_summarize="C8", summary_budget=100, focus_topic=None,
     memory_context="", has_user_turn=False,
     io={"_memory_provider_section": script(""),
         "_section_instructions": script(BSEC),
         "_summary_template_sections": exc("BaseException", "b")})
case("_build_summary_prompt", self={"_previous_summary": None},
     content_to_summarize="C9", summary_budget=100, focus_topic=None,
     memory_context="", has_user_turn=False,
     io={})  # fail-closed

# ── write + coverage report ──────────────────────────────────────────────
OUT.parent.mkdir(parents=True, exist_ok=True)
with open(OUT, "w", encoding="utf-8") as f:
    for c in cases:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")
counts = Counter(c["func"] for c in cases)
print("wrote %d cases to %s" % (len(cases), OUT))
for name in sorted(counts):
    print("  %-40s %d" % (name, counts[name]))
assert len(FUNCS) == len(counts), (len(FUNCS), len(counts))
assert all(v >= 6 for v in counts.values()), counts
