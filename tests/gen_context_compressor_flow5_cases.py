#!/usr/bin/env python3
"""Generate tests/fixtures/context_compressor_flow5.jsonl (>=6 cases/function).

Each case is {"func": <live_name>, "in": {...}}; in.io holds the universal-seam
scripts. Both the C harness and the LIVE oracle consume the same file.

FLOW5 ports 18 UNCLAIMED module functions / methods (remainder beyond flow4):
  _is_image_part, _content_has_images, _replace_image_parts,
  _strip_images_from_content, _image_part_label, _str_arg,
  _tool_content_has_images, _strip_images_from_tool_msg,
  _filter_legacy_clarify_answers, _memory_provider_section,
  resolve_model_threshold, _compact_fallback_turn,
  is_user_originated_turn, fallback_reason, _effective_threshold_cap,
  _tripped, _tail_soft_ceiling, _render_tool_args_for_summary.
"""
import json
from collections import Counter
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "context_compressor_flow5.jsonl"
cases = []


def case(func, **inp):
    cases.append({"func": func, "in": inp})


def exc(kind, msg="boom"):
    return {"__raise__": kind, "message": msg}


def script(ret):
    return {"__script__": True, "ret": ret}


IMAGE_STRIP = "[Attached image — stripped after compression]"

# ── _is_image_part (no seams) ─────────────────────────────────────────────
case("_is_image_part", part={"type": "image_url"})
case("_is_image_part", part={"type": "input_image"})
case("_is_image_part", part={"type": "image"})
case("_is_image_part", part={"type": "text", "text": "x"})
case("_is_image_part", part="not a dict")
case("_is_image_part", part={})
case("_is_image_part", part={"type": 5})

# ── _content_has_images (no seams; composed directly) ────────────────────
case("_content_has_images", content=["hi", {"type": "image_url"}])
case("_content_has_images", content=["hi"])
case("_content_has_images", content="str")
case("_content_has_images", content=[])
case("_content_has_images", content=[{"type": "text", "text": "x"}])
case("_content_has_images", content=[{"type": "input_image"}, {"type": "image"}])

# ── _replace_image_parts (no seams) ──────────────────────────────────────
case("_replace_image_parts", parts=["a", "b"], placeholder="[P]")
case("_replace_image_parts", parts="str", placeholder="[P]")
case("_replace_image_parts", parts=[], placeholder="[P]")
case("_replace_image_parts", parts=[{"type": "text", "text": "a"},
                                    {"type": "image_url", "image_url": {"url": "x"}}],
     placeholder="[P]")
case("_replace_image_parts", parts=[{"type": "image"}], placeholder="[P]")
case("_replace_image_parts", parts=[{"type": "input_image"}, {"type": "text", "text": "keep"}],
     placeholder="[P]")

# ── _strip_images_from_content (no seams) ────────────────────────────────
case("_strip_images_from_content", content="plain")
case("_strip_images_from_content", content=[{"type": "text", "text": "a"}])
case("_strip_images_from_content", content=None)
case("_strip_images_from_content", content=[])
case("_strip_images_from_content", content=[{"type": "image"}])
case("_strip_images_from_content", content=[{"type": "text", "text": "a"},
                                            {"type": "image_url"}])

# ── _image_part_label (no seams) ─────────────────────────────────────────
case("_image_part_label", part={"image_url": {"url": "https://x/img.png"}})
case("_image_part_label", part={"image_url": {"url": "http://x"}})
case("_image_part_label", part={"image_url": {"url": "data:image/png;base64,AA"}})
case("_image_part_label", part={"image_url": "https://direct"})
case("_image_part_label", part={"url": "https://alt"})
case("_image_part_label", part={})
case("_image_part_label", part={"image_url": {"url": 123}})

# ── _str_arg (no seams) ──────────────────────────────────────────────────
case("_str_arg", args={"command": "ls"}, key="command")
case("_str_arg", args={}, key="command")
case("_str_arg", args={"command": 5}, key="command")
case("_str_arg", args={"command": None}, key="command")
case("_str_arg", args={"command": True}, key="command")
case("_str_arg", args={"other": "x"}, key="command", default="def")
case("_str_arg", args={"command": "x"}, key="command", default="def")

# ── _tool_content_has_images (seam _tool_result_parts) ───────────────────
case("_tool_content_has_images", content={"_multimodal": True, "content": [{"type": "image_url"}]},
     io={"_tool_result_parts": [{"type": "image_url"}]})
case("_tool_content_has_images", content="x", io={"_tool_result_parts": ["text"]})
case("_tool_content_has_images", content="x", io={"_tool_result_parts": None})
case("_tool_content_has_images", content="x", io={"_tool_result_parts": exc("SystemExit", "bye")})
case("_tool_content_has_images", content="x", io={"_tool_result_parts":
     script([{"type": "text", "text": "a"}, {"type": "image"}])})
case("_tool_content_has_images", content="x")  # fail-closed

