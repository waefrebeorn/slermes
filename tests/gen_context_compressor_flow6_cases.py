#!/usr/bin/env python3
"""Generate tests/fixtures/context_compressor_flow6.jsonl (>=6 cases/function).

Each case is {"func": <live_name>, "in": {...}}; in.io holds the universal-seam
scripts. Both the C harness and the LIVE oracle consume the same file.

FLOW6 ports 18 UNCLAIMED module functions / methods (remainder beyond flow5):
  _is_real_user_turn, _real_user_indices_desc, preview_threshold_tokens,
  _render_tool_call_for_summary, _drop_blank_echoes, _prune_boundary,
  _demote_stale_tail_tools, _clear_terminal_summary_failures,
  maybe_seed_preflight_display_tokens, note_native_compaction_checkpoint,
  record_rejected_compaction, _apply_real_prompt_verdict,
  _abort_on_summary_failure, _assemble_head, _assemble_tail,
  _dedupe_tool_results, _augment_summary_lean, _merge_summary_into_tail_row.
"""
import json
from collections import Counter
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "context_compressor_flow6.jsonl"
cases = []


def case(func, **inp):
    cases.append({"func": func, "in": inp})


def exc(kind, msg="boom"):
    return {"__raise__": kind, "message": msg}


def script(ret):
    return {"__script__": True, "ret": ret}


TERMINAL = ["_last_summary_auth_failure", "_last_summary_network_failure",
            "_last_summary_truncated_failure", "_last_summary_empty_content_failure",
            "_last_summary_overload_failure"]


def term_self(**over):
    s = {k: False for k in TERMINAL}
    s.update(over)
    return s


END = "--- END OF CONTEXT SUMMARY — respond to the message below, not the summary above ---"
DELIM = "[END OF PRIOR CONTEXT — COMPACTION SUMMARY BELOW]"
HDR = "[PRIOR CONTEXT — for reference only; not a new message]"
ANCHOR = "## Anchor Index (mechanically extracted, exact)"
USER = "## User Messages (verbatim, newest first)"
RECOVERY = "## Context Recovery"

# ── _is_real_user_turn (seam _turns_is_real_user_turn) ────────────────────
case("_is_real_user_turn", message={"role": "user", "content": "hi"},
     io={"_turns_is_real_user_turn": True})
case("_is_real_user_turn", message={"role": "user"},
     io={"_turns_is_real_user_turn": False})
case("_is_real_user_turn", message={"role": "tool"},
     io={"_turns_is_real_user_turn": script(True)})
case("_is_real_user_turn", message=None,
     io={"_turns_is_real_user_turn": exc("KeyboardInterrupt", "k")})
case("_is_real_user_turn", message="x", io={"_turns_is_real_user_turn": exc("BaseException", "b")})
case("_is_real_user_turn", message={"role": "user"})  # fail-closed
case("_is_real_user_turn", message={}, io={"_turns_is_real_user_turn": False})

# ── _real_user_indices_desc (seam _turns_is_real_user_turn, composed) ────
_MSGS = [{"role": "user"}, {"role": "tool"}, {"role": "user"}, {"role": "assistant"}]
case("_real_user_indices_desc", messages=_MSGS, head_end=0,
     io={"_turns_is_real_user_turn": [True, False, True, False]})
case("_real_user_indices_desc", messages=_MSGS, head_end=2,
     io={"_turns_is_real_user_turn": [True, True, False, False]})
case("_real_user_indices_desc", messages=_MSGS, head_end=4,
     io={"_turns_is_real_user_turn": [True, False, True, True]})
case("_real_user_indices_desc", messages=[], head_end=0,
     io={"_turns_is_real_user_turn": []})
case("_real_user_indices_desc", messages=_MSGS, head_end=1,
     io={"_turns_is_real_user_turn": [False, False, True, exc("SystemExit", "s")]})
case("_real_user_indices_desc", messages=_MSGS, head_end=0)  # fail-closed
case("_real_user_indices_desc", messages=_MSGS, head_end=0,
     io={"_turns_is_real_user_turn": [False, False, False, False]})

# ── preview_threshold_tokens (seam _derive_trigger) ──────────────────────
case("preview_threshold_tokens", model="m", context_length=100000, provider="p",
     io={"_derive_trigger": script([0, 0, 0.7])})