# ── _strip_images_from_tool_msg (seam _rewritten) ────────────────────────
case("_strip_images_from_tool_msg", msg={"content": "plain"})
case("_strip_images_from_tool_msg", msg={"content": [{"type": "text", "text": "a"}]})
case("_strip_images_from_tool_msg",
     msg={"content": [{"type": "text", "text": "a"}, {"type": "image_url"}]},
     io={"_rewritten": {"role": "tool", "content": IMAGE_STRIP}})
case("_strip_images_from_tool_msg",
     msg={"content": {"_multimodal": True, "text_summary": "a screenshot"}},
     io={"_rewritten": {"role": "tool", "content": "[screenshot removed] a screenshot"}})
case("_strip_images_from_tool_msg",
     msg={"content": {"_multimodal": True}},
     io={"_rewritten": {"role": "tool", "content": "[screenshot removed] " + IMAGE_STRIP}})
case("_strip_images_from_tool_msg", msg={"content": [{"type": "image"}]},
     io={"_rewritten": exc("PermissionError", "denied")})

# ── _filter_legacy_clarify_answers (seams _clarify_headless_fullmatch, _is_clarify_non_response) ──
case("_filter_legacy_clarify_answers", values=[],
     io={"_clarify_headless_fullmatch": False, "_is_clarify_non_response": False})
case("_filter_legacy_clarify_answers", values=["yes", "no"],
     io={"_clarify_headless_fullmatch": False, "_is_clarify_non_response": False})
case("_filter_legacy_clarify_answers",
     values=["[oneshot mode: no user available. waiting", "answer"],
     io={"_clarify_headless_fullmatch": [True, False], "_is_clarify_non_response": False})
case("_filter_legacy_clarify_answers",
     values=["[single-query mode: no user available to answer q", ", rest", "answer"],
     io={"_clarify_headless_fullmatch": [False, True, True], "_is_clarify_non_response": False})
case("_filter_legacy_clarify_answers", values=["notice", "keep"],
     io={"_clarify_headless_fullmatch": False, "_is_clarify_non_response": [True, False]})
case("_filter_legacy_clarify_answers", values=["x"],
     io={"_clarify_headless_fullmatch": False, "_is_clarify_non_response": exc("CancelledError", "c")})
case("_filter_legacy_clarify_answers", values=["a"])  # fail-closed

# ── _memory_provider_section (seams _sanitize_memory_context, _json_dumps) ──
case("_memory_provider_section", memory_context="ctx",
     io={"_sanitize_memory_context": None, "_json_dumps": '"unused"'})
case("_memory_provider_section", memory_context="ctx",
     io={"_sanitize_memory_context": {"k": "v"}, "_json_dumps": '{"k": "v"}'})
case("_memory_provider_section", memory_context="ctx",
     io={"_sanitize_memory_context": "a&b<c>d", "_json_dumps": '"a&b<c>d"'})
case("_memory_provider_section", memory_context="ctx",
     io={"_sanitize_memory_context": exc("SystemExit", "x")})
case("_memory_provider_section", memory_context="ctx",
     io={"_sanitize_memory_context": {"k": "v"}, "_json_dumps": exc("ValueError", "bad")})
case("_memory_provider_section", memory_context="ctx")  # fail-closed

# ── resolve_model_threshold (seam _model_threshold_key_rank) ─────────────
case("resolve_model_threshold", model="m", model_thresholds={}, default=0.5)
case("resolve_model_threshold", model="", model_thresholds={"a": 0.7}, default=0.5)
case("resolve_model_threshold", model="gpt-astra", model_thresholds={"astra": 0.7},
     default=0.5, io={"_model_threshold_key_rank": script([4, 0])})
case("resolve_model_threshold", model="x", model_thresholds={"a": 0.7, "b": 0.9},
     default=0.5, io={"_model_threshold_key_rank": script([[1, 0], [2, 0]])})
case("resolve_model_threshold", model="x", model_thresholds={"a": 0.7, "b": 0.9},
     default=0.5, io={"_model_threshold_key_rank": script([[1, 0], [1, 0]])})
case("resolve_model_threshold", model="x", provider=" CODEX ",
     model_thresholds={"codex:a": 0.8}, default=0.5,
     io={"_model_threshold_key_rank": script([1, 1])})
case("resolve_model_threshold", model="x", model_thresholds={"a": 0.7},
     default=0.5, io={"_model_threshold_key_rank": None})
case("resolve_model_threshold", model="x", model_thresholds={"a": 0.7},
     default=0.5, io={"_model_threshold_key_rank": exc("RuntimeError", "r")})

# ── _compact_fallback_turn (six ordered seams) ───────────────────────────
case("_compact_fallback_turn", value="x",
     io={"_content_text_for_contains": " raw ", "_redact_compaction_text": " redacted ",
         "_gh_token_sub1": "gh_abc12345", "_ws_collapse": "  a  b ",
         "_elide": "  a  b ", "_gh_token_sub2": "final"})
case("_compact_fallback_turn", value="x",
     io={"_content_text_for_contains": exc("KeyboardInterrupt", "k")})
case("_compact_fallback_turn", value="x",
     io={"_content_text_for_contains": "raw", "_redact_compaction_text": exc("ValueError", "v")})