case("preview_threshold_tokens", model="m", context_length=100000,
     io={"_derive_trigger": script([0, 0, 0.5])})
case("preview_threshold_tokens", model="m", context_length=1, provider="codex",
     io={"_derive_trigger": script([1, 2, 123])})
case("preview_threshold_tokens", model="m", context_length=0,
     io={"_derive_trigger": script([0, 0, 0.0])})
case("preview_threshold_tokens", model="m", context_length=10,
     io={"_derive_trigger": exc("RuntimeError", "r")})
case("preview_threshold_tokens", model="m", context_length=10,
     io={"_derive_trigger": script([1, 2])})  # IndexError
case("preview_threshold_tokens", model="m", context_length=10)  # fail-closed

# ── _render_tool_call_for_summary (self-method seam _render_tool_args_for_summary) ──
case("_render_tool_call_for_summary", tc={"function": {"name": "read_file", "arguments": "{}"}},
     io={"_render_tool_args_for_summary": "{}"})
case("_render_tool_call_for_summary", tc={"function": {"name": "x", "arguments": "a" * 1600}},
     io={"_render_tool_args_for_summary": "a" * 1200 + "..."})
case("_render_tool_call_for_summary", tc={"function": {}},
     io={"_render_tool_args_for_summary": ""})
case("_render_tool_call_for_summary", tc={"function": {"name": 5, "arguments": 7}},
     io={"_render_tool_args_for_summary": "7"})
case("_render_tool_call_for_summary", tc="not a dict",
     io={"_render_tool_args_for_summary": "unused"})
case("_render_tool_call_for_summary", tc={"function": {"name": "y", "arguments": "z"}},
     io={"_render_tool_args_for_summary": exc("KeyboardInterrupt", "k")})
case("_render_tool_call_for_summary", tc={"function": {"name": "y", "arguments": "z"}})  # fail-closed

# ── _drop_blank_echoes (seams _find_last_user_message_idx, _blank_echo_indices_after) ──
_DM = [{"role": "user", "content": "a"}, {"role": "assistant", "content": ""},
       {"role": "user", "content": "b"}]
case("_drop_blank_echoes", messages=_DM,
     io={"_find_last_user_message_idx": 2, "_blank_echo_indices_after": script([1])})
case("_drop_blank_echoes", messages=_DM,
     io={"_find_last_user_message_idx": 2, "_blank_echo_indices_after": script([])})
case("_drop_blank_echoes", messages=_DM,
     io={"_find_last_user_message_idx": 0, "_blank_echo_indices_after": script([0, 1])})
case("_drop_blank_echoes", messages=[],
     io={"_find_last_user_message_idx": 0, "_blank_echo_indices_after": script([])})
case("_drop_blank_echoes", messages=_DM,
     io={"_find_last_user_message_idx": 2, "_blank_echo_indices_after": exc("ValueError", "v")})
case("_drop_blank_echoes", messages=_DM,
     io={"_find_last_user_message_idx": exc("SystemExit", "s")})
case("_drop_blank_echoes", messages=_DM)  # fail-closed

# ── _prune_boundary (seam _walk_tail_budget) ──────────────────────────────
_PR = [{"role": "user"}, {"role": "assistant"}, {"role": "user"}, {"role": "tool"}]
case("_prune_boundary", result=_PR, protect_tail_count=2, protect_tail_tokens=None,
     io={"_walk_tail_budget": script([1, None])})
case("_prune_boundary", result=_PR, protect_tail_count=3, protect_tail_tokens=0,
     io={"_walk_tail_budget": script([0, None])})
case("_prune_boundary", result=_PR, protect_tail_count=2, protect_tail_tokens=100,
     io={"_walk_tail_budget": script([2, None])})
case("_prune_boundary", result=_PR, protect_tail_count=20, protect_tail_tokens=100,
     io={"_walk_tail_budget": script([0, None])})
case("_prune_boundary", result=[], protect_tail_count=2, protect_tail_tokens=100,
     io={"_walk_tail_budget": script([0, None])})
case("_prune_boundary", result=_PR, protect_tail_count=1, protect_tail_tokens=100,
     io={"_walk_tail_budget": exc("RuntimeError", "r")})
case("_prune_boundary", result=_PR, protect_tail_count=1,
     protect_tail_tokens=100)  # fail-closed

# ── _demote_stale_tail_tools (seam demote_stale_tail_tools) ───────────────
_DS = [{"role": "tool", "content": "x"}, {"role": "user", "content": "y"}]
case("_demote_stale_tail_tools", self={"_session_id": "s1", "quiet_mode": True}, messages=_DS, tail_start=1,
     io={"demote_stale_tail_tools": script([_DS, 0])})
case("_demote_stale_tail_tools", self={"_session_id": "s1", "quiet_mode": False}, messages=_DS, tail_start=0,
     io={"demote_stale_tail_tools": script([[{"role": "tool", "content": "stub"}], 1])})
case("_demote_stale_tail_tools", self={"quiet_mode": True}, messages=_DS, tail_start=0,
     io={"demote_stale_tail_tools": script([_DS, 0])})
case("_demote_stale_tail_tools", self={"_session_id": None, "quiet_mode": True}, messages=_DS, tail_start=1,
     io={"demote_stale_tail_tools": script([[], 2])})
case("_demote_stale_tail_tools", self={"_session_id": "s1", "quiet_mode": True}, messages=_DS, tail_start=1,
     io={"demote_stale_tail_tools": exc("CancelledError", "c")})
case("_demote_stale_tail_tools", self={"_session_id": "s1", "quiet_mode": True}, messages=_DS, tail_start=1)  # fail-closed

# ── _clear_terminal_summary_failures (no seams; self flags) ───────────────
case("_clear_terminal_summary_failures", self=term_self())
case("_clear_terminal_summary_failures", self=term_self(**{TERMINAL[0]: True}))
case("_clear_terminal_summary_failures", self=term_self(**{TERMINAL[2]: True, TERMINAL[4]: True}))
case("_clear_terminal_summary_failures", self=term_self(**{k: True for k in TERMINAL}))
case("_clear_terminal_summary_failures", self={})
case("_clear_terminal_summary_failures", self=term_self(extra_flag=True))

# ── maybe_seed_preflight_display_tokens (no seams; self state) ───────────
case("maybe_seed_preflight_display_tokens", self={"last_prompt_tokens": 0}, preflight_tokens=100)
case("maybe_seed_preflight_display_tokens", self={"last_prompt_tokens": 0}, preflight_tokens=0)
case("maybe_seed_preflight_display_tokens", self={"last_prompt_tokens": 5}, preflight_tokens=100)
case("maybe_seed_preflight_display_tokens", self={"last_prompt_tokens": -1}, preflight_tokens=100)
case("maybe_seed_preflight_display_tokens", self={"last_prompt_tokens": 0, "seed": "x"}, preflight_tokens=100)
case("maybe_seed_preflight_display_tokens", self={"last_prompt_tokens": 0}, preflight_tokens=-5)

# ── note_native_compaction_checkpoint (no seams; self state) ─────────────
case("note_native_compaction_checkpoint", self={})
case("note_native_compaction_checkpoint", self={"awaiting_real_usage_after_compression": False})
case("note_native_compaction_checkpoint", self={"last_compression_rough_tokens": 999})
case("note_native_compaction_checkpoint", self={"awaiting_real_usage_after_compression": True,
                                                "last_compression_rough_tokens": 12345})
case("note_native_compaction_checkpoint", self={"other": "keep"})
case("note_native_compaction_checkpoint", self={"awaiting_real_usage_after_compression": False,
                                                "last_compression_rough_tokens": 0})

# ── record_rejected_compaction (seam verdict; self count) ────────────────
case("record_rejected_compaction", self={"_ineffective_compression_count": 0, "quiet_mode": True},
     io={"_record_ineffective_compression_verdict": None})
case("record_rejected_compaction", self={"_ineffective_compression_count": 2, "quiet_mode": True},
     io={"_record_ineffective_compression_verdict": None})
case("record_rejected_compaction", self={"_ineffective_compression_count": 5, "quiet_mode": False},
     io={"_record_ineffective_compression_verdict": None})
case("record_rejected_compaction", self={"_ineffective_compression_count": -1, "quiet_mode": True},
     io={"_record_ineffective_compression_verdict": None})
case("record_rejected_compaction", self={"_ineffective_compression_count": 1, "quiet_mode": True},
     io={"_record_ineffective_compression_verdict": exc("BaseException", "b")})
case("record_rejected_compaction", self={"_ineffective_compression_count": 1, "quiet_mode": True})  # fail-closed