case("_compact_fallback_turn", value="x",
     io={"_content_text_for_contains": "raw", "_redact_compaction_text": "r",
         "_gh_token_sub1": "g", "_ws_collapse": "w", "_elide": exc("ZeroDivisionError", "z")})
case("_compact_fallback_turn", value="x",
     io={"_content_text_for_contains": "raw", "_redact_compaction_text": "r",
         "_gh_token_sub1": "g", "_ws_collapse": "w", "_elide": "e",
         "_gh_token_sub2": exc("RuntimeError", "last")})
case("_compact_fallback_turn", value="x")  # fail-closed

# ── is_user_originated_turn (seam _user_originated_turn_view) ────────────
case("is_user_originated_turn", message={"role": "user"},
     io={"_user_originated_turn_view": {"role": "user"}})
case("is_user_originated_turn", message={"role": "user"},
     io={"_user_originated_turn_view": None})
case("is_user_originated_turn", message="x", io={"_user_originated_turn_view": "view"})
case("is_user_originated_turn", message=None, io={"_user_originated_turn_view": {}})
case("is_user_originated_turn", message={"role": "user"},
     io={"_user_originated_turn_view": exc("KeyboardInterrupt", "k")})
case("is_user_originated_turn", message={"role": "user"})  # fail-closed

# ── fallback_reason (no seams; self booleans) ────────────────────────────
# `_fallback_reason` reasons tuple reads EVERY flag eagerly — self must carry all seven.
def fr(**flags):
    s = {k: False for k in ("json_decode", "truncated", "empty_content", "overloaded",
                            "model_not_found", "streaming_closed", "timeout")}
    s.update(flags)
    return s


case("fallback_reason", self=fr(json_decode=True, truncated=True))
case("fallback_reason", self=fr(truncated=True))
case("fallback_reason", self=fr(empty_content=True))
case("fallback_reason", self=fr(overloaded=True))
case("fallback_reason", self=fr(model_not_found=True))
case("fallback_reason", self=fr(streaming_closed=True))
case("fallback_reason", self=fr(timeout=True))
case("fallback_reason", self=fr())

# ── _effective_threshold_cap (no seams) ──────────────────────────────────
case("_effective_threshold_cap", self={"threshold_tokens_cap": 5000}, context_length=100000)
case("_effective_threshold_cap", self={"threshold_tokens_cap": 200000}, context_length=100000)
case("_effective_threshold_cap", self={"threshold_tokens_cap": None}, context_length=100000)
case("_effective_threshold_cap", self={"threshold_tokens_cap": 0}, context_length=100000)
case("_effective_threshold_cap", self={"threshold_tokens_cap": -5}, context_length=100000)
case("_effective_threshold_cap", self={"threshold_tokens_cap": 30000}, context_length=20000)

# ── _tripped (no seams) ──────────────────────────────────────────────────
case("_tripped", self={"_ineffective_compression_count": 0})
case("_tripped", self={"_ineffective_compression_count": 1})
case("_tripped", self={"_ineffective_compression_count": 2})
case("_tripped", self={"_ineffective_compression_count": 3})
case("_tripped", self={"_ineffective_compression_count": -1})
case("_tripped", self={"_ineffective_compression_count": 5})

# ── _tail_soft_ceiling (no seams) ────────────────────────────────────────
case("_tail_soft_ceiling", token_budget=1000, self={"context_length": 10000})
case("_tail_soft_ceiling", token_budget=1000, self={"context_length": 5000})
case("_tail_soft_ceiling", token_budget=1000, self={})
case("_tail_soft_ceiling", token_budget=100, self={"context_length": 100000})
case("_tail_soft_ceiling", token_budget=100, self={"context_length": 100})
case("_tail_soft_ceiling", token_budget=0, self={"context_length": 1000})

# ── _render_tool_args_for_summary (seam _redact_compaction_text) ─────────
_RTAS = {"_TOOL_ARGS_HEAD": 1200, "_TOOL_ARGS_MAX": 1500}
case("_render_tool_args_for_summary", self=_RTAS, arguments="short",
     io={"_redact_compaction_text": "short"})
case("_render_tool_args_for_summary", self=_RTAS, arguments=None,
     io={"_redact_compaction_text": ""})
case("_render_tool_args_for_summary", self=_RTAS, arguments="A" * 1600,
     io={"_redact_compaction_text": "A" * 1600})
case("_render_tool_args_for_summary", self=_RTAS, arguments="B" * 1400,
     io={"_redact_compaction_text": "B" * 1400})
case("_render_tool_args_for_summary", self=_RTAS, arguments="x",
     io={"_redact_compaction_text": exc("ValueError", "v")})
case("_render_tool_args_for_summary", self=_RTAS, arguments="x")  # fail-closed

OUT.parent.mkdir(parents=True, exist_ok=True)
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for c in cases:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")

cnt = Counter(c["func"] for c in cases)
assert len(cnt) == 18, cnt
assert all(cnt[f] >= 6 for f in cnt), cnt
print("wrote %d cases across %d functions -> %s" % (len(cases), len(cnt), OUT))
print("per-function counts:", dict(sorted(cnt.items())))