# ── _apply_real_prompt_verdict (seam verdict; self state) ─────────────────
def rp(lpt, thr, verify, cnt=0, quiet=True):
    return {"last_prompt_tokens": lpt, "threshold_tokens": thr,
            "_verify_compaction_cleared_threshold": verify,
            "_ineffective_compression_count": cnt, "quiet_mode": quiet}


case("_apply_real_prompt_verdict", self=rp(0, 100, False),
     io={"_record_ineffective_compression_verdict": None})
case("_apply_real_prompt_verdict", self=rp(50, 100, False),
     io={"_record_ineffective_compression_verdict": None})
case("_apply_real_prompt_verdict", self=rp(150, 100, False),
     io={"_record_ineffective_compression_verdict": None})
case("_apply_real_prompt_verdict", self=rp(150, 100, True, cnt=2),
     io={"_record_ineffective_compression_verdict": None})
case("_apply_real_prompt_verdict", self=rp(50, 100, True, cnt=3),
     io={"_record_ineffective_compression_verdict": None})
case("_apply_real_prompt_verdict", self=rp(100, 100, True, cnt=1),
     io={"_record_ineffective_compression_verdict": exc("KeyboardInterrupt", "k")})
case("_apply_real_prompt_verdict", self=rp(50, 100, True))  # fail-closed

# ── _abort_on_summary_failure (seam _caller_attempt_is_current; self + telemetry state) ──
def ab(**over):
    s = term_self(abort_on_summary_failure=False, quiet_mode=True,
                  _last_summary_dropped_count=9, _last_summary_fallback_used=True,
                  _last_compress_aborted=False)
    s.update(over)
    return s


case("_abort_on_summary_failure", self=ab(), telemetry={}, n_skipped=3,
     previous_summary_before_scan="prev", io={"_caller_attempt_is_current": False})
case("_abort_on_summary_failure", self=ab(abort_on_summary_failure=True), telemetry={}, n_skipped=1,
     previous_summary_before_scan=None, io={"_caller_attempt_is_current": False})
case("_abort_on_summary_failure", self=ab(**{TERMINAL[0]: True}), telemetry={"a": 1}, n_skipped=2,
     previous_summary_before_scan="P", io={"_caller_attempt_is_current": True})
case("_abort_on_summary_failure", self=ab(**{TERMINAL[4]: True}), telemetry={}, n_skipped=0,
     previous_summary_before_scan="Q", io={"_caller_attempt_is_current": True})
case("_abort_on_summary_failure", self=ab(**{TERMINAL[1]: True, TERMINAL[3]: True}), telemetry={}, n_skipped=5,
     previous_summary_before_scan="R", io={"_caller_attempt_is_current": False})
case("_abort_on_summary_failure", self=ab(abort_on_summary_failure=True), telemetry={}, n_skipped=1,
     previous_summary_before_scan="S", io={"_caller_attempt_is_current": exc("SystemExit", "x")})
case("_abort_on_summary_failure", self=ab(abort_on_summary_failure=True), telemetry={}, n_skipped=1,
     previous_summary_before_scan="T")  # fail-closed

# ── _assemble_head (seams _fresh_compaction_message_copy, _content_text_for_contains,
#                     _append_text_to_content, _strip_context_summary_handoff_message) ──
NOTE = "COMPACTION NOTE"
SYS = {"role": "system", "content": "base system"}
USR = {"role": "user", "content": "hello"}
case("_assemble_head", self={"_COMPRESSION_NOTE": NOTE}, messages=[SYS, USR], compress_start=1,
     io={"_fresh_compaction_message_copy": [{"role": "system", "content": "base system"}],
         "_content_text_for_contains": ["base system"],
         "_append_text_to_content": ["base system\n\n" + NOTE],
         "_strip_context_summary_handoff_message": [{"role": "system", "content": "base system\n\n" + NOTE}]})
case("_assemble_head", self={"_COMPRESSION_NOTE": NOTE}, messages=[SYS, USR], compress_start=2,
     io={"_fresh_compaction_message_copy": [{"role": "system", "content": "base system"},
                                            {"role": "user", "content": "hello"}],
         "_content_text_for_contains": ["base system"],
         "_append_text_to_content": ["base system\n\n" + NOTE],
         "_strip_context_summary_handoff_message": [{"role": "system", "content": "base system\n\n" + NOTE},
                                                    {"role": "user", "content": "hello"}]})
case("_assemble_head", self={"_COMPRESSION_NOTE": NOTE}, messages=[SYS], compress_start=1,
     io={"_fresh_compaction_message_copy": [{"role": "system", "content": "base system"}],
         "_content_text_for_contains": ["base system " + NOTE],
         "_strip_context_summary_handoff_message": [{"role": "system", "content": "base system"}]})
case("_assemble_head", self={"_COMPRESSION_NOTE": NOTE}, messages=[{"role": "system"}], compress_start=1,
     io={"_fresh_compaction_message_copy": [{"role": "system"}],
         "_content_text_for_contains": [""],
         "_append_text_to_content": [NOTE],
         "_strip_context_summary_handoff_message": [{"role": "system", "content": NOTE}]})
case("_assemble_head", self={"_COMPRESSION_NOTE": NOTE}, messages=[USR], compress_start=1,
     io={"_fresh_compaction_message_copy": [{"role": "user", "content": "hello"}],
         "_strip_context_summary_handoff_message": [None]})
case("_assemble_head", self={"_COMPRESSION_NOTE": NOTE}, messages=[SYS], compress_start=1,
     io={"_fresh_compaction_message_copy": exc("SystemExit", "s")})
case("_assemble_head", self={"_COMPRESSION_NOTE": NOTE}, messages=[SYS], compress_start=1)  # fail-closed

# ── _assemble_tail (seams _fresh_compaction_message_copy, _strip_context_summary_handoff_message) ──
_TM = [{"role": "user", "content": "a"}, {"role": "assistant", "content": "b"},
       {"role": "user", "content": "c"}]
case("_assemble_tail", messages=_TM, compress_end=1, tail_start=0, summary_indices=[],
     io={"_fresh_compaction_message_copy": [c for c in _TM[1:]],
         "_strip_context_summary_handoff_message": [c for c in _TM[1:]]})
case("_assemble_tail", messages=_TM, compress_end=0, tail_start=0, summary_indices=[1],
     io={"_fresh_compaction_message_copy": [_TM[0], _TM[2]],
         "_strip_context_summary_handoff_message": [_TM[0], _TM[2]]})
case("_assemble_tail", messages=_TM, compress_end=0, tail_start=2, summary_indices=[1, 2],
     io={"_fresh_compaction_message_copy": [_TM[0]],
         "_strip_context_summary_handoff_message": [_TM[0]]})
case("_assemble_tail", messages=[], compress_end=0, tail_start=0, summary_indices=[],
     io={"_fresh_compaction_message_copy": [], "_strip_context_summary_handoff_message": []})
case("_assemble_tail", messages=_TM, compress_end=0, tail_start=0, summary_indices=[],
     io={"_fresh_compaction_message_copy": [c for c in _TM],
         "_strip_context_summary_handoff_message": [None, _TM[1], None]})
case("_assemble_tail", messages=_TM, compress_end=0, tail_start=0, summary_indices=[],
     io={"_fresh_compaction_message_copy": exc("CancelledError", "c")})
case("_assemble_tail", messages=_TM, compress_end=0, tail_start=0, summary_indices=[])  # fail-closed

# ── _dedupe_tool_results (seam md5hex; mutates result) ────────────────────
LONG_A = "A" * 250
LONG_B = "B" * 250
LONG_C = "C" * 250
case("_dedupe_tool_results", result=[{"role": "tool", "content": LONG_A}],
     io={"md5hex": ["aaaaaaaaaaaaaaaa"]})
case("_dedupe_tool_results", result=[{"role": "tool", "content": LONG_A},
                                     {"role": "tool", "content": LONG_A}],
     io={"md5hex": ["cccccccccccc", "cccccccccccc"]})
case("_dedupe_tool_results", result=[{"role": "tool", "content": LONG_A},
                                     {"role": "tool", "content": LONG_B},
                                     {"role": "tool", "content": LONG_C}],
     io={"md5hex": ["111111111111", "222222222222", "333333333333"]})
case("_dedupe_tool_results", result=[{"role": "tool", "content": "short"},
                                     {"role": "user", "content": LONG_A}],
     io={"md5hex": []})
case("_dedupe_tool_results", result=[{"role": "tool", "content": LONG_A}],
     io={"md5hex": exc("KeyboardInterrupt", "k")})
case("_dedupe_tool_results", result=[], io={"md5hex": []})
case("_dedupe_tool_results", result=[{"role": "tool", "content": LONG_A}])  # fail-closed

# ── _augment_summary_lean (seams _build_anchor_index, _build_verbatim_user_section,
#                            _build_recovery_footer, _redact_compaction_text) ──
def lean(**over):
    s = {"tail_mode": "lean", "_session_id": "sid"}
    s.update(over)
    return s


_turns = [{"role": "user", "content": "hi"}]
case("_augment_summary_lean", self=lean(), summary="BASE", turns_to_summarize=_turns,
     io={"_build_anchor_index": "[anchors]", "_redact_compaction_text": ["[anchors redacted]", "[users redacted]"],
         "_build_verbatim_user_section": "[users]", "_build_recovery_footer": "[footer]"})
case("_augment_summary_lean", self=lean(), summary="BASE\n" + ANCHOR + " present",
     turns_to_summarize=_turns,
     io={"_build_verbatim_user_section": "[users]", "_redact_compaction_text": ["[users redacted]"],
         "_build_recovery_footer": "[footer]"})
case("_augment_summary_lean", self=lean(tail_mode="legacy"), summary="BASE", turns_to_summarize=_turns,
     io={})
case("_augment_summary_lean", self=lean(), summary="BASE", turns_to_summarize=[],
     io={"_build_anchor_index": "", "_redact_compaction_text": ["", ""],
         "_build_verbatim_user_section": "", "_build_recovery_footer": ""})
case("_augment_summary_lean", self=lean(), summary="", turns_to_summarize=_turns,
     io={"_build_anchor_index": "[a]", "_redact_compaction_text": ["[a]", "[u]"],
         "_build_verbatim_user_section": "[u]", "_build_recovery_footer": exc("BaseException", "b")})
case("_augment_summary_lean", self=lean(), summary="BASE", turns_to_summarize=_turns)  # fail-closed

# ── _merge_summary_into_tail_row (seams _append_text_to_content, drop_stale_api_content;
#                                   mutates msg) ──
def mself(**over):
    s = {"_summary_has_user_turn": False}
    s.update(over)
    return s


case("_merge_summary_into_tail_row", self=mself(), msg={"role": "user", "content": "old"},
     summary="SUM", summary_role="user", force_user_leading=True,
     io={"_append_text_to_content": ["old\n\nSUM\n\n" + END + "\n\n"],
         "drop_stale_api_content": None})
case("_merge_summary_into_tail_row", self=mself(_summary_has_user_turn=True),
     msg={"role": "user", "content": "old"}, summary="SUM", summary_role="assistant",
     force_user_leading=False,
     io={"_append_text_to_content": ["old\n\n" + DELIM + "\n\nSUM\n\n" + END,
                                     HDR + "\nold\n\n" + DELIM + "\n\nSUM\n\n" + END],
         "drop_stale_api_content": None})
case("_merge_summary_into_tail_row", self=mself(), msg={}, summary="S", summary_role="user",
     force_user_leading=True,
     io={"_append_text_to_content": ["\n\nS\n\n" + END + "\n\n"],
         "drop_stale_api_content": None})
case("_merge_summary_into_tail_row", self=mself(), msg={"role": "user", "content": "old"},
     summary="S", summary_role="user", force_user_leading=False,
     io={"_append_text_to_content": exc("ValueError", "v")})
case("_merge_summary_into_tail_row", self=mself(), msg={"role": "user", "content": "old"},
     summary="S", summary_role="user", force_user_leading=True,
     io={"_append_text_to_content": ["new content"],
         "drop_stale_api_content": exc("KeyboardInterrupt", "k")})
case("_merge_summary_into_tail_row", self=mself(), msg={"role": "user", "content": "old"},
     summary="S", summary_role="user", force_user_leading=True)  # fail-closed

OUT.parent.mkdir(parents=True, exist_ok=True)
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for c in cases:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")

cnt = Counter(c["func"] for c in cases)
assert len(cnt) == 18, cnt
assert all(cnt[f] >= 6 for f in cnt), cnt
print("wrote %d cases across %d functions -> %s" % (len(cases), len(cnt), OUT))
print("per-function counts:", dict(sorted(cnt.items())))
